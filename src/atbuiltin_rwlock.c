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
  int ret;
  attr->rwlock_attr = ATBUILTIN_RWLOCK_READ_PRIORITY;
  attr->write_lock_interval = 0;
  if ((ret = pthread_condattr_init(&attr->cond_attr)))
    goto error_condattr_init;
  if ((ret = pthread_mutexattr_init(&attr->mutex_attr)))
    goto error_mutexattr_init;
  return 0;

error_mutexattr_init:
  pthread_condattr_destroy(&attr->cond_attr);
error_condattr_init:
  return ret;
}

int atbuiltin_rwlockattr_destroy(atbuiltin_rwlock_attr_t *attr)
{
  int ret1, ret2;
  ret1 = pthread_condattr_destroy(&attr->cond_attr);
  ret2 = pthread_mutexattr_destroy(&attr->mutex_attr);
  if (ret1)
    return ret1;
  return ret2;
}

int atbuiltin_rwlockattr_setpshared_cond(atbuiltin_rwlock_attr_t *attr, int pshared)
{
  return pthread_condattr_setpshared(&attr->cond_attr, pshared);
}

int atbuiltin_rwlockattr_getpshared_cond(atbuiltin_rwlock_attr_t *attr, int *pshared)
{
  return pthread_condattr_getpshared(&attr->cond_attr, pshared);
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
    case ATBUILTIN_RWLOCK_READ_PRIORITY:
    case ATBUILTIN_RWLOCK_NO_PRIORITY:
    case ATBUILTIN_RWLOCK_WRITE_PRIORITY:
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
  int ret;
  lock->lock_body = 0;
  lock->writer_count = 0;
  lock->write_waiting = false;
  if (attr)
  {
    lock->write_lock_interval = attr->write_lock_interval;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    if (attr->rwlock_attr == ATBUILTIN_RWLOCK_READ_PRIORITY)
    {
      lock->write_priority = false;
      lock->write_counting = false;
    } else {
      lock->write_priority = true;
      if (attr->rwlock_attr == ATBUILTIN_RWLOCK_WRITE_PRIORITY)
      {
        lock->write_counting = true;
      }
    }
    if ((ret = pthread_cond_init(&lock->cond, &attr->cond_attr)))
      goto error_cond_init;
    if ((ret = pthread_mutex_init(&lock->mutex, &attr->mutex_attr)))
      goto error_mutex_init;
  } else {
    lock->write_lock_interval = 0;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    lock->write_priority = false;
    lock->write_counting = false;
    if ((ret = pthread_cond_init(&lock->cond, NULL)))
      goto error_cond_init;
    if ((ret = pthread_mutex_init(&lock->mutex, NULL)))
      goto error_mutex_init;
  }
  return 0;

error_mutex_init:
  pthread_cond_destroy(&lock->cond);
error_cond_init:
  return ret;
}

int atbuiltin_rwlock_destroy(atbuiltin_rwlock_t *lock)
{
  int ret1, ret2;
  ret1 = pthread_cond_destroy(&lock->cond);
  ret2 = pthread_mutex_destroy(&lock->mutex);
  if (ret1)
    return ret1;
  return ret2;
}

int atbuiltin_rwlock_tryrlock(atbuiltin_rwlock_t *lock)
{
  int res;
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
  long long int cnt;
#else
  int cnt;
#endif
  if (lock->write_priority)
  {
    if (lock->write_waiting)
    {
      return EBUSY;
    }
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
  if (lock->write_priority)
  {
    while (lock->write_waiting)
    {
      if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
      {
        return res;
      }
      if (lock->write_waiting)
      {
        clock_gettime(CLOCK_MONOTONIC, &tsc);
        if (timespec_sub(&tsr, &tss, &tsc))
        {
          pthread_mutex_unlock(&lock->mutex);
          return ETIMEDOUT;
        }
        if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
        {
          if (res == ETIMEDOUT)
          {
            pthread_mutex_unlock(&lock->mutex);
            return ETIMEDOUT;
          }
        }
      }
      pthread_mutex_unlock(&lock->mutex);
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        return ETIMEDOUT;
      }
    }
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
    if (lock->write_waiting)
    {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        pthread_mutex_unlock(&lock->mutex);
        return ETIMEDOUT;
      }
      if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
      {
        if (res == ETIMEDOUT)
        {
          pthread_mutex_unlock(&lock->mutex);
          return ETIMEDOUT;
        }
      }
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
  if (lock->write_priority)
  {
    while (lock->write_waiting)
    {
      pthread_mutex_lock(&lock->mutex);
      if (lock->write_waiting)
      {
        pthread_cond_wait(&lock->cond, &lock->mutex);
      }
      pthread_mutex_unlock(&lock->mutex);
    }
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
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
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
    if (lock->write_counting)
    {
      __sync_add_and_fetch(&lock->writer_count, 1);
    }
    lock->write_waiting = true;
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
  if (lock->write_counting)
  {
    __sync_add_and_fetch(&lock->writer_count, 1);
    if (lock->write_waiting)
    {
      /* lock success */
      return 0;
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
      if (
        !lock->write_counting ||
        __sync_sub_and_fetch(&lock->writer_count, 1) == 0
      ) {
        lock->write_waiting = false;
        pthread_cond_broadcast(&lock->cond);
      }
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
  if (lock->write_counting)
  {
    __sync_add_and_fetch(&lock->writer_count, 1);
  }
  if (pthread_mutex_trylock(&lock->mutex))
  {
    pthread_mutex_lock(&lock->mutex);
  }
  if (
    lock->write_counting &&
    lock->write_waiting
  ) {
    /* lock success */
    return 0;
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
    if (lock->write_lock_interval)
    {
      nanosleep(&lock->write_lock_interval_ts, NULL);
    }
  }
}

int atbuiltin_rwlock_wunlock(atbuiltin_rwlock_t *lock)
{
  if (lock->write_counting)
  {
    if (__sync_sub_and_fetch(&lock->writer_count, 1) == 0)
    {
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
      while (!__sync_bool_compare_and_swap(&lock->lock_body, LLONG_MIN, 0))
#else
      while (!__sync_bool_compare_and_swap(&lock->lock_body, INT_MIN, 0))
#endif
      {
        ;
      }
      lock->write_waiting = false;
      pthread_cond_broadcast(&lock->cond);
    }
    pthread_mutex_unlock(&lock->mutex);
    /* unlock success */
    return 0;
  } else {
    while (true)
    {
#ifdef ATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY
      if (__sync_bool_compare_and_swap(&lock->lock_body, LLONG_MIN, 0))
#else
      if (__sync_bool_compare_and_swap(&lock->lock_body, INT_MIN, 0))
#endif
      {
        lock->write_waiting = false;
        pthread_cond_broadcast(&lock->cond);
        pthread_mutex_unlock(&lock->mutex);
        /* unlock success */
        return 0;
      }
    }
  }
}
