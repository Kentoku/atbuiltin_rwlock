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
#include <atbuiltin_rwlock.h>

static int atbuiltin_rwlock_timedrlock_read_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_rlock_read_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_timedwlock_read_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_wlock_read_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_wunlock_read_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_timedrlock_no_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_rlock_no_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_timedwlock_no_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_wlock_no_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_wunlock_no_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_timedrlock_write_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_rlock_write_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_timedwlock_write_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
static int atbuiltin_rwlock_wlock_write_priority(atbuiltin_rwlock_t *lock);
static int atbuiltin_rwlock_wunlock_write_priority(atbuiltin_rwlock_t *lock);

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

#ifdef ATBUILTIN_RWLOCK_WITHOUT_SPIN_LOCK
static inline int atbuiltin_spin_timedlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  if ((res = pthread_mutex_timedlock(&lock->mutex, timeout)))
  {
    return res;
  }
  return 0;
}

static inline void atbuiltin_spin_lock(atbuiltin_rwlock_t *lock)
{
  pthread_mutex_lock(&lock->mutex);
}
#else
#define ATBUILTIN_RWLOCK_SPIN_LOOPS 7
static inline int atbuiltin_spin_timedlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  unsigned int i;
  if (pthread_mutex_trylock(&lock->mutex))
  {
    for (i = 0; i < ATBUILTIN_RWLOCK_SPIN_LOOPS; i++)
    {
      if (!pthread_mutex_trylock(&lock->mutex))
      {
        break;
      }
    }
    if (i == ATBUILTIN_RWLOCK_SPIN_LOOPS)
    {
      if ((res = pthread_mutex_timedlock(&lock->mutex, timeout)))
      {
        return res;
      }
    }
  }
  return 0;
}

static inline void atbuiltin_spin_lock(atbuiltin_rwlock_t *lock)
{
  unsigned int i;
  if (pthread_mutex_trylock(&lock->mutex))
  {
    for (i = 0; i < ATBUILTIN_RWLOCK_SPIN_LOOPS; i++)
    {
      if (!pthread_mutex_trylock(&lock->mutex))
      {
        break;
      }
    }
    if (i == ATBUILTIN_RWLOCK_SPIN_LOOPS)
    {
      pthread_mutex_lock(&lock->mutex);
    }
  }
}
#endif

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

int atbuiltin_rwlockattr_settype_priority(atbuiltin_rwlock_attr_t *attr, int priority)
{
  switch (priority)
  {
    case ATBUILTIN_RWLOCK_READ_PRIORITY:
    case ATBUILTIN_RWLOCK_NO_PRIORITY:
    case ATBUILTIN_RWLOCK_WRITE_PRIORITY:
      break;
    default:
      return EINVAL;
  }
  attr->rwlock_attr = priority;
  return 0;
}

int atbuiltin_rwlockattr_gettype_priority(atbuiltin_rwlock_attr_t *attr, int *priority)
{
  *priority = attr->rwlock_attr;
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
  lock->tr_waiter_count = 0;
  lock->read_waiting = false;
  lock->write_waiting = false;
  if (attr)
  {
    lock->write_lock_interval = attr->write_lock_interval;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    if (attr->rwlock_attr == ATBUILTIN_RWLOCK_READ_PRIORITY)
    {
      lock->timedrlock = atbuiltin_rwlock_timedrlock_read_priority;
      lock->rlock = atbuiltin_rwlock_rlock_read_priority;
      lock->timedwlock = atbuiltin_rwlock_timedwlock_read_priority;
      lock->wlock = atbuiltin_rwlock_wlock_read_priority;
      lock->wunlock = atbuiltin_rwlock_wunlock_read_priority;
    } else {
      if (attr->rwlock_attr == ATBUILTIN_RWLOCK_WRITE_PRIORITY)
      {
        lock->timedrlock = atbuiltin_rwlock_timedrlock_write_priority;
        lock->rlock = atbuiltin_rwlock_rlock_write_priority;
        lock->timedwlock = atbuiltin_rwlock_timedwlock_write_priority;
        lock->wlock = atbuiltin_rwlock_wlock_write_priority;
        lock->wunlock = atbuiltin_rwlock_wunlock_write_priority;
      } else {
        lock->timedrlock = atbuiltin_rwlock_timedrlock_no_priority;
        lock->rlock = atbuiltin_rwlock_rlock_no_priority;
        lock->timedwlock = atbuiltin_rwlock_timedwlock_no_priority;
        lock->wlock = atbuiltin_rwlock_wlock_no_priority;
        lock->wunlock = atbuiltin_rwlock_wunlock_no_priority;
      }
    }
    if ((ret = pthread_cond_init(&lock->cond, &attr->cond_attr)))
      goto error_cond_init;
    if ((ret = pthread_mutex_init(&lock->mutex, &attr->mutex_attr)))
      goto error_mutex_init;
  } else {
    lock->write_lock_interval = 0;
    get_timespec_from_nanosec(&lock->write_lock_interval_ts, lock->write_lock_interval);
    lock->timedrlock = atbuiltin_rwlock_timedrlock_read_priority;
    lock->rlock = atbuiltin_rwlock_rlock_read_priority;
    lock->timedwlock = atbuiltin_rwlock_timedwlock_read_priority;
    lock->wlock = atbuiltin_rwlock_wlock_read_priority;
    lock->wunlock = atbuiltin_rwlock_wunlock_read_priority;
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
  atbuiltin_rwlock_signed cnt;
  if (lock->write_waiting)
  {
    return EBUSY;
  }
  cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
  if (cnt > 0)
  {
    /* lock success */
    return 0;
  }
  atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
  return EBUSY;
}

/*
int atbuiltin_rwlock_timedrlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  return lock->timedrlock(lock, timeout);
}

int atbuiltin_rwlock_rlock(atbuiltin_rwlock_t *lock)
{
  return lock->rlock(lock);
}
*/

int atbuiltin_rwlock_runlock(atbuiltin_rwlock_t *lock)
{
  atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
  return 0;
}

int atbuiltin_rwlock_trywlock(atbuiltin_rwlock_t *lock)
{
  int ret;
  if ((ret = pthread_mutex_trylock(&lock->mutex)))
    return ret;
  if (lock->write_waiting)
  {
    atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
    /* lock success */
    return 0;
  }
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
    lock->write_waiting = true;
    /* lock success */
    return 0;
  }
  pthread_mutex_unlock(&lock->mutex);
  return EBUSY;
}

/*
int atbuiltin_rwlock_timedwlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  return lock->timedwlock(lock, timeout);
}

int atbuiltin_rwlock_wlock(atbuiltin_rwlock_t *lock)
{
  return lock->wlock(lock);
}

int atbuiltin_rwlock_wunlock(atbuiltin_rwlock_t *lock)
{
  return lock->wunlock(lock);
}
*/

static int atbuiltin_rwlock_timedrlock_read_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  atbuiltin_rwlock_signed cnt;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  atbuiltin_add_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  if (lock->write_waiting)
  {
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
      return ETIMEDOUT;
    }
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
      return res;
    }
    if (lock->write_waiting)
    {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        pthread_mutex_unlock(&lock->mutex);
        atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
        return ETIMEDOUT;
      }
      if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
      {
        if (res == ETIMEDOUT)
        {
          pthread_mutex_unlock(&lock->mutex);
          atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
          return ETIMEDOUT;
        }
      }
    }
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
      /* lock success */
      return 0;
    }
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return ETIMEDOUT;
    }
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return res;
    }
    if (lock->write_waiting)
    {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        pthread_mutex_unlock(&lock->mutex);
        atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
          ATBUILTIN_RWLOCK_RELAXED);
        return ETIMEDOUT;
      }
      if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
      {
        if (res == ETIMEDOUT)
        {
          pthread_mutex_unlock(&lock->mutex);
          atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
            ATBUILTIN_RWLOCK_RELAXED);
          return ETIMEDOUT;
        }
      }
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

static int atbuiltin_rwlock_rlock_read_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_rwlock_signed cnt;
  if (lock->write_waiting)
  {
    lock->read_waiting = true;
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      lock->read_waiting = false;
      /* lock success */
      return 0;
    }
    lock->read_waiting = true;
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

static int atbuiltin_rwlock_timedwlock_read_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  if ((res = atbuiltin_spin_timedlock(lock, &tsr)))
  {
    return res;
  }
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    lock->write_waiting = true;
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
    {
      lock->write_waiting = true;
      /* lock success */
      return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      lock->write_waiting = false;
      if (
        atbuiltin_sub_and_fetch(&lock->writer_count, 1,
          ATBUILTIN_RWLOCK_RELAXED) == 0 ||
        lock->read_waiting ||
        lock->tr_waiter_count
      ) {
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

static int atbuiltin_rwlock_wlock_read_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  atbuiltin_spin_lock(lock);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    lock->write_waiting = true;
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
    {
      lock->write_waiting = true;
      /* lock success */
      return 0;
    }
    if (lock->write_lock_interval)
    {
      nanosleep(&lock->write_lock_interval_ts, NULL);
    }
  }
}

static int atbuiltin_rwlock_wunlock_read_priority(atbuiltin_rwlock_t *lock)
{
  if (
    atbuiltin_sub_and_fetch(&lock->writer_count, 1,
      ATBUILTIN_RWLOCK_RELAXED) == 0 ||
    lock->read_waiting ||
    lock->tr_waiter_count
  ) {
    register atbuiltin_rwlock_signed min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    while (!atbuiltin_compare_and_swap_n(&lock->lock_body, &min_val, 0,
      ATBUILTIN_RWLOCK_CAS_WEAK, ATBUILTIN_RWLOCK_RELAXED,
      ATBUILTIN_RWLOCK_RELAXED))
    {
      min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    }
    lock->write_waiting = false;
    pthread_cond_broadcast(&lock->cond);
  }
  pthread_mutex_unlock(&lock->mutex);
  /* unlock success */
  return 0;
}

static int atbuiltin_rwlock_timedrlock_no_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  atbuiltin_rwlock_signed cnt;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  atbuiltin_add_and_fetch(&lock->tr_waiter_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  while (lock->write_waiting)
  {
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return res;
    }
    if (lock->write_waiting)
    {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        pthread_mutex_unlock(&lock->mutex);
        atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
          ATBUILTIN_RWLOCK_RELAXED);
        return ETIMEDOUT;
      }
      if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
      {
        if (res == ETIMEDOUT)
        {
          pthread_mutex_unlock(&lock->mutex);
          atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
            ATBUILTIN_RWLOCK_RELAXED);
          return ETIMEDOUT;
        }
      }
    }
    pthread_mutex_unlock(&lock->mutex);
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return ETIMEDOUT;
    }
  }
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      /* lock success */
      return 0;
    }
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return ETIMEDOUT;
    }
    if ((res = pthread_mutex_timedlock(&lock->mutex, &tsr)))
    {
      atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
        ATBUILTIN_RWLOCK_RELAXED);
      return res;
    }
    if (lock->write_waiting)
    {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      if (timespec_sub(&tsr, &tss, &tsc))
      {
        pthread_mutex_unlock(&lock->mutex);
        atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
          ATBUILTIN_RWLOCK_RELAXED);
        return ETIMEDOUT;
      }
      if ((res = pthread_cond_timedwait(&lock->cond, &lock->mutex, &tsr)))
      {
        if (res == ETIMEDOUT)
        {
          pthread_mutex_unlock(&lock->mutex);
          atbuiltin_sub_and_fetch(&lock->tr_waiter_count, 1,
            ATBUILTIN_RWLOCK_RELAXED);
          return ETIMEDOUT;
        }
      }
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

static int atbuiltin_rwlock_rlock_no_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_rwlock_signed cnt;
  while (lock->write_waiting)
  {
    lock->read_waiting = true;
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      lock->read_waiting = false;
      /* lock success */
      return 0;
    }
    lock->read_waiting = true;
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

static int atbuiltin_rwlock_timedwlock_no_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  if ((res = atbuiltin_spin_timedlock(lock, &tsr)))
  {
    return res;
  }
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  lock->write_waiting = true;
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
    {
      /* lock success */
      return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      lock->write_waiting = false;
      if (
        atbuiltin_sub_and_fetch(&lock->writer_count, 1,
          ATBUILTIN_RWLOCK_RELAXED) == 0 ||
        lock->read_waiting ||
        lock->tr_waiter_count
      ) {
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

static int atbuiltin_rwlock_wlock_no_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  atbuiltin_spin_lock(lock);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  lock->write_waiting = true;
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
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

static int atbuiltin_rwlock_wunlock_no_priority(atbuiltin_rwlock_t *lock)
{
  if (
    atbuiltin_sub_and_fetch(&lock->writer_count, 1,
      ATBUILTIN_RWLOCK_RELAXED) == 0 ||
    lock->read_waiting ||
    lock->tr_waiter_count
  ) {
    register atbuiltin_rwlock_signed min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    while (!atbuiltin_compare_and_swap_n(&lock->lock_body, &min_val, 0,
      ATBUILTIN_RWLOCK_CAS_WEAK, ATBUILTIN_RWLOCK_RELAXED,
      ATBUILTIN_RWLOCK_RELAXED))
    {
      min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    }
    lock->write_waiting = false;
    pthread_cond_broadcast(&lock->cond);
  }
  pthread_mutex_unlock(&lock->mutex);
  /* unlock success */
  return 0;
}

static int atbuiltin_rwlock_timedrlock_write_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  atbuiltin_rwlock_signed cnt;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
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
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      /* lock success */
      return 0;
    }
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
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

static int atbuiltin_rwlock_rlock_write_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_rwlock_signed cnt;
  while (lock->write_waiting)
  {
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
  while (true)
  {
    cnt = atbuiltin_add_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    if (cnt > 0)
    {
      /* lock success */
      return 0;
    }
    atbuiltin_sub_and_fetch(&lock->lock_body, 1, ATBUILTIN_RWLOCK_RELAXED);
    pthread_mutex_lock(&lock->mutex);
    if (lock->write_waiting)
    {
      pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    pthread_mutex_unlock(&lock->mutex);
  }
}

static int atbuiltin_rwlock_timedwlock_write_priority(atbuiltin_rwlock_t *lock, const struct timespec *timeout)
{
  int res;
  struct timespec tss, tsc, tsr;
  tsr = *timeout;
  clock_gettime(CLOCK_MONOTONIC, &tss);
  if ((res = atbuiltin_spin_timedlock(lock, &tsr)))
  {
    return res;
  }
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  lock->write_waiting = true;
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
    {
      /* lock success */
      return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &tsc);
    if (timespec_sub(&tsr, &tss, &tsc))
    {
      lock->write_waiting = false;
      if (atbuiltin_sub_and_fetch(&lock->writer_count, 1,
        ATBUILTIN_RWLOCK_RELAXED) == 0)
      {
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

static int atbuiltin_rwlock_wlock_write_priority(atbuiltin_rwlock_t *lock)
{
  atbuiltin_add_and_fetch(&lock->writer_count, 1, ATBUILTIN_RWLOCK_RELAXED);
  atbuiltin_spin_lock(lock);
  if (lock->write_waiting)
  {
    /* lock success */
    return 0;
  }
  lock->write_waiting = true;
  register atbuiltin_rwlock_signed zero_val = 0;
  if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
    ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
    ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
  {
    /* lock success */
    return 0;
  }
  while (true)
  {
    zero_val = 0;
    if (atbuiltin_compare_and_swap_n(&lock->lock_body, &zero_val,
      ATBUILTIN_RWLOCK_MIN_VAL, ATBUILTIN_RWLOCK_CAS_WEAK,
      ATBUILTIN_RWLOCK_RELAXED, ATBUILTIN_RWLOCK_RELAXED))
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

static int atbuiltin_rwlock_wunlock_write_priority(atbuiltin_rwlock_t *lock)
{
  if (atbuiltin_sub_and_fetch(&lock->writer_count, 1,
    ATBUILTIN_RWLOCK_RELAXED) == 0)
  {
    register atbuiltin_rwlock_signed min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    while (!atbuiltin_compare_and_swap_n(&lock->lock_body, &min_val, 0,
      ATBUILTIN_RWLOCK_CAS_WEAK, ATBUILTIN_RWLOCK_RELAXED,
      ATBUILTIN_RWLOCK_RELAXED))
    {
      min_val = ATBUILTIN_RWLOCK_MIN_VAL;
    }
    lock->write_waiting = false;
    pthread_cond_broadcast(&lock->cond);
  }
  pthread_mutex_unlock(&lock->mutex);
  /* unlock success */
  return 0;
}
