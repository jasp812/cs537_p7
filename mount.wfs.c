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



static void printLog(struct wfs_log_entry *log) {
    printf("------ Log ---------\n");
    printf("Inode number: %d, ", log->inode.inode_number);
    printf("Mode: %d, ", log->inode.mode);
    printf("Size: %d\n", log->inode.size);


    int cnt = log->inode.size / sizeof(struct wfs_dentry);
    struct wfs_dentry *dentry = (struct wfs_dentry *)log->data;

    for(int i = 0; i < cnt; i++) {
        printf("Data entry name: %s, ", dentry->name);
        printf("Data entry inode num: %ld\n", dentry->inode_number);
        dentry += 1;
    }
}

// Find the latest log entry associated with the inode_num
static struct wfs_log_entry *get_latest_log_entry(unsigned long inode_num){
    char *start = (char*)mapped + sizeof(struct wfs_sb);
    uint32_t head_off = ((struct wfs_sb *)mapped)->head;
    struct wfs_log_entry *latest = NULL;
    while((void*)start < (void *)((uintptr_t)mapped + head_off)) {
        struct wfs_log_entry *curr = (struct wfs_log_entry *)start;
        if (curr->inode.inode_number == inode_num){
            printf("found log_entry with inode number: %d\n", curr->inode.inode_number);
            latest = curr;
        }

        start += sizeof(struct wfs_inode) + curr->inode.size;
    }

    return latest;
}

// Find the log entry associated with the path
static struct wfs_log_entry *get_log_entry_path(const char *path){
    
    printf("path to traverse: %s\n", path);
    // find the latest root log entry
    struct wfs_log_entry *root = get_latest_log_entry(0);
    struct wfs_log_entry *curr = root;

    char copy[1000];
    strcpy(copy, path);

    char *token = strtok(copy,"/");

    int found = 1;
    while(token){
        // printf("Token: %s\n", token);
        // printf("Traversing through the path\n");
        found = 0;
        printLog(curr);
        struct wfs_dentry *entry = (struct wfs_dentry *)curr->data;
        printf("entries: %s\n", curr->data);

        printf("Travering through dentries\n");
        while((char *)entry < (char *)curr->data + curr->inode.size){
            printf("entry names: %s\n", entry->name);
            if(strcmp(entry->name, token) == 0){
                printf("entry found\n");
                found = 1;
                int inode_num  = entry ->inode_number;
                curr = get_latest_log_entry(inode_num);
                break;
                
            }

            printf("Go to next entry\n");
            entry += 1;



            printf("next entry: %ld\n", entry->inode_number);
            printf("next entry: %s\n", entry->name);
        }
        // invalid entry in the path
        if (found == 0 || curr == NULL){
            return NULL;
        }
        
        printf("got to next token\n");
        token = strtok(NULL, "/");

        
    }

    if (found == 0){
        printf("Nothing found\n");
        return NULL;
    }

    printf("Found!!!\n");
    return curr;

}

static int wfs_getattr(const char* path, struct stat* stbuf){

    struct wfs_log_entry *log = get_log_entry_path(path);

    if(log == NULL){
        return -ENOENT;
    }

    struct wfs_inode *inode  = &log->inode;
    memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_uid  = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_mtime = inode->mtime;
    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = inode->links;
    stbuf->st_size = inode->size;
    
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t dev){
    // printf("Getting inode\n");
    // struct wfs_log_entry *log = get_log_entry_path(path);
    // printf("Got inode\n");

    // if(log != NULL){
    //     return -EEXIST;
    // }

    // printf("make a new one!\n");

    // struct wfs_inode *inode = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));

    // inode->inode_number = ++nextInodeNum;
    // // Alternative: could use the nextInodeNum global variable initialized in the header file and update it

    // inode->deleted = 0;
    // inode->mode = mode;
    // inode->uid = getuid();
    // inode->gid = getgid();
    // inode->flags = 0;
    // inode->size = 0;
    // inode->atime = time(NULL);
    // inode->mtime = time(NULL);
    // inode->ctime = time(NULL);
    // inode->links = 1;

    // struct wfs_log_entry *new_log = (struct wfs_log_entry *)malloc(sizeof(struct wfs_inode));
    // new_log->inode = *inode;

    // struct wfs_sb *sb = (struct wfs_sb *)mapped;
    // memcpy((void *)(sb->head + (uintptr_t)mapped), new_log, sizeof(*new_log));
    // sb->head += sizeof(new_log);

    // char parentPath[1000];

    // char copy[1000];
    // strcpy(copy, path);

    
    // char *token = (strtok(copy, "/"));
    // char new_filename[MAX_FILE_NAME_LEN];

    // // Parse through given path to construct the path of the parent directory and also get the new inode filename
    // while(token) {
    //     char prevToken[300];
    //     strcpy(prevToken, token);

    //     token = strtok(NULL, "/");

    //     // Reason we check for this before strcat call on parentPath is to check if the prevToken is 
    //     // the filename of the new inode. If it is, we don't want to include it in the parentPath
    //     // and so just break
    //     if(token == NULL) {
    //         strcpy(new_filename, prevToken);
    //         break;
    //     }

    //     strcat(parentPath, "/");
    //     strcat(parentPath, prevToken);
        
    // }

    // printf("parent path: %s\n", parentPath);
    // // Get the log entry associated with the parent directory
    // struct wfs_log_entry *parent_log = get_log_entry_path(parentPath);
    // if(parent_log == NULL) {
    //     printf("NULL\n");
    // }
    // // struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;

    // printf("Make new dentry\n");
    // // Make new dentry corresponding to the new inode, and add it to the parent dir log entry
    // struct wfs_dentry *new_dentry = malloc(sizeof(struct wfs_dentry));
    // strcpy(new_dentry->name, parentPath);
    // printf("String copied\n");
    // new_dentry->inode_number = inode->inode_number;

    // printf("Copying new dentry into new parent log entry\n");
    // // Copy that new dentry into a new parent log entry
    // struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*log) + sizeof(struct wfs_dentry) + log->inode.size);
    // printf("new log created\n");
    // void *parent_log_head = (void *)((char*)(uintptr_t)new_parent_log + sizeof(struct wfs_log_entry) + parent_log->inode.size);
    // printf("new log head\n");
    // // struct wfs_inode *inodeNew = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));
    // // inodeNew->inode_number = parent_log->inode.inode_number;
    // // inodeNew->deleted = parent_log->inode.deleted;
    // // inodeNew->mode = parent_log->inode.mode;
    // // inodeNew->uid = parent_log->inode.uid;
    // // inodeNew->gid = parent_log->inode.gid;
    // // inodeNew->flags = parent_log->inode.flags;
    // // inodeNew->size = parent_log->inode.size + sizeof(struct wfs_dentry);
    // // inodeNew->atime = time(NULL);
    // // inodeNew->mtime = time(NULL);
    // // inodeNew->ctime = time(NULL);
    // // inodeNew->links = 1;
    // memcpy(new_parent_log, parent_log, sizeof(struct wfs_log_entry) + parent_log->inode.size);
    // printf("memcopy old data to new log\n");
    // new_parent_log->inode.size += sizeof(struct wfs_dentry);
    // printf("update size of new log\n");
    // memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));
    // printf("Parent Log:\n");
    // printLog(new_parent_log);

    // printf("Copy parent log entry into log\n");
    // // Copy new update parent log entry into the log
    // memcpy((void *)(sb->head + (uintptr_t)mapped), new_parent_log, sizeof(*new_parent_log) + new_parent_log->inode.size);
    // printLog((struct wfs_log_entry *)(void *)(sb->head + (uintptr_t)mapped));
    // sb->head += sizeof(*new_parent_log) + new_parent_log->inode.size;
    

    // // printf("Copying new log entry\n");
    // // // Copy new log entry for new inode into log
    // // memcpy((void *)(sb_pointer->head + (uintptr_t)mapped), new_log, sizeof(*new_log) + new_log->inode.size);
    // // printLog((struct wfs_log_entry *)(void *)(sb->head + (uintptr_t)mapped));
    // // sb_pointer->head += sizeof(*new_log) + new_log->inode.size;

    // return 0;

    printf("Getting inode\n");
    struct wfs_log_entry *log = get_log_entry_path(path);
    printf("Got inode\n");

    if(log != NULL){
        return -EEXIST;
    }

    printf("mode: %d\n", mode);

    struct wfs_inode *inode = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));

    inode->inode_number = ++nextInodeNum;
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
    // memcpy(new_log, inode, sizeof(*inode));
    new_log->inode = *inode;
    // printf("New Log:\n");
    // printLog(new_log);
    // ------- Update log with new updated log entry of the parent directory of this new inode -----------
    char *parentPath = calloc(256, 1);
    strcpy(parentPath, "");
    printf("Parent path malloc'd: %s\n", parentPath);

    char copy[strlen(path) + 1];
    strcpy(copy, path);
    printf("copy: %s\n", copy);

    char *token = strtok(copy, "/");

    if(token == NULL) {
        printf("Why is your strtok token null\n");
    }
    char *new_filename = malloc(MAX_FILE_NAME_LEN);

    printf("Parsing parent path\n");
    // Parse through given path to construct the path of the parent directory and also get the new inode filename
    while(token) {
        char *prevToken = malloc(256);
        strcpy(prevToken, token);
        printf("Token: %s\n", token);
        printf("Prev token:%s\n", prevToken);

        token = strtok(NULL, "/");
        printf("Token after while strtok: %s\n", token);

        // Reason we check for this before strcat call on parentPath is to check if the prevToken is 
        // the filename of the new inode. If it is, we don't want to include it in the parentPath
        // and so just break
        if(token == NULL) {
            strcpy(new_filename, prevToken);
            break;
        }

        strcat(parentPath, "/");
        strcat(parentPath, prevToken);
        free(prevToken);
    }
    printf("Done parsing parent path: %s\n", parentPath);

    // Get the log entry associated with the parent directory
    struct wfs_log_entry *parent_log = get_log_entry_path(parentPath);
    if(parent_log == NULL) {
        printf("NULL\n");
    }
    // struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;

    printf("Make new dentry\n");
    // Make new dentry corresponding to the new inode, and add it to the parent dir log entry
    struct wfs_dentry *new_dentry = malloc(sizeof(struct wfs_dentry));

    strcpy(new_dentry->name, new_filename);
    free(new_filename);

    printf("String copied\n");
    new_dentry->inode_number = inode->inode_number;

    printf("Copying new dentry into new parent log entry\n");
    // Copy that new dentry into a new parent log entry
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*parent_log) + sizeof(struct wfs_dentry) + parent_log->inode.size);
    void *parent_log_head = (void *)((char*)(uintptr_t)new_parent_log + sizeof(struct wfs_log_entry) + parent_log->inode.size);
    struct wfs_inode *inodeNew = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));
    inodeNew->inode_number = parent_log->inode.inode_number;
    inodeNew->deleted = parent_log->inode.deleted;
    inodeNew->mode = parent_log->inode.mode;
    inodeNew->uid = parent_log->inode.uid;
    inodeNew->gid = parent_log->inode.gid;
    inodeNew->flags = parent_log->inode.flags;
    inodeNew->size = parent_log->inode.size + sizeof(struct wfs_dentry);
    inodeNew->atime = time(NULL);
    inodeNew->mtime = time(NULL);
    inodeNew->ctime = time(NULL);
    inodeNew->links = 1;
    memcpy(new_parent_log, inodeNew, sizeof(struct wfs_inode));
    memcpy((char*)new_parent_log + sizeof(struct wfs_inode), parent_log->data, parent_log->inode.size);
    printf("Parent Log after the memcpy inode new_parent_log:\n");
    printLog(new_parent_log);
    memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));
    printf("Parent Log after the memcpy int new_parent_log:\n");
    printLog(new_parent_log);

    printf("Copy parent log entry into log\n");
    // Copy new update parent log entry into the log
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)(sb->head + (uintptr_t)mapped), new_parent_log, sizeof(*new_parent_log) + new_parent_log->inode.size);
    // printLog((struct wfs_log_entry *)(void *)(sb->head + (uintptr_t)mapped));
    sb->head += sizeof(*new_parent_log) + new_parent_log->inode.size;
    
    

    printf("Copying new log entry\n");
    // Copy new log entry for new inode into log
    memcpy((void *)(sb->head + (uintptr_t)mapped), new_log, sizeof(*new_log) + new_log->inode.size);
    // printLog((struct wfs_log_entry *)(void *)(sb->head + (uintptr_t)mapped));
    sb->head += sizeof(*new_log) + new_log->inode.size;

    free(new_dentry);
    free(new_log);
    free(new_parent_log);
    free(inodeNew);
    free(parentPath);
    // free(copy);
    free(inode);
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode){
    // Check that it is not of mode S_IFREG (regular file)
    if(mode == __S_IFREG) {
        printf("Can't initialize a file with mkdir\n");
        return -ENOTDIR;
    }

    mode = mode | __S_IFDIR;
    mode = mode & (~__S_IFREG);

    // make inode for the directory
    // mknod will also update the log for us with an new updated parent dir log entry, and a
    // new log entry for the new dir
    int ret = wfs_mknod(path, mode, makedev(0, 0));

    return ret;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){

    printf("In Read\n");

    struct wfs_log_entry *log = get_log_entry_path(path);
    // char *data = &(log_entry->data);
    size_t numBytes = size;

    printf("Error Checks\n");
    if(log == NULL){
        return -ENOENT;
    }

    printf("Checking offset value\n");
    if(offset >= log->inode.size) {
        printf("Offset cannot exceed size of data\n");
        return 0;
    }

    printf("Checking if file or directory\n");
    if(log->inode.mode == __S_IFDIR) {
        printf("Cannot read from a non-regular file\n");
        return -ENOENT;
    }

    printf("Finding number of bytes to read\n");
    if(size > log->inode.size - offset) {
        numBytes = log->inode.size - offset;
    }else{
        numBytes = size;
    } 

    printf("bytes Read: %ld\n", numBytes);

    printf("Copying to buffer\n");
    printf("buffer size %ld\n", strlen(buf));
    memcpy((void *)buf, (void *)(log->data + offset), numBytes);

    printf("Buffer Read: %s\n", buf);

    return numBytes; 
}

// TODO: change write so that it doesn't check if size or offset exceeds inode's size bounds,
// since that prevents write from being used to append to the end of a file
static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
    
    printf("In Write\n");

    struct wfs_log_entry *log =get_log_entry_path(path);

    if(log == NULL){
        return -ENOENT;
    }

    if(log->inode.mode == __S_IFDIR){
        return -ENOENT;
    }

    if (size < 0){
        return 0;
    }
    // if(offset >= log->inode.size){
    //     return 0;
    // }

    // size_t max_write = log->inode.size - offset;
    // size_t write_size; 

    // if(size < max_write){
    //     write_size = size;
    // }
    // else{
    //     write_size = max_write;
    // }

    size_t increased_size = offset + size - log->inode.size;
    if(increased_size <= 0){
        increased_size = 0;
    }

    printf("buffer: %s\n", buf);
    printf("Offset: %ld\n", offset);
    printf("Size: %ld\n", size);
    printf("Inode size: %d\n", log->inode.size);

    printf("Write Size: %ld\n", increased_size);

    printf("getting latest log entry\n");
    // struct wfs_log_entry *latest = (struct wfs_log_entry *)&log->inode;

    // Create new log entry containing new written data
    printf("new written data\n");
    struct wfs_log_entry *new_log_entry = (struct wfs_log_entry *)calloc(1, sizeof(*log) + log->inode.size + increased_size);
    void *write_addr = (void *)(new_log_entry->data + offset);
    memcpy(new_log_entry, &log->inode, sizeof(log->inode));

    // FLAG
    printf("old inode size: %d\n", new_log_entry->inode.size);
    new_log_entry->inode.size = log->inode.size + increased_size; 
    printf("new inode size: %d\n", new_log_entry->inode.size);
    new_log_entry->inode.mtime = time(NULL);
    new_log_entry->inode.ctime = time(NULL);

    memcpy(write_addr, buf, size);
    // printf("print log after being done\n");
    // printLog(new_log_entry);
    printf("entries after write: %s\n", new_log_entry->data);

    // Update log with this new log entry and update superblock head
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)(sb->head + (uintptr_t)mapped), new_log_entry, sizeof(*new_log_entry) + new_log_entry->inode.size);
    sb->head += sizeof(*new_log_entry) + new_log_entry->inode.size;

    free(new_log_entry);

    return size;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
    printf("Entering readdir\n");
    printf("original filename %s\n", path);
    struct wfs_log_entry *dir_to_read = get_log_entry_path(path);
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
        
        // off_t off = (((char*)entry + sizeof(struct wfs_dentry)) < (char *)(&dir_to_read->data) + dir_to_read->inode.size) ? (off_t)((char*)entry + sizeof(struct wfs_dentry)) : 0;
        filler(buf, entry->name, NULL, 0);
        
        entry += 1;
    }

    return 0;
}

static int wfs_unlink(const char* path){


    struct wfs_log_entry *entry = get_log_entry_path(path);

    if(entry == NULL){
        return -ENOENT;
    }

    entry->inode.links--;
    
    if (entry->inode.links > 0){
        return 0;
    }
    // entry is now deleted
    entry ->inode.deleted = 1;

    // ------- Update log with new updated log entry of the parent directory of this new inode -----------
    char *parentPath = calloc(256, 1);
    strcpy(parentPath, "");
    printf("Parent path malloc'd: %s\n", parentPath);

    char copy[strlen(path) + 1];
    strcpy(copy, path);
    printf("copy: %s\n", copy);

    char *token = strtok(copy, "/");

    if(token == NULL) {
        printf("Why is your strtok token null\n");
    }
    char *new_filename = malloc(MAX_FILE_NAME_LEN);

    printf("Parsing parent path\n");
    // Parse through given path to construct the path of the parent directory and also get the new inode filename
    while(token) {
        char *prevToken = malloc(256);
        strcpy(prevToken, token);
        printf("Token: %s\n", token);
        printf("Prev token:%s\n", prevToken);

        token = strtok(NULL, "/");
        printf("Token after while strtok: %s\n", token);

        // Reason we check for this before strcat call on parentPath is to check if the prevToken is 
        // the filename of the new inode. If it is, we don't want to include it in the parentPath
        // and so just break
        if(token == NULL) {
            strcpy(new_filename, prevToken);
            break;
        }

        strcat(parentPath, "/");
        strcat(parentPath, prevToken);
        free(prevToken);
    }
    printf("Done parsing parent path: %s\n", parentPath);
    
    struct wfs_log_entry *entry2 = get_log_entry_path(parentPath);
    // struct wfs_inode *inode = &entry2->inode;
    // struct wfs_log_entry *log_entry = (struct wfs_log_entry *)inode;
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)malloc(sizeof(*entry2) + entry2->inode.size);


    // Copy all the direntries from the previous inode that aren't the deleted 
    // file
    char *start = (char *)entry2->data;
    void *parent_log_head = (void *)((uintptr_t)new_parent_log + sizeof(struct wfs_inode));
    memcpy(new_parent_log, &entry2->inode, sizeof(struct wfs_inode));
    while(start < (char *)entry2->data + entry2->inode.size){
        struct wfs_dentry *latest = (struct wfs_dentry *)start;
        if(strcmp(latest->name, path)){
            struct wfs_dentry *new_dentry = (struct wfs_dentry *)malloc(sizeof(struct wfs_dentry));
            strcpy(new_dentry->name, latest->name);
            new_dentry->inode_number = latest->inode_number;
            // Copy that new dentry into a new parent log entry
            new_parent_log->inode.size += sizeof(*new_dentry);
            new_parent_log->inode.ctime = time(NULL);
            new_parent_log->inode.mtime = time(NULL);
            memcpy(parent_log_head, new_dentry, sizeof(struct wfs_dentry));
            parent_log_head = (char*)parent_log_head + sizeof(struct wfs_dentry);

        }
        start += sizeof(struct wfs_dentry);
    }

    // Copy new update parent log entry into the log
    struct wfs_sb *sb = (struct wfs_sb *)mapped;
    memcpy((void *)(sb->head + (uintptr_t)mapped), new_parent_log, sizeof(*new_parent_log) + new_parent_log->inode.size);
    sb->head += sizeof(*new_parent_log) + new_parent_log->inode.size;

    free(parentPath);

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

    // // wfs_mkdir("/a", __S_IFDIR);
    // wfs_mknod("/data11", __S_IFREG, makedev(0,0));
    // wfs_write("/data11", "Hello", strlen("Hello"), 0, NULL);
    // char buff[100];
    // wfs_read("/data11", buff, strlen("Hello"), 0, NULL);
    
    int ret = fuse_main(argc, argv, &ops, NULL);
    
    munmap(mapped, statbuf.st_size);

    return ret;
}