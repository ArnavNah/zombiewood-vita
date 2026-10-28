/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <errno.h>
#include <strings.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"
#include "config.h"

// The APK probes app0 before its data search path for sounds and models. Those
// resources live in ux0 on Vita. An immutable name index avoids failed opens,
// repeated search-path probes and emulator error logging for every sound.
#define ASSET_INDEX_LIMIT 1024
#define ASSET_INDEX_BUCKETS 2048
static char asset_names[ASSET_INDEX_LIMIT][192];
static uint16_t asset_buckets[ASSET_INDEX_BUCKETS];
static unsigned asset_count;
static uint32_t asset_hash(const char *name) {
    uint32_t hash=2166136261u;
    while(*name){unsigned c=(unsigned char)*name++;if(c>='A' && c<='Z')c+='a'-'A';hash=(hash^c)*16777619u;}
    return hash;
}
static const char *indexed_asset(const char *name) {
    if(strchr(name,'/') || strchr(name,'\\'))return NULL;
    unsigned bucket=asset_hash(name)&(ASSET_INDEX_BUCKETS-1);
    for(unsigned probes=0;probes<ASSET_INDEX_BUCKETS;++probes){
        unsigned index=asset_buckets[bucket];
        if(!index)return NULL;
        if(!strcasecmp(name,asset_names[index-1]))return asset_names[index-1];
        bucket=(bucket+1)&(ASSET_INDEX_BUCKETS-1);
    }
    return NULL;
}
void io_asset_index_init(void) {
    asset_count=0;memset(asset_buckets,0,sizeof(asset_buckets));
    DIR *directory=opendir(DATA_PATH);
    if(!directory)return;
    struct dirent *entry;
    while((entry=readdir(directory)) && asset_count<ASSET_INDEX_LIMIT){
        const char *name=entry->d_name,*extension=strrchr(name,'.');
        if(!extension || SCE_S_ISDIR(entry->d_stat.st_mode) || strlen(name)>=sizeof(asset_names[0]))continue;
        if(!strcasecmp(name,"config.txt"))continue;
        static const char *types[]={".wav",".bdae",".vxn",".bar",".mpc",".obb",".swf",".xml",".txt",".png",".conf",".json"};
        int immutable=0;
        for(unsigned i=0;i<sizeof(types)/sizeof(types[0]);++i)if(!strcasecmp(extension,types[i]))immutable=1;
        if(!immutable || indexed_asset(name))continue;
        unsigned bucket=asset_hash(name)&(ASSET_INDEX_BUCKETS-1);
        while(asset_buckets[bucket])bucket=(bucket+1)&(ASSET_INDEX_BUCKETS-1);
        strcpy(asset_names[asset_count],name);
        asset_buckets[bucket]=++asset_count;
    }
    closedir(directory);
#ifdef PORT_TEST_INPUT
    unsigned passed=0;
    char expected[32]={0},actual[32]={0};
    FILE *input=fopen(DATA_PATH "GameVersion.txt","rb");
    size_t length=input?fread(expected,1,sizeof(expected),input):0;
    if(input)fclose(input);
    input=fopen_soloader("app0:/GameVersion.txt","rb");
    if(input){size_t count=fread(actual,1,sizeof(actual),input);passed+=length && count==length && !memcmp(expected,actual,length);fclose_soloader(input);}
    memset(actual,0,sizeof(actual));input=fopen_soloader("app0:/GAMEVERSION.TXT","rb");
    if(input){size_t count=fread(actual,1,sizeof(actual),input);passed+=length && count==length && !memcmp(expected,actual,length);fclose_soloader(input);}
    input=fopen_soloader("app0:/cpuinfo","rb");
    if(input){passed+=fgetc(input)!=EOF;fclose_soloader(input);}
    FILE *report=fopen(DATA_PATH "asset-index-test.txt","w");
    if(report){fprintf(report,"passed=%u/3 indexed=%u\n",passed,asset_count);fclose(report);}
#endif
}

static const char *vita_file_path(const char *path, char *buffer, size_t capacity) {
    if (!path) { errno = EFAULT; return NULL; }
    // GLF joins its current directory to an already absolute Vita device path.
    while (!strncmp(path, "./", 2) && strchr(path + 2, ':')) path += 2;
    // GLF may prepend either its application directory or its search path.
    // Every file operation must resolve these aliases to the same save file.
    static const char *prefixes[] = {
        "/data/data/" PACKAGE_NAME "/", "data/data/" PACKAGE_NAME "/",
        "app0:/data/data/" PACKAGE_NAME "/",
        DATA_PATH "data/data/" PACKAGE_NAME "/"
    };
    for (unsigned i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        size_t length = strlen(prefixes[i]);
        if (!strncmp(path, prefixes[i], length)) {
            int result = snprintf(buffer, capacity, DATA_PATH "%s", path + length);
            if (result < 0 || (size_t)result >= capacity) {
                errno = ENAMETOOLONG;
                return NULL;
            }
            return buffer;
        }
    }
    const char *relative=NULL;
    if(!strncmp(path,"app0:/",6))relative=path+6;
    else if(!strncmp(path,"app0:",5))relative=path+5;
    const char *asset=relative?indexed_asset(relative):NULL;
    if(asset){
        int result=snprintf(buffer,capacity,DATA_PATH "%s",asset);
        if(result<0 || (size_t)result>=capacity){errno=ENAMETOOLONG;return NULL;}
        return buffer;
    }
    return path;
}

int access_soloader(const char *path, int mode) {
    char buffer[PATH_MAX]; path = vita_file_path(path, buffer, sizeof(buffer));
    return path ? access(path, mode) : -1;
}
int mkdir_soloader(const char *path, mode_t mode) {
    char buffer[PATH_MAX]; path = vita_file_path(path, buffer, sizeof(buffer));
    return path ? mkdir(path, mode) : -1;
}
int remove_soloader(const char *path) {
    char buffer[PATH_MAX]; path = vita_file_path(path, buffer, sizeof(buffer));
    return path ? remove(path) : -1;
}
int rename_soloader(const char *from, const char *to) {
    char source[PATH_MAX], target[PATH_MAX];
    from = vita_file_path(from, source, sizeof(source));
    if (!from) return -1;
    to = vita_file_path(to, target, sizeof(target));
    return to ? rename(from, to) : -1;
}
int rmdir_soloader(const char *path) {
    char buffer[PATH_MAX]; path = vita_file_path(path, buffer, sizeof(buffer));
    return path ? rmdir(path) : -1;
}
int unlink_soloader(const char *path) {
    char buffer[PATH_MAX]; path = vita_file_path(path, buffer, sizeof(buffer));
    return path ? unlink(path) : -1;
}

void io_migrate_legacy_saves(void) {
    static const char *names[]={"player.dat","playerbckp.dat","level.dat","zomb.dat"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i){
        char source[PATH_MAX],target[PATH_MAX],temporary[PATH_MAX];
        snprintf(source,sizeof(source),"app0:/data/data/" PACKAGE_NAME "/%s",names[i]);
        snprintf(target,sizeof(target),DATA_PATH "%s",names[i]);
        snprintf(temporary,sizeof(temporary),DATA_PATH "%s.migration.tmp",names[i]);
        if(access(target,F_OK)==0)continue;
        FILE *input=fopen(source,"rb");
        if(!input)continue;
        FILE *output=fopen(temporary,"wb");
        if(!output){fclose(input);continue;}
        char buffer[4096];size_t count,total=0;int good=1;
        while((count=fread(buffer,1,sizeof(buffer),input))!=0){
            if(fwrite(buffer,1,count,output)!=count){good=0;break;}
            total+=count;
        }
        if(ferror(input))good=0;
        fclose(input);
        if(fclose(output))good=0;
        if(good && total){
            if(rename(temporary,target))remove(temporary);
        }else remove(temporary);
    }
}

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

FILE * fopen_soloader(const char * filename, const char * mode) {
    char translated_path[1024];
    filename = vita_file_path(filename, translated_path, sizeof(translated_path));
    if (!filename) return NULL;
    l_info("[IO ENTER] fopen(%s, %s)", filename, mode);
    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(filename, mode);
#else
    FILE* ret = fopen(filename, mode);
#endif

    if (ret)
        l_debug("fopen(%s, %s): %p", filename, mode, ret);
    else
        l_warn("fopen(%s, %s): %p", filename, mode, ret);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    char translated_path[1024];
    path = vita_file_path(path, translated_path, sizeof(translated_path));
    if (!path) return -1;
    l_info("[IO ENTER] open(%s, %x)", path, oflag);
    if (strcmp(path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    } else if (strcmp(path, "/dev/urandom") == 0) {
        return open_soloader("app0:/urandom", oflag);
    }

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(path, oflag, mode);
    if (ret >= 0)
        l_debug("open(%s, %x): %i", path, oflag, ret);
    else
        l_warn("open(%s, %x): %i", path, oflag, ret);
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    char translated_path[1024];
    path = vita_file_path(path, translated_path, sizeof(translated_path));
    if (!path) return -1;
    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        memset(buf, 0, sizeof(*buf));
        buf->st_mode = S_IFREG | 0444;
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    struct stat st;
    int res = stat(path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s): %i", path, res);
    return res;
}

int fclose_soloader(FILE * f) {
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int lstat_soloader(const char *path, stat64_bionic *buf) {
    char translated[PATH_MAX];
    path=vita_file_path(path,translated,sizeof(translated));
    if(!path)return -1;
    struct stat info;
    int result=lstat(path,&info);
    if(!result)stat_newlib_to_bionic(&info,buf);
    return result;
}

int close_soloader(int fd) {
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    char buffer[PATH_MAX];
    _pathname = (char *)vita_file_path(_pathname, buffer, sizeof(buffer));
    if (!_pathname) return NULL;
    DIR* ret = opendir(_pathname);
    l_debug("opendir(\"%s\"): %p", _pathname, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        if (!entry_tmp) { errno=ENOMEM; return NULL; }
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0 && pdirent_tmp != NULL) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        if (!entry_tmp) { *result=NULL; return ENOMEM; }
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    } else {
        *result = NULL;
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}
