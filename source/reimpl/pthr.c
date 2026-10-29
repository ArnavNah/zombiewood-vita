/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022      GrapheneCt
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/pthr.h"

#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <errno.h>
#include <psp2/kernel/error.h>

#include "utils/utils.h"
#include "utils/logger.h"

#define PTHR_MAX_OBJECTS 1024

#define BIONIC_PTHREAD_COND_INITIALIZER              0
#define BIONIC_PTHREAD_MUTEX_INITIALIZER             0
#define BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER   0x4000
#define BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER  0x8000

enum {
    BIONIC_PTHREAD_MUTEX_NORMAL = 0,
    BIONIC_PTHREAD_MUTEX_RECURSIVE = 1,
    BIONIC_PTHREAD_MUTEX_ERRORCHECK = 2,

    BIONIC_PTHREAD_MUTEX_ERRORCHECK_NP = BIONIC_PTHREAD_MUTEX_ERRORCHECK,
    BIONIC_PTHREAD_MUTEX_RECURSIVE_NP  = BIONIC_PTHREAD_MUTEX_RECURSIVE,

    BIONIC_PTHREAD_MUTEX_DEFAULT = BIONIC_PTHREAD_MUTEX_NORMAL
};

#define PTHR_INLINE static inline __attribute__((always_inline))

void * initializedObjects[PTHR_MAX_OBJECTS] = {0};
static pthread_mutex_t pthr_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t object_init_mutex = PTHREAD_MUTEX_INITIALIZER;

#define PTHR_LOCK pthread_mutex_lock(&pthr_mutex);

#define PTHR_UNLOCK pthread_mutex_unlock(&pthr_mutex);

int isObjectInitialized(const void * mut) {
    PTHR_LOCK
    for (int i = 0; i < PTHR_MAX_OBJECTS; ++i) {
        if (initializedObjects[i] == mut) {
            PTHR_UNLOCK
            return 1;
        }
    }
    PTHR_UNLOCK
    return 0;
}

int rememberObject(void * mut) {
    PTHR_LOCK
    for (int i = 0; i < PTHR_MAX_OBJECTS; ++i) {
        if (initializedObjects[i] == 0) {
            initializedObjects[i] = mut;
            PTHR_UNLOCK
            return 1;
        }
    }
    PTHR_UNLOCK
    return 0;
}

int forgetObject(const void * mut) {
    PTHR_LOCK
    for (int i = 0; i < PTHR_MAX_OBJECTS; ++i) {
        if (initializedObjects[i] == mut) {
            initializedObjects[i] = 0;
            PTHR_UNLOCK
            return 1;
        }
    }
    PTHR_UNLOCK
    return 0;
}

// null check for `attr` must be performed before this
PTHR_INLINE int _attr_t_static_init(pthread_attr_t_bionic * attr) {
    pthread_mutex_lock(&object_init_mutex);
    int ret = 0;
    if (attr->magic != 0x42424242) {
        attr->real_ptr = malloc(sizeof(pthread_attr_t));
        if (!attr->real_ptr) ret = ENOMEM;
        else {
            ret = pthread_attr_init(attr->real_ptr);
            if (!ret) ret = pthread_attr_setstacksize(attr->real_ptr, 1024 * 1024);
            if (ret) { free(attr->real_ptr); attr->real_ptr = NULL; }
            else { attr->stack_size = 1024 * 1024; attr->magic = 0x42424242; }
        }
    }
    pthread_mutex_unlock(&object_init_mutex);
    return ret;
}

// null check for `mutex` param must be performed before this, `attr` is fine as null
PTHR_INLINE int _mutex_t_static_init(pthread_mutex_t_bionic * mutex, const pthread_mutexattr_t * attr) {
    int ret = 0, kind = PTHREAD_MUTEX_NORMAL;
    if (isObjectInitialized(mutex)) return 0;
    pthread_mutex_lock(&object_init_mutex);
    if (isObjectInitialized(mutex)) {
        //logv_debug("mutex already initialized: %p", mutex);
        pthread_mutex_unlock(&object_init_mutex);
        return ret;
    }

    if (attr) {
        pthread_mutexattr_gettype((pthread_mutexattr_t *) attr, &kind);
    } else {
        if (* (int *) mutex == BIONIC_PTHREAD_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_NORMAL;
        else if (* (int *) mutex == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_RECURSIVE;
        else if (* (int *) mutex == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_ERRORCHECK;
    }

    mutex->real_ptr = malloc(sizeof(pthread_mutex_t));
    if (!mutex->real_ptr) { pthread_mutex_unlock(&object_init_mutex); return ENOMEM; }

    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_settype(&mutattr, kind);
    ret = pthread_mutex_init(mutex->real_ptr, &mutattr);
    pthread_mutexattr_destroy(&mutattr);

    if (ret == 0) {
        if (!rememberObject(mutex)) { pthread_mutex_destroy(mutex->real_ptr); ret = EAGAIN; }
    } else {
        l_error("mutex initialization for %p has failed", mutex);
    }

    if (ret) { free(mutex->real_ptr); mutex->real_ptr = NULL; }
    pthread_mutex_unlock(&object_init_mutex);
    return ret;
}

// null check for `cond` param must be performed before this, `attr` is fine as null
PTHR_INLINE int _cond_t_static_init(pthread_cond_t_bionic * cond, const pthread_condattr_t * attr) {
    int ret = 0;
    if (isObjectInitialized(cond)) return 0;
    pthread_mutex_lock(&object_init_mutex);
    if (isObjectInitialized(cond)) {
        //logv_debug("cond already initialized: %p", cond);
        pthread_mutex_unlock(&object_init_mutex);
        return ret;
    }

    cond->real_ptr = malloc(sizeof(pthread_cond_t));
    if (!cond->real_ptr) { pthread_mutex_unlock(&object_init_mutex); return ENOMEM; }

    ret = pthread_cond_init(cond->real_ptr, attr);

    if (ret == 0) {
        if (!rememberObject(cond)) { pthread_cond_destroy(cond->real_ptr); ret = EAGAIN; }
    } else {
        l_error("cond initialization for %p has failed", cond);
    }

    if (ret) { free(cond->real_ptr); cond->real_ptr = NULL; }
    pthread_mutex_unlock(&object_init_mutex);
    return ret;
}

int pthread_create_soloader(pthread_t *thread, const pthread_attr_t_bionic *attr, void *(*start)(void *), void *param) {
    int ret;

    if (!attr) {
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 1024 * 1024);
        ret = pthread_create(thread, &a, start, param);
        pthread_attr_destroy(&a);
    } else{
        ret = _attr_t_static_init((pthread_attr_t_bionic *) attr);
        if (ret) return ret;
        ret = pthread_create(thread, attr->real_ptr, start, param);
    }

    return ret;
}

int pthread_mutexattr_init_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_init(attr);
}

int pthread_mutexattr_settype_soloader(pthread_mutexattr_t *attr, int type)
{
    return pthread_mutexattr_settype(attr, type);
}

int pthread_mutexattr_destroy_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_destroy(attr);
}

int pthread_kill_soloader(pthread_t thread, int sig)
{
    return pthread_kill(thread, sig);
}

int pthread_mutex_init_soloader(pthread_mutex_t_bionic *uid, const pthread_mutexattr_t *attr)
{
    if (!uid) return EINVAL;
    return _mutex_t_static_init(uid, attr);
}

int pthread_mutex_destroy_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return 0;
    if (!isObjectInitialized(mutex)) return EINVAL;
    int ret = pthread_mutex_destroy(mutex->real_ptr);
    if (!ret) {
        forgetObject(mutex);
        free(mutex->real_ptr);
        mutex->real_ptr = NULL;
    }
    return ret;
}

int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    int ret = _mutex_t_static_init(mutex, NULL);
    if (ret) return ret;
    return pthread_mutex_lock(mutex->real_ptr);
}

int pthread_mutex_trylock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    int ret = _mutex_t_static_init(mutex, NULL);
    if (ret) return ret;
    return pthread_mutex_trylock(mutex->real_ptr);
}

int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    if (!mutex->real_ptr) return EINVAL;
    return pthread_mutex_unlock(mutex->real_ptr);
}

int pthread_join_soloader(pthread_t thread, void **value_ptr)
{
    return pthread_join(thread, value_ptr);
}

int pthread_condattr_init_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_init(attr);
}

int pthread_condattr_destroy_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_destroy(attr);
}

int pthread_cond_init_soloader(pthread_cond_t_bionic *cond,
                               const pthread_condattr_t *attr)
{
    if (!cond) return EINVAL;

    return _cond_t_static_init(cond, attr);
}

int pthread_cond_destroy_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    if (!isObjectInitialized(cond)) return EINVAL;
    int ret = pthread_cond_destroy(cond->real_ptr);
    if (!ret) {
        forgetObject(cond);
        free(cond->real_ptr);
        cond->real_ptr = NULL;
    }
    return ret;
}

int pthread_cond_signal_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return EINVAL;

    int ret = _cond_t_static_init(cond, NULL);
    if (ret) return ret;

    return pthread_cond_signal(cond->real_ptr);
}

int pthread_cond_timedwait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex, struct timespec *abstime)
{
    if (!cond || !mutex) return EINVAL;

    int ret = _cond_t_static_init(cond, NULL);
    if (ret) return ret;
    ret = _mutex_t_static_init(mutex, NULL);
    if (ret) return ret;

    return pthread_cond_timedwait(cond->real_ptr, mutex->real_ptr, abstime);
}


int pthread_cond_wait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex)
{
    if (!cond || !mutex) return EINVAL;

    int ret = _cond_t_static_init(cond, NULL);
    if (ret) return ret;
    ret = _mutex_t_static_init(mutex, NULL);
    if (ret) return ret;

    return pthread_cond_wait(cond->real_ptr, mutex->real_ptr);
}

int pthread_cond_broadcast_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return EINVAL;

    int ret = _cond_t_static_init(cond, NULL);
    if (ret) return ret;

    return pthread_cond_broadcast(cond->real_ptr);
}

int pthread_attr_init_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return EINVAL;
    attr->magic = 0;
    return _attr_t_static_init(attr);
}

int pthread_attr_destroy_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return 0;
    if (attr->magic != 0x42424242) return 0;

    int ret = pthread_attr_destroy(attr->real_ptr);
    free(attr->real_ptr);
    attr->magic = 0x0;

    return ret;
}

int pthread_attr_setdetachstate_soloader(pthread_attr_t_bionic *attr, int state)
{
    if (!attr) return -1;
    int ret = _attr_t_static_init(attr);
    if (ret) return ret;
    if (state != 0 && state != 1) return EINVAL;
    state = state ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    return pthread_attr_setdetachstate(attr->real_ptr, state);
}

int pthread_attr_setstacksize_soloader(pthread_attr_t_bionic *attr, size_t stacksize) {
    if (!attr) return -1;
    int ret = _attr_t_static_init(attr);
    if (ret) return ret;
    ret = pthread_attr_setstacksize(attr->real_ptr, stacksize);
    if (!ret) attr->stack_size = stacksize;
    return ret;
}

int pthread_setschedparam_soloader(pthread_t thread, int policy,
                                   const struct sched_param *param)
{
   return pthread_setschedparam(thread, policy, param);
}

int pthread_getschedparam_soloader(pthread_t thread, int *policy,
                                   struct sched_param *param)
{
    return pthread_getschedparam(thread, policy, param);
}

int pthread_detach_soloader(pthread_t thread)
{
    return pthread_detach(thread);
}

int pthread_equal_soloader(const pthread_t t1, const pthread_t t2)
{
    if (t1 == t2)
        return 1;
    if (!t1 || !t2)
        return 0;
    return pthread_equal(t1, t2);
}

pthread_t pthread_self_soloader()
{
    return pthread_self();
}

int pthread_once_soloader(volatile int *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine)
        return -1;
    // Publish completion separately from ownership. A competing thread must
    // not observe partially initialized renderer/audio state.
    if (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 2) return 0;
    int expected = 0;
    if (__atomic_compare_exchange_n(once_control, &expected, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        (*init_routine)();
        __atomic_store_n(once_control, 2, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) != 2)
            sceKernelDelayThread(50);
    }
    return 0;
}

#ifndef MAX_TASK_COMM_LEN
#define MAX_TASK_COMM_LEN 16
#endif

int pthread_setname_np_soloader(pthread_t thread, const char* thread_name) {
    if (thread == 0 || thread_name == NULL) {
        return EINVAL;
    }
    size_t thread_name_len = strlen(thread_name);
    if (thread_name_len >= MAX_TASK_COMM_LEN) {
        return ERANGE;
    }

    sceClibPrintf("PTHREAD: pthread_setname_np with name %s for thread:0x%x\n", thread_name, pthread_self());

    return 0;
}

int sem_destroy_soloader(int * uid) {
    if (sceKernelDeleteSema(*uid) < 0)
        return -1;
    return 0;
}

int sem_getvalue_soloader (int * uid, int * sval) {
    SceKernelSemaInfo info;
    info.size = sizeof(SceKernelSemaInfo);

    if (sceKernelGetSemaInfo(*uid, &info) < 0) return -1;
    if (!sval) sval = malloc(sizeof(int32_t));
    *sval = info.currentCount;
    return 0;
}

int sem_init_soloader (int * uid, int pshared, unsigned int value) {
    *uid = sceKernelCreateSema("sema", 0, (int) value, 0x7fffffff, NULL);
    if (*uid < 0)
        return -1;
    return 0;
}

int sem_post_soloader (int * uid) {
    if (sceKernelSignalSema(*uid, 1) < 0)
        return -1;
    return 0;
}

int sem_timedwait_soloader (int * uid, const struct timespec * abstime) {
    if (!uid || !abstime || abstime->tv_nsec<0 || abstime->tv_nsec>=1000000000L) {
        errno=EINVAL; return -1;
    }
    if (sceKernelPollSema(*uid,1)>=0) return 0;
    int64_t deadline=(int64_t)abstime->tv_sec*1000000+abstime->tv_nsec/1000;
    for (;;) {
        int64_t remaining=deadline-(int64_t)current_timestamp_ms()*1000;
        if (remaining<=0) { errno=ETIMEDOUT; return -1; }
        unsigned timeout=remaining>UINT32_MAX?UINT32_MAX:(unsigned)remaining;
        int ret=sceKernelWaitSema(*uid,1,&timeout);
        if (ret>=0) return 0;
        if (ret!=SCE_KERNEL_ERROR_WAIT_TIMEOUT) { errno=EINVAL; return -1; }
    }
}

int sem_trywait_soloader (int * uid) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) < 0)
        return -1;
    return 0;
}

int sem_wait_soloader (int * uid) {
    if (sceKernelWaitSema(*uid, 1, NULL) < 0)
        return -1;
    return 0;
}
