/* Native automated compatibility tests; not linked into the game release. */
#include "reimpl/pthr.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

int _newlib_heap_size_user=16*1024*1024;
void _log_print(int type,const char *fmt,...) {}
uint64_t current_timestamp_ms(void) {
    struct timeval t;gettimeofday(&t,NULL);return (uint64_t)t.tv_sec*1000+t.tv_usec/1000;
}
static volatile int once,go,ready,initialized,initializations,failures;
static pthread_mutex_t_bionic counter_mutex;
static unsigned counter;
static void initialize(void) {
    __atomic_add_fetch(&initializations,1,__ATOMIC_RELAXED);
    sceKernelDelayThread(20000);
    initialized=1;
}
static void *worker(void *arg) {
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))sceKernelDelayThread(50);
    if(pthread_once_soloader(&once,initialize) || initialized!=1)
        __atomic_add_fetch(&failures,1,__ATOMIC_RELAXED);
    for(unsigned i=0;i<1000;++i) {
        if(pthread_mutex_lock_soloader(&counter_mutex)) {++failures;break;}
        ++counter;
        pthread_mutex_unlock_soloader(&counter_mutex);
    }
    return NULL;
}
static void *post_later(void *arg) {sceKernelDelayThread(20000);sem_post_soloader(arg);return NULL;}
static struct timespec deadline(unsigned ms) {
    struct timeval t;gettimeofday(&t,NULL);
    int64_t ns=(int64_t)t.tv_usec*1000+(int64_t)ms*1000000;
    return (struct timespec){t.tv_sec+ns/1000000000,ns%1000000000};
}
int main(void) {
    unsigned passed=0,total=0;
    for(unsigned round=0;round<4;++round) {
        once=go=ready=initialized=initializations=failures=0;counter=0;
        counter_mutex=(pthread_mutex_t_bionic){0};
        pthread_t threads[8];unsigned created=0;
        for(;created<8;++created) if(pthread_create(&threads[created],NULL,worker,NULL))break;
        while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=(int)created)sceKernelDelayThread(50);
        __atomic_store_n(&go,1,__ATOMIC_RELEASE);
        for(unsigned i=0;i<created;++i)pthread_join(threads[i],NULL);
        ++total;passed+=created==8 && initializations==1 && failures==0;
        ++total;passed+=created==8 && counter==8000;
        pthread_mutex_destroy_soloader(&counter_mutex);
    }
    pthread_attr_t_bionic attr={0};pthread_t thread;
    int ret=pthread_attr_init_soloader(&attr);
    ret|=pthread_attr_setstacksize_soloader(&attr,2*1024*1024);
    int sem;sem_init_soloader(&sem,0,0);
    ret|=pthread_create_soloader(&thread,&attr,post_later,&sem);
    size_t stack=0;pthread_attr_getstacksize(attr.real_ptr,&stack);
    ++total;passed+=ret==0 && stack==2*1024*1024;
    struct timespec end=deadline(500);
    ++total;passed+=sem_timedwait_soloader(&sem,&end)==0;
    pthread_join(thread,NULL);pthread_attr_destroy_soloader(&attr);
    end=deadline(30);errno=0;
    ++total;passed+=sem_timedwait_soloader(&sem,&end)==-1 && errno==ETIMEDOUT;
    sem_post_soloader(&sem);end=(struct timespec){0,0};
    ++total;passed+=sem_timedwait_soloader(&sem,&end)==0;
    sem_destroy_soloader(&sem);
    char result[128];int n=snprintf(result,sizeof(result),"thread bridge: %u/%u passed\n",passed,total);
    sceIoRemove("ux0:data/thread-review.txt");
    int fd=sceIoOpen("ux0:data/thread-review.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){sceIoWrite(fd,result,n);sceIoClose(fd);}
    sceKernelExitProcess(passed==total?0:1);return 0;
}
