/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  io.h
 * @brief Wrappers and implementations for some of the IO functions.
 */

#ifndef SOLOADER_IO_H
#define SOLOADER_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>
#include <sys/fcntl.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef DT_DIR
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
#endif

// Android ARM EABI; Vita's uid/gid/nlink types and packed layout differ.
// Reference: bionic android-4.1.2_r1/libc/include/sys/stat.h.
typedef struct { int32_t tv_sec, tv_nsec; } timespec32_bionic;
typedef struct stat64_bionic {
    uint64_t st_dev;
    uint8_t __pad0[4];
    uint32_t __st_ino, st_mode, st_nlink, st_uid, st_gid;
    uint64_t st_rdev;
    uint8_t __pad3[4];
    int64_t st_size;
    uint32_t st_blksize;
    uint64_t st_blocks;
    timespec32_bionic st_atim, st_mtim, st_ctim;
    uint64_t st_ino;
} stat64_bionic;

typedef struct dirent64_bionic {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
} dirent64_bionic;

#ifndef __cplusplus
_Static_assert(sizeof(stat64_bionic)==104, "Android ARM stat size");
_Static_assert(offsetof(stat64_bionic,st_size)==48, "Android ARM file size offset");
_Static_assert(offsetof(stat64_bionic,st_blocks)==64, "Android ARM block count offset");
_Static_assert(offsetof(stat64_bionic,st_atim)==72, "Android ARM timestamp offset");
_Static_assert(offsetof(stat64_bionic,st_ino)==96, "Android ARM inode offset");
_Static_assert(sizeof(dirent64_bionic)==280, "Android ARM dirent size");
_Static_assert(offsetof(dirent64_bionic,d_name)==19, "Android ARM directory name offset");
#endif

int open_soloader(const char * path, int oflag, ...);
int access_soloader(const char *path, int mode);
int mkdir_soloader(const char *path, mode_t mode);
int remove_soloader(const char *path);
int rename_soloader(const char *from, const char *to);
int rmdir_soloader(const char *path);
int unlink_soloader(const char *path);

FILE * fopen_soloader(const char * filename, const char * mode);

DIR *opendir_soloader(char *name);

int stat_soloader(const char * path, stat64_bionic * buf);
int lstat_soloader(const char *path, stat64_bionic *buf);
void io_migrate_legacy_saves(void);
void io_asset_index_init(void);

int fstat_soloader(int fd, stat64_bionic * buf);

struct dirent64_bionic * readdir_soloader(DIR *dir);

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result);

int close_soloader(int fd);

int fclose_soloader(FILE *f);

int closedir_soloader(DIR *dir);

int fcntl_soloader(int fd, int cmd, ...);

int ioctl_soloader(int fd, int request, ... /* arg */);

int fsync_soloader(int fd);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_IO_H
