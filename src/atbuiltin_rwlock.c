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

#include <errno.h>
#include <limits.h>
#include <atbuiltin_rwlock.h>

static void get_timespec_from_nanosec(struct timespec *ts, unsigned long long int nanosec)
{
  ts->tv_sec = nanosec / 1000000000;
  ts->tv_nsec = nanosec % 1000000000;
}

static struct timespec *get_smaller_timespec(struct timespec *ts1, struct timespec *ts2)
{
  if (ts1->tv_sec > ts2->tv_sec)
    return ts2;
  else if (ts1->tv_sec < ts2->tv_sec)
    return ts1;
  else if (ts1->tv_nsec > ts2->tv_nsec)
    return ts2;
  return ts1;
}

static bool timespec_sub(struct timespec *tsr, const struct timespec *tss, const struct timespec *tse)
{
  time_t tv_sec = (tse->tv_sec > tss->tv_sec ? tse->tv_sec - tss->tv_sec : 0);
  long tv_nsec;
  if (tse->tv_nsec >= tss->tv_nsec)
  {
    tv_nsec = tse->tv_nsec - tss->tv_nsec;
  } else if (tv_sec > 0) {
    tv_sec--;
    tv_nsec = 1000000000 - tss->tv_nsec + tse->tv_nsec;
  } else {
    tv_nsec = 0;
  }
  if (
    tsr->tv_sec > tv_sec ||
    (tsr->tv_sec == tv_sec && tsr->tv_nsec > tv_nsec)
  ) {
    tsr->tv_sec -= tv_sec;
    if (tsr->tv_nsec >= tv_nsec)
    {
      tsr->tv_nsec -= tv_nsec;
    } else {
      tsr->tv_sec--;
      tsr->tv_nsec = 1000000000 - tsr->tv_nsec + tv_nsec;
    }
  } else {
    return true;
  }
  return false;
}

int atbuiltin_rwlockattr_init(atbuiltin_rwlock_attr_t *attr)
{
  attr->rwlock_attr = PTHREAD_RWLOCK_PREFER_READER_NP;
  attr->write_lock_interval = 0;
  return pthread_mutexattr_init(&attr->mutex_attr);
}

int atbuiltin_rwlockattr_destroy(atbuiltin_rwlock_attr_t *attr)
{
  return pthread_mutexattr_destroy(&attr->mutex_attr);
}

int atbuiltin_rwlockattr_settype_mutex(atbuiltin_rwlock_attr_t *attr, int kind)
{
  return pthread_mutexattr_settype(&attr->mutex_attr, kind);
}

int atbuiltin_rwlockattr_gettype_mutex(atbuiltin_rwlock_attr_t *attr, int *kind)
{
  return pthread_mutexattr_gettype(&attr->mutex_attr, kind);
}

int atbuiltin_rwlockattr_settype_np(atbuiltin_rwlock_attr_t *attr, int kind)
{
  switch (kind)
  {
    case PTHREAD_RWLOCK_PREFER_READER_NP:
    case PTHREAD_RWLOCK_PREFER_WRITER_NP:
    case PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP:
      break;
    default:
      return EINVAL;
  }
  attr->rwlock_attr = kind;
  return 0;
}

int atbuiltin_rwlockattr_gettype_np(atbuiltin_rwlock_attr_t *attr, int *kind)
{
  *kind = attr->rwlock_attr;
  return 0;
}

int atbuiltin_rwlockattr_settype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int interval)
{
  attr->write_lock_interval = interval;
  return 0;
}

int atbuiltin_rwlockattr_gettype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int *interval)
{
  *interval = attr->write_lock_interval;
  return 0;
}

int atbuiltin_rwlock_init(atbuiltin_rwlock_t *lock, const atbuiltin_rwlock_attr_t *attr)
{
  lock->lock_body = 0;
  lock->write_waiting = false;
  if (attr)
  {
    lock->write_lock_interval = attr->write_lock_interval;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    if (attr->rwlock_attr == PTHREAD_RWLOCK_PREFER_READER_NP)
    {
      lock->write_priority = false;
    } else {
      lock->write_priority = true;
    }
    return pthread_mutex_init(&lock->mutex, &attr->mutex_attr);
  } else {
    lock->write_lock_interval = 0;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    lock->write_priority = false;
    return pthread_mutex_init(&lock->mutex, NULL);
  }
}

int atbuiltin_rwlock_destroy(atbuiltin_rwlock_t *lock)
{
  return pthread_mutex_destroy(&lock->mutex);
}

int atbuiltin_rwlock_tryrlock(atbuiltin_rwlock_t *lock)
{
  int res;
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  long long int cnt;
#else
  int cnt;
#endif
  if (lock->write_priority && lock->write_waiting)
  {
    if ((res = pthread_mutex_trylock(&lock->mutex)))
      return res;
    pthread_mutex_unlock(&lock->mutex);
  }
  cnt = __sync_add_and_fetch(&lock->lock_body, 1);
  if (cnt > 0)
  {
    /* lock success */
    return 0;
  }
  __sync_sub_and_fetch(&lock->lock_body, 1);
  return EBUSY;
}

int atbuiltin_rwlock_timedrlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  long long int cnt;
#else
  int cnt;
#endif
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  if (lock->write_priority && lock->write_waiting)
  {
    if ((res = pthread_mutex_timedlock(&lock->mutex, timeout)))
    {
      return res;
    }
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = __sync_add_and_fetch(&lock->lock_body, 1);
    if (cnt > 0)
    {
      /* lock success */
      return 0;
    }
    __sync_sub_and_fetch(&lock->lock_body, 1);
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      return ETIMEDOUT;
    }
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      return res;
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

int atbuiltin_rwlock_rlock(atbuiltin_rwlock_t *lock)
{
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  long long int cnt;
#else
  int cnt;
#endif
  if (lock->write_priority && lock->write_waiting)
  {
    pthread_mutex_lock(&lock->mutex);
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = __sync_add_and_fetch(&lock->lock_body, 1);
    if (cnt > 0)
    {
      /* lock success */
      return 0;
    }
    __sync_sub_and_fetch(&lock->lock_body, 1);
    pthread_mutex_lock(&lock->mutex);
    pthread_mutex_unlock(&lock->mutex);
  }
}

int atbuiltin_rwlock_runlock(atbuiltin_rwlock_t *lock)
{
  __sync_sub_and_fetch(&lock->lock_body, 1);
  return 0;
}

int atbuiltin_rwlock_trywlock(atbuiltin_rwlock_t *lock)
{
  int ret;
  if ((ret = pthread_mutex_trylock(&lock->mutex)))
    return ret;
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  if (__sync_bool_compare_and_swap(&lock->lock_body, 0, LLONG_MIN))
#else
  if (__sync_bool_compare_and_swap(&lock->lock_body, 0, INT_MIN))
#endif
  {
    /* lock success */
    return 0;
  }
  pthread_mutex_unlock(&lock->mutex);
  return EBUSY;
}

int atbuiltin_rwlock_timedwlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  if (pthread_mutex_trylock(&lock->mutex))
  {
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      return res;
    }
  }
  lock->write_waiting = true;
  while (true)
  {
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
    if (__sync_bool_compare_and_swap(&lock->lock_body, 0, LLONG_MIN))
#else
    if (__sync_bool_compare_and_swap(&lock->lock_body, 0, INT_MIN))
#endif
    {
      /* lock success */
      return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      pthread_mutex_unlock(&lock->mutex);
      return ETIMEDOUT;
    }
    if (lock->write_lock_interval)
    {
      nanosleep(get_smaller_timespec(&tsr, &lock->write_lock_interval_ts), NULL);
    }
  }
}

int atbuiltin_rwlock_wlock(atbuiltin_rwlock_t *lock)
{
  struct timespec interval;
  if (lock->write_lock_interval)
  {
    get_timespec_from_nanosec(&interval, lock->write_lock_interval);
  }
  if (pthread_mutex_trylock(&lock->mutex))
    pthread_mutex_lock(&lock->mutex);
  lock->write_waiting = true;
  while (true)
  {
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
    if (__sync_bool_compare_and_swap(&lock->lock_body, 0, LLONG_MIN))
#else
    if (__sync_bool_compare_and_swap(&lock->lock_body, 0, INT_MIN))
#endif
    {
      /* lock success */
      return 0;
    }
    if (lock->write_lock_interval)
    {
      nanosleep(&lock->write_lock_interval_ts, NULL);
    }
  }
}

int atbuiltin_rwlock_wunlock(atbuiltin_rwlock_t *lock)
{
  while (true)
  {
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
    if (__sync_bool_compare_and_swap(&lock->lock_body, LLONG_MIN, 0))
#else
    if (__sync_bool_compare_and_swap(&lock->lock_body, INT_MIN, 0))
#endif
    {
      lock->write_waiting = false;
      pthread_mutex_unlock(&lock->mutex);
      /* unlock success */
      return 0;
    }
  }
}