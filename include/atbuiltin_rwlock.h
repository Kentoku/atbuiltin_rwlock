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
#include <time.h>

#define ATBUILTIN_RWLOCK_READ_PRIORITY  0
#define ATBUILTIN_RWLOCK_NO_PRIORITY    1
#define ATBUILTIN_RWLOCK_WRITE_PRIORITY 2

struct atbuiltin_rwlock_attr_t
{
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;
  int rwlock_attr;
  unsigned long long int write_lock_interval;
};

struct atbuiltin_rwlock_t
{
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  long long int lock_body;
  unsigned long long int writer_count;
#else
  int lock_body;
  unsigned int writer_count;
#endif
  unsigned long long int write_lock_interval;
  struct timespec write_lock_interval_ts;
  volatile bool write_waiting;
  bool write_priority;
  bool write_counting;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
};

int atbuiltin_rwlockattr_setpshared_cond(atbuiltin_rwlock_attr_t *attr, int pshared);
int atbuiltin_rwlockattr_getpshared_cond(atbuiltin_rwlock_attr_t *attr, int *pshared);
int atbuiltin_rwlockattr_init(atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlockattr_destroy(atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlockattr_settype_mutex(atbuiltin_rwlock_attr_t *attr, int kind);
int atbuiltin_rwlockattr_gettype_mutex(atbuiltin_rwlock_attr_t *attr, int *kind);
int atbuiltin_rwlockattr_settype_np(atbuiltin_rwlock_attr_t *attr, int kind);
int atbuiltin_rwlockattr_gettype_np(atbuiltin_rwlock_attr_t *attr, int *kind);
int atbuiltin_rwlockattr_settype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int interval);
int atbuiltin_rwlockattr_gettype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int *interval);
int atbuiltin_rwlock_init(atbuiltin_rwlock_t *lock, const atbuiltin_rwlock_attr_t *attr);
int atbuiltin_rwlock_destroy(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_tryrlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_timedrlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
int atbuiltin_rwlock_rlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_runlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_trywlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_timedwlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
int atbuiltin_rwlock_wlock(atbuiltin_rwlock_t *lock);
int atbuiltin_rwlock_wunlock(atbuiltin_rwlock_t *lock);

#endif /* _ATBUILTIN_RWLOCK_H */
