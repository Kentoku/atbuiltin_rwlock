/*
  Atbuiltin RW lock functions : RW lock functions using atomic builtins

  Copyright (C) 2014, Kentoku SHIBA
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of Kentoku SHIBA nor the names of its contributors
        may be used to endorse or promote products derived from this software
        without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY Kentoku SHIBA "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
  EVENT SHALL Kentoku SHIBA BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef _ATBUILTIN_RWLOCK_H
#define _ATBUILTIN_RWLOCK_H
#include <pthread.h>
#include <limits.h>
#include <time.h>

#define ATBUILTIN_RWLOCK_READ_PRIORITY  0
#define ATBUILTIN_RWLOCK_NO_PRIORITY    1
#define ATBUILTIN_RWLOCK_WRITE_PRIORITY 2

#if __GNUC__ > 4 || \
  (__GNUC__ == 4 && (__GNUC_MINOR__ > 7 || \
                   (__GNUC_MINOR__ == 7 && __GNUC_PATCHLEVEL__ > 0)))
#else
  #ifndef ATBUILTIN_RWLOCK_USE_SYNC_BUILTIN
    #define ATBUILTIN_RWLOCK_USE_SYNC_BUILTIN
  #endif
#endif

#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  #define atbuiltin_rwlock_signed long long int
  #define atbuiltin_rwlock_unsigned unsigned long long int
  #define ATBUILTIN_RWLOCK_MIN_VAL LLONG_MIN
#else
  #define atbuiltin_rwlock_signed int
  #define atbuiltin_rwlock_unsigned unsigned int
  #define ATBUILTIN_RWLOCK_MIN_VAL INT_MIN
#endif

#ifdef ATBUILTIN_RWLOCK_USE_SYNC_BUILTIN
  #define ATBUILTIN_RWLOCK_RELAXED
  #define ATBUILTIN_RWLOCK_CONSUME
  #define ATBUILTIN_RWLOCK_ACQUIRE
  #define ATBUILTIN_RWLOCK_RELEASE
  #define ATBUILTIN_RWLOCK_ACQ_REL
  #define ATBUILTIN_RWLOCK_SEQ_CST
  #define atbuiltin_compare_and_swap_n(A, B, C, D, E, F) \
    __sync_bool_compare_and_swap(A, *(B), C)
  #define atbuiltin_add_and_fetch(A, B, C) \
    __sync_add_and_fetch(A, B)
  #define atbuiltin_sub_and_fetch(A, B, C) \
    __sync_sub_and_fetch(A, B)
#else
  #define ATBUILTIN_RWLOCK_RELAXED __ATOMIC_RELAXED
  #define ATBUILTIN_RWLOCK_CONSUME __ATOMIC_CONSUME
  #define ATBUILTIN_RWLOCK_ACQUIRE __ATOMIC_ACQUIRE
  #define ATBUILTIN_RWLOCK_RELEASE __ATOMIC_RELEASE
  #define ATBUILTIN_RWLOCK_ACQ_REL __ATOMIC_ACQ_REL
  #define ATBUILTIN_RWLOCK_SEQ_CST __ATOMIC_SEQ_CST
  #define atbuiltin_compare_and_swap_n(A, B, C, D, E, F) \
    __atomic_compare_exchange_n(A, B, C, D, E, F)
  #define atbuiltin_add_and_fetch(A, B, C) \
    __atomic_add_fetch(A, B, C)
  #define atbuiltin_sub_and_fetch(A, B, C) \
    __atomic_sub_fetch(A, B, C)
#endif

#ifdef ATBUILTIN_RWLOCK_USE_STRONG_FOR_CAS
  #define ATBUILTIN_RWLOCK_CAS_WEAK false
#else
  #define ATBUILTIN_RWLOCK_CAS_WEAK true
#endif

struct atbuiltin_rwlock_attr_t
{
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;
  int rwlock_attr;
  unsigned long long int write_lock_interval;
};

struct atbuiltin_rwlock_t
{
  atbuiltin_rwlock_signed lock_body;
  atbuiltin_rwlock_unsigned writer_count;
  volatile atbuiltin_rwlock_unsigned tr_waiter_count;
  unsigned long long int write_lock_interval;
  struct timespec write_lock_interval_ts;
  volatile bool read_waiting;
  volatile bool write_waiting;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int (*timedrlock)(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
  int (*rlock)(atbuiltin_rwlock_t *lock);
  int (*timedwlock)(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
  int (*wlock)(atbuiltin_rwlock_t *lock);
  int (*wunlock)(atbuiltin_rwlock_t *lock);
};

int atbuiltin_rwlockattr_setpshared_cond(atbuiltin_rwlock_attr_t *attr, int pshared);
int atbuiltin_rwlockattr_getpshared_cond(atbuiltin_rwlock_attr_t *attr, int *pshared);
int atbuiltin_rwlockattr_init(atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlockattr_destroy(atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlockattr_settype_mutex(atbuiltin_rwlock_attr_t *attr, int kind);
int atbuiltin_rwlockattr_gettype_mutex(atbuiltin_rwlock_attr_t *attr, int *kind);
int atbuiltin_rwlockattr_settype_priority(atbuiltin_rwlock_attr_t *attr, int priority);
int atbuiltin_rwlockattr_gettype_priority(atbuiltin_rwlock_attr_t *attr, int *priority);
int atbuiltin_rwlockattr_settype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int interval);
int atbuiltin_rwlockattr_gettype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int *interval);
int atbuiltin_rwlock_init(atbuiltin_rwlock_t *lock, const atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlock_destroy(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_tryrlock(atbuiltin_rwlock_t *lock);
#define atbuiltin_rwlock_timedrlock(A, B) (A)->timedrlock(A, B)
#define atbuiltin_rwlock_rlock(A) (A)->rlock(A)
int atbuiltin_rwlock_runlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_trywlock(atbuiltin_rwlock_t *lock);
#define atbuiltin_rwlock_timedwlock(A, B) (A)->timedwlock(A, B)
#define atbuiltin_rwlock_wlock(A) (A)->wlock(A)
#define atbuiltin_rwlock_wunlock(A) (A)->wunlock(A)

#endif /* _ATBUILTIN_RWLOCK_H */
