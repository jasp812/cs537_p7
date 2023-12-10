#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "wfs.h"
#include <stdlib.h>
#include <sys/sysmacros.h>

void *mapped;

static unsigned long get_max_inode_num(const char* path){
    unsigned long max_inode_num = 0;

    char *start = (char *)mapped + sizeof(struct wfs_sb);
    while(start < (char *)mapped + ((struct wfs_sb *)mapped)->head){
        struct wfs_log_entry *entry  = (struct wfs_log_entry *)start; 

        if(entry->inode.inode_number > max_inode_num){
            max_inode_num = entry->inode.inode_number;
        }

        start += sizeof(struct wfs_inode) + entry->inode.size;
    }


    return max_inode_num;
}

// Find the inode number associated with path
static unsigned long get_inode_num(const char* path){
    
    // Inode number starts at root 
    unsigned long inode = 0; 

    char copy[MAX_FILE_NAME_LEN];
    strcpy(copy, path);

    char *ptr;
    char *token = (strtok_r(copy, "/", &ptr));

    // Traverse through the path itself
    int found = 1;
    while(token){
        found = 0;
        // Start after the superblock to be at the first log entry 
        char *start = (char*)mapped + sizeof(struct wfs_sb);

        //The latest log entry
        struct wfs_log_entry *latest;
        
        // iterate through until you have reached the end of the disk
        while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head){
            struct wfs_log_entry *curr = (struct wfs_log_entry *)start; 

            // Check if entry exists, is a directory, and see if inode number found
            if(curr->inode.deleted == 0 && S_ISDIR(curr->inode.mode) && curr->inode.inode_number == inode){
                latest = curr;
            }

            // Go to the Next Log Entry
            start += sizeof(struct wfs_inode) + curr->inode.size ;
        }

        // Look at the data associated with log entry
        struct wfs_dentry *entry = (struct wfs_dentry *)latest->data;
        int offset = 0;

        while(offset < latest->inode.size){
            // Find the inode number 
            if(strcmp(token, entry->name) == 0){
                found = 1;
                inode = entry->inode_number;
                break;
            }

            offset += sizeof(struct wfs_dentry);
            entry++; 
        }

        token = strtok_r(NULL, "/", &ptr);
    }


    if(found == 0){
        return -1;
    }

    return inode;

}

// Find the inode with the associated number
static struct wfs_inode *get_inode(unsigned long inode_num){

    char *start = (char *)mapped + sizeof(struct wfs_sb);
    struct wfs_inode *latest = NULL;

     while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head){
        struct wfs_log_entry *curr = (struct wfs_log_entry*)start; 

        // Check if entry exists, is a directory, and see if inode number found
        if(curr->inode.deleted == 0 && curr->inode.inode_number == inode_num){
            latest = &(curr -> inode);
        }

        // Go to the Next Log Entry
        start += sizeof(struct wfs_inode) + curr->inode.size;
    }

    return latest;

}



static int wfs_getattr(const char* path, struct stat* stbuf){

    unsigned long inode_num = get_inode_num(path);

    if (inode_num == -1){
        return  -ENOENT;
    }

    struct wfs_inode *inode = get_inode(inode_num);

    if(inode == NULL){
        return -ENOENT;
    }

    stbuf->st_uid  = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_mtime = inode->mtime;
    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = inode->links;
    stbuf->st_size = inode->size;

    printf("%ld\n", stbuf->st_nlink);
    
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t dev){

    unsigned long inode_num = get_inode_num(path);

    if(inode_num >= 0){
        return -EEXIST;
    }

    struct wfs_inode *inode = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));

    inode->inode_number = get_max_inode_num(path) + 1;
    // Alternative: could use the nextInodeNum global variable initialized in the header file and update it

    inode->deleted = 0;
    inode->mode = mode;
    inode->uid = getuid();
    inode->gid = getgid();
    inode->flags = 0;
    inode->size = 0;
    inode->atime = time(NULL);
    inode->mtime = time(NULL);
    inode->ctime = time(NULL);
    inode->links = 1;

    struct wfs_log_entry *new_log = (struct wfs_log_entry *)malloc(sizeof(struct wfs_inode));
    new_log->inode = *inode;

    // ------- Update log with new updated log entry of the parent directory of this new inode -----------
    char parentPath[1000];

    char copy[1000];
    strcpy(copy, path);

    char *ptr;
    char *token = (strtok_r(copy, "/", &ptr));
    char new_filename[MAX_FILE_NAME_LEN];

    // Parse through given path to construct the path of the parent directory and also get the new inode filename
    while(token) {
        char prevToken[300];
        strcpy(prevToken, token);

        token = strtok_r(NULL, "/", &ptr);

        // Reason we check for this before strcat call on parentPath is to check if the prevToken is 
        // the filename of the new inode. If it is, we don't want to include it in the parentPath
        // and so just break
        if(token == NULL) {
            strcpy(new_filename, prevToken);
            break;
        }

        strcat(parentPath, "/");
        strcat(parentPath, prevToken);
        
    }

    // Get the log entry associated with the parent directory
    int inodeNum = get_inode_num(parentPath);
    struct wfs_inode *inode = get_inode(inodeNum);
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;

    // Make new dentry corresponding to the new inode, and add it to the parent dir log entry
    struct wfs_dentry *new_dentry = malloc(sizeof(struct wfs_dentry));
    strcpy(new_dentry->name, new_filename);
    new_dentry->inode_number = inode->inode_number;

    // Copy that new dentry into a new parent log entry
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*log_entry) + sizeof(struct wfs_dentry));
    void *parent_log_head = (void *)((uintptr_t)new_parent_log + sizeof(*log_entry));
    memcpy(new_parent_log, inode, sizeof(*inode));
    new_parent_log->inode.size += sizeof(*new_dentry);
    new_parent_log->inode.ctime = time(NULL);
    new_parent_log->inode.mtime = time(NULL);
    memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));

    // Copy new update parent log entry into the log
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)((uintptr_t)(sb->head) + disk_path), new_parent_log, sizeof(*new_parent_log));
    sb->head += sizeof(*new_parent_log);

    // Copy new log entry for new inode into log
    memcpy((void *)((uintptr_t)(sb->head)), new_log, sizeof(*new_log));
    sb->head += sizeof(*new_log);


    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode){
    // Check that it is not of mode S_IFREG (regular file)
    if(mode == __S_IFREG) {
        printf("Can't initialize a file with mkdir\n");
        return -ENOTDIR;
    }

    // make inode for the directory
    // mknod will also update the log for us with an new updated parent dir log entry, and a
    // new log entry for the new dir
    int ret = wfs_mknod(path, mode, makedev(0, 0));

    return ret;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){

    unsigned long inodeNum = get_inode_num(path);
    struct wfs_inode *inode = get_inode(inodeNum);
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;
    // char *data = &(log_entry->data);
    size_t numBytes = size;

     if(inodeNum == -1){
        return -ENOENT;
    }


    if(inode == NULL){
        return -ENOENT;
    }

    if(offset >= inode->size) {
        printf("Offset cannot exceed size of data\n");
        return 0;
    }

    if(inode->mode != __S_IFREG) {
        printf("Cannot read from a non-regular file\n");
        return -ENOENT;
    }

    if(size >= inode->size - offset) {
        numBytes = inode->size - offset;
    } 

    memcpy((void *)buf, (void *)(log_entry->data + offset), numBytes);



    return numBytes; 
}

// TODO: change write so that it doesn't check if size or offset exceeds inode's size bounds,
// since that prevents write from being used to append to the end of a file
static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
    
    unsigned long inode_num = get_inode_num(path);

    if(inode_num == -1){
        return -ENOENT;
    }

    struct wfs_inode *inode = get_inode(inode_num);

    if(inode == NULL){
        return -ENOENT;
    }

    if(inode->mode != __S_IFREG){
        return -ENOENT;
    }


    if(offset >= inode -> size){
        return 0;
    }

    size_t max_write = inode->size - offset;
    size_t write_size; 

    if(size < max_write){
        write_size = size;
    }
    else{
        write_size = max_write;
    }

    struct wfs_log_entry *latest = (struct wfs_log_entry *)inode;

    // Create new log entry containing new written data
    struct wfs_log_entry *new_log_entry = (struct wfs_log_entry *)malloc(sizeof(*latest) + write_size);
    void *write_addr = (void *)(new_log_entry->data + offset);
    memcpy(new_log_entry, inode, sizeof(*inode));
    memcpy(write_addr, buf, write_size);
    new_log_entry->inode.size = sizeof(*new_log_entry) - sizeof(struct wfs_inode);
    new_log_entry->inode.mtime = time(NULL);
    new_log_entry->inode.ctime = time(NULL);

    // Update log with this new log entry and update superblock head
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)((uintptr_t)(sb->head) + disk_path), new_log_entry, sizeof(*new_log_entry));
    sb->head += sizeof(*new_log_entry);

    return write_size;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
    int inodeNum = get_inode_num(path);

    // Check that the dir exists
    if(inodeNum == -1) {
        print("Directory does not exist\n");
        return -ENOENT;
    }

    struct wfs_log_entry *dir_to_read = (struct wfs_log_entry *)get_inode(inodeNum);

    // Check that it is a dir
    if(dir_to_read->inode.mode == __S_IFREG) {
        printf("Cannot readdir on type regular file\n");
        return -ENOTDIR;
    }

    struct wfs_dentry *entry = &dir_to_read->data;

    // While there is still an entry to read, fill buffer with entry name
    while((void *)entry < (void *)(&dir_to_read->data) + dir_to_read->inode.size && !filler(buf, entry->name, NULL, entry + sizeof(struct wfs_dentry))) {
        entry += sizeof(struct wfs_dentry);
    }

    return 0;
}

// static int wfs_unlink(const char* path){
//     return 0;
// }

static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    // .unlink    	= wfs_unlink,
};


int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    struct stat statbuf;
    int fd = open(disk_path, O_WRONLY);

    if (fstat(fd, &statbuf) < 0) {
        printf ("fstat error");
        return 0;
    }
    mapped = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    argv[argc - 2] = argv[argc - 1];
    argv[argc - 1] = NULL;
    argc--;

    return fuse_main(argc, argv, &ops, NULL);
}