#define FUSE_USE_VERSION 30
#include <unistd.h>
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
char *disk_path;

// static unsigned long get_max_inode_num(const char* path){
//     unsigned long max_inode_num = 0;

//     char *start = (char *)mapped + sizeof(struct wfs_sb);
//     while(start < (char *)mapped + ((struct wfs_sb *)mapped)->head){
//         struct wfs_log_entry *entry  = (struct wfs_log_entry *)start; 

//         if(entry->inode.inode_number > max_inode_num){
//             max_inode_num = entry->inode.inode_number;
//         }

//         start += sizeof(struct wfs_inode) + entry->inode.size;
//     }


//     return max_inode_num;
// }

static char *get_filename(const char* path) {
    if(strcmp(path, "") == 0) {
        printf("Returning blank\n");
        char *re = "";
        return re;
    }

    char copy[1000];
    strcpy(copy, path);

    char *token = (strtok(copy, "/"));
    char new_filename[MAX_FILE_NAME_LEN];

    // Parse through given path to get the leaf node filename
    while(token) {
        char prevToken[300];
        strcpy(prevToken, token);

        token = strtok(NULL, "/");

        // Reason we check for this before strcat call on parentPath is to check if the prevToken is 
        // the filename of the new inode. If it is, we don't want to include it in the parentPath
        // and so just break
        if(token == NULL) {
            strcpy(new_filename, prevToken);
            break;
        }
    }

    char *ret = malloc(MAX_FILE_NAME_LEN);
    strcpy(ret, new_filename);
    return ret;
}

static struct wfs_inode *get_inode(const char* filename) {
    char *start = (char*)mapped + sizeof(struct wfs_sb);

    while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head) {
        struct wfs_log_entry *curr = (struct wfs_log_entry *)start;

        if(curr->inode.deleted == 0 && S_ISDIR(curr->inode.mode)){

            struct wfs_dentry *current_dentry = (struct wfs_dentry *)(curr + sizeof(struct wfs_inode));

            while((char*)current_dentry < (char*)current_dentry + curr->inode.size) {
                if(strcmp(filename, "") == 0) {
                    if(curr->inode.inode_number == 0) {
                        return &curr->inode;
                    }
                }
                
                if(strcmp(current_dentry->name, filename) == 0) {
                    return &curr->inode;
                }
                current_dentry += sizeof(struct wfs_dentry);
            }
                
        }
        printf("Go to next log entry\n");
        // Go to the Next Log Entry
        start += sizeof(struct wfs_inode) + curr->inode.size; 
    }

    return NULL;
}

// static unsigned long getInodeNumHelper(const char* filename, unsigned long inode_num) {
//     char *start = (char*)mapped + sizeof(struct wfs_sb);
//     struct wfs_log_entry *latest = (struct wfs_log_entry *)start;
//     while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head) {
//         struct wfs_log_entry *curr = (struct wfs_log_entry *)start;

//         if(curr->inode.deleted == 0 && S_ISDIR(curr->inode.mode) && curr->inode.inode_number == inode_num){
//                 latest = curr;
//         }
//         printf("Go to next log entry\n");
//         // Go to the Next Log Entry
//         start += sizeof(struct wfs_inode) + curr->inode.size; 
//     }
//     struct wfs_dentry *curr = latest + sizeof(struct wfs_inode);

//     while(curr < (char*)curr + latest->inode.size) {
//         if(strcmp(curr->name, filename) == 0) {
//             return curr->inode_number;
//         }

//         int ret = getInodeNumHelper(filename, curr->inode_number);
//         if(ret > 0) {
//             return ret;
//         }
//     }

//     return 0;


   
    

// }

// // Find the inode number associated with path
// static unsigned long get_inode_num(const char* path){
    
//     // Inode number starts at root 
//     unsigned long inode = 0; 

//     char copy[1000];
//     strcpy(copy, path);

//     char *ptr;
//     char *token = (strtok_r(copy, "/", &ptr));

//     // Traverse through the path itself
//     int found = 1;
//     while(token){
//         found = 0;
//         // Start after the superblock to be at the first log entry 
//         char *start = (char*)mapped + sizeof(struct wfs_sb);

//         //The latest log entry
//         struct wfs_log_entry *latest = (struct wfs_log_entry *)start;
        
//         printf("Iterating through disk now\n");
//         printf("%p\n", start);
//         printf("%p\n", (char*)mapped + ((struct wfs_sb *)mapped)->head);
//         // iterate through until you have reached the end of the disk
//         while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head){
//             struct wfs_log_entry *curr = (struct wfs_log_entry *)start; 
//             printf("%d\n", curr->inode.inode_number);

//             printf("Checking current entry\n");
//             // Check if entry exists, is a directory, and see if inode number found
//             if(curr->inode.deleted == 0 && S_ISDIR(curr->inode.mode) && curr->inode.inode_number == inode){
//                 latest = curr;
//                 inode = latest->inode.inode_number;
//             }
//             printf("Go to next log entry\n");
//             // Go to the Next Log Entry
//             start += sizeof(struct wfs_inode) + curr->inode.size ;
//         }

//         printf("Examine data\n");
//         // Look at the data associated with log entry
//         struct wfs_dentry *entry = (struct wfs_dentry *)latest->data;
//         int offset = 0;

//         while(offset < latest->inode.size){
//             // Find the inode number 
//             if(strcmp(token, entry->name) == 0){
//                 printf("inode found\n");
//                 found = 1;
//                 inode = entry->inode_number;
//                 break;
//             }

//             // printf("next dentry\n");
//             offset += sizeof(struct wfs_dentry);
//             entry++; 
//         }

//         token = strtok_r(NULL, "/", &ptr);
//     }


//     if(found == 0){
//         return -1;
//     }

//     return inode;

// }

// // Find the inode with the associated number
// static struct wfs_inode *get_inode(unsigned long inode_num){

//     char *start = (char *)mapped + sizeof(struct wfs_sb);
//     struct wfs_inode *latest = NULL;

//      while(start < (char*)mapped + ((struct wfs_sb *)mapped)->head){
//         struct wfs_log_entry *curr = (struct wfs_log_entry*)start; 

//         // Check if entry exists, is a directory, and see if inode number found
//         if(curr->inode.deleted == 0 && curr->inode.inode_number == inode_num){
//             latest = &(curr -> inode);
//         }

//         // Go to the Next Log Entry
//         start += sizeof(struct wfs_inode) + curr->inode.size;
//     }

//     return latest;

// }



static int wfs_getattr(const char* path, struct stat* stbuf){

    struct wfs_inode *inode = get_inode(get_filename(path));

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
    printf("Getting inode\n");
    struct wfs_inode *i = get_inode(get_filename(path));
    printf("Got inode\n");

    if(i != NULL){
        return -EEXIST;
    }

    struct wfs_inode *inode = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));

    inode->inode_number = nextInodeNum++;
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
    memcpy(new_log, inode, sizeof(*inode));

    // ------- Update log with new updated log entry of the parent directory of this new inode -----------
    char *parentPath = malloc(1000);
    strcat(parentPath, "");

    char *copy = malloc(1000);
    strcpy(copy, path);

    char *token = (strtok(copy, "/"));
    char *new_filename = malloc(MAX_FILE_NAME_LEN);

    printf("Parsing parent path\n");
    // Parse through given path to construct the path of the parent directory and also get the new inode filename
    while(token) {
        char *prevToken = malloc(300);
        strcpy(prevToken, token);

        token = strtok(NULL, "/");

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
    printf("Done parsing parent path\n");

    // Get the log entry associated with the parent directory
    printf("Parent path: %s\n", parentPath);
    printf("get_filename result:%s\n", get_filename(parentPath));
    struct wfs_inode *parent_inode = get_inode(get_filename(parentPath));
    if(parent_inode == NULL) {
        printf("NULL\n");
    }
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;

    printf("Make new dentry\n");
    // Make new dentry corresponding to the new inode, and add it to the parent dir log entry
    struct wfs_dentry *new_dentry = malloc(sizeof(struct wfs_dentry));
    strcpy(new_dentry->name, new_filename);
    printf("String copied\n");
    new_dentry->inode_number = parent_inode->inode_number;

    printf("Copying new dentry into new parent log entry\n");
    // Copy that new dentry into a new parent log entry
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*log_entry) + sizeof(struct wfs_dentry));
    void *parent_log_head = (void *)((uintptr_t)new_parent_log + sizeof(*log_entry));
    memcpy(new_parent_log, parent_inode, sizeof(*inode));
    new_parent_log->inode.size += sizeof(*new_dentry);
    new_parent_log->inode.ctime = time(NULL);
    new_parent_log->inode.mtime = time(NULL);
    memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));

    printf("Copy parent log entry into log\n");
    // Copy new update parent log entry into the log
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)((uintptr_t)(sb->head) + disk_path), new_parent_log, sizeof(*new_parent_log));
    sb->head += sizeof(*new_parent_log);

    printf("Copying new log entry\n");
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

    
    struct wfs_inode *inode = get_inode(get_filename(path));
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;
    // char *data = &(log_entry->data);
    size_t numBytes = size;

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
    

    struct wfs_inode *inode = get_inode(get_filename(path));

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
    printf("Entering readdir\n");

    struct wfs_log_entry *dir_to_read = (struct wfs_log_entry *)get_inode(get_filename(path));
    printf("%d\n inode number\n", dir_to_read->inode.inode_number);

    // Check that it is a dir
    if(dir_to_read->inode.mode == __S_IFREG) {
        printf("Cannot readdir on type regular file\n");
        return -ENOTDIR;
    }

    struct wfs_dentry *entry = (struct wfs_dentry *)dir_to_read->data;

    printf("%p\n", (void *)entry);
    printf("%p\n", (char *)(&dir_to_read->data) + dir_to_read->inode.size);
    // While there is still an entry to read, fill buffer with entry name
    while((char *)entry < (char *)(&dir_to_read->data) + dir_to_read->inode.size) {
        
        off_t off = ((char *)(entry + sizeof(struct wfs_dentry)) < (char *)(&dir_to_read->data) + dir_to_read->inode.size) ? (off_t)(entry + sizeof(struct wfs_dentry)) : 0;
        if(!filler(buf, entry->name, NULL, off)) {
            break;
        }
        entry += sizeof(struct wfs_dentry);
    }

    return 0;
}

static int wfs_unlink(const char* path){


    struct wfs_log_entry *entry = (struct wfs_log_entry *)get_inode(get_filename(path));

    if(entry == NULL){
        return -ENOENT;
    }
    
    // entry is now deleted
    entry ->inode.deleted = 1;

    // ------- Update log with new updated log entry of the parent directory of this new inode -----------
    char parentPath[1000];

    char copy[1000];
    strcpy(copy, path);

    
    char *token = (strtok(copy, "/"));
    char new_filename[MAX_FILE_NAME_LEN];

    // Parse through given path to construct the path of the parent directory and also get the new inode filename
    while(token) {
        char prevToken[300];
        strcpy(prevToken, token);

        token = strtok(NULL, "/");

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
    struct wfs_inode *inode = get_inode(get_filename(path));
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*log_entry) + sizeof(struct wfs_dentry));


    // Copy all the direntries from the previous inode that aren't the deleted 
    // file
    char *start = (char *)log_entry->data;
    while(start != NULL){
        struct wfs_dentry *latest = (struct wfs_dentry *)start;
        if(strcmp(latest->name, path)){
            struct wfs_dentry *new_dentry = malloc(sizeof(struct wfs_dentry));
            strcpy(new_dentry->name, new_filename);
            new_dentry->inode_number = inode->inode_number;

            // Copy that new dentry into a new parent log entry
            void *parent_log_head = (void *)((uintptr_t)new_parent_log + sizeof(*log_entry));
            memcpy(new_parent_log, inode, sizeof(*inode));
            new_parent_log->inode.size += sizeof(*new_dentry);
            new_parent_log->inode.ctime = time(NULL);
            new_parent_log->inode.mtime = time(NULL);
            memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));

        }
        start += sizeof(struct wfs_dentry);
    }

    // Copy new update parent log entry into the log
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)((uintptr_t)(sb->head) + disk_path), new_parent_log, sizeof(*new_parent_log));
    sb->head += sizeof(*new_parent_log);

    return 0;
}

static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};


int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    struct stat statbuf;
    disk_path = realpath(argv[argc-2], NULL);
    int fd = open(disk_path, O_RDWR);

    if(fd < 0) {
        printf("Uh oh\n");
        printf("%s", strerror(errno));
        return 0;
    }
    if (fstat(fd, &statbuf) < 0) {
        printf ("fstat error\n");
        printf("%s", strerror(errno));
        return 0;
    }
    printf("try to map\n");
    mapped = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("mapped success\n");


    argv[argc - 2] = argv[argc - 1];
    argv[argc - 1] = NULL;
    argc--;
    
    printf("%s\n", argv[argc - 2]);
    printf("%s\n", argv[argc - 1]);
    printf("trying to fuse\n");
    return fuse_main(argc, argv, &ops, NULL);
}