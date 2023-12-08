#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "wfs.h"



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

// static int wfs_mknod(const char* path, mode_t mode, dev_t dev){
//     return 0;
// }

// static int wfs_mkdir(const char* path, mode_t mode){
//     return 0;
// }

// static int wfs_read(const char* path, struct fuse_file_info* fi){
//     return 0; 
// }

// static int wfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
//     return 0;
// }

// static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
//     return 0;
// }

// static int wfs_unlink(const char* path){
//     return 0;
// }

static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    // .mknod      = wfs_mknod,
    // .mkdir      = wfs_mkdir,
    // .read	    = wfs_read,
    // .write      = wfs_write,
    // .readdir	= wfs_readdir,
    // .unlink    	= wfs_unlink,
};


int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main

    argv[argc - 2] = argv[argc - 1];
    argv[argc - 1] = NULL;
    argc--;

    return fuse_main(argc, argv, &ops, NULL);
}