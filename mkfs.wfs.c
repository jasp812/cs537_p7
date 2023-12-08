#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "wfs.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int init_filesystem(char *disk_path) {
    int fd;
    void *mapped;
    struct stat statbuf;
 
    // opening the file in write mode
    fd = open(disk_path, O_RDWR);
 
    // checking if the file is opened successfully
    if (fd < 0) {
        printf("Error occured while opening file\n.");
        exit(0);
    }

    // find size of input file
    if (fstat(fd, &statbuf) < 0) {
        printf ("fstat error");
        return 0;
    }

    // Map mem, this is where we are going to put our superblock and all log entries
    mapped = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("Mapped memory: %p\n", mapped);
    printf("Potential error: %s\n", strerror(errno));

    // Initialize superblock
    struct wfs_sb *sb = (struct wfs_sb *)malloc(sizeof(struct wfs_sb));
    printf("Init superblock\n");

    if (sb == NULL) {
        printf("Error allocating memory for superblock");
        exit(1);
    }

    // Set the magic number and head
    sb->magic = WFS_MAGIC;
    sb->head = sizeof(struct wfs_sb);
    printf("Set magic number and head\n");

    // Copy superblock into mapped memory and then free the malloc ptr
    memcpy((void*)mapped, (void*)sb, sizeof(sb));
    free(sb);
    printf("Copying in superblock\n");

    // Initialize root inode
    struct wfs_inode *inode = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));
    inode->inode_number = nextInodeNum++;
    inode->deleted = 0;
    inode->mode = __S_IFDIR;
    inode->uid = getuid();
    inode->gid = getgid();
    inode->flags = 0;
    inode->size = 0;
    inode->atime = time(NULL);
    inode->mtime = time(NULL);
    inode->ctime = time(NULL);
    inode->links = 1;
    printf("Init root inode\n");

    // Initialize root log entry
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *)malloc(sizeof(struct wfs_log_entry));
    log_entry->inode = *inode;
    memcpy(log_entry->data, "", 0);
    printf("Copying in some empty data to log entry\n");

    // Copy root log entry into disk, update head, and free malloc'd ptrs
    memcpy((void *)((uintptr_t)mapped + sizeof(struct wfs_sb)), log_entry, sizeof(*log_entry));
    printf("Copy root log entry into disk\n");
    sb->head += sizeof(*log_entry);
    free(inode);
    free(log_entry);
    
    munmap(mapped, sb->head);
    close(fd);


    return 0;
}

int main( int argc, char *argv[] )  {

    if(argc == 2) {
        printf("The argument supplied is %s\n", argv[1]);
    }
    else if(argc > 2) {
        printf("Too many arguments supplied. Usage: mkfs.wfs <disk_path>\n");
    }
    else {
        printf("Usage: mkfs.wfs <disk_path>\n");
    }

    printf("FF\n");
    nextInodeNum = 0;
    printf("Checkpoint 1\n");

    disk_path = argv[1];
    printf("Checkpoint 2\n");
    init_filesystem(disk_path);
}

