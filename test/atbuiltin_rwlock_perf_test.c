/*
  Tests of atbuiltin RW lock functions

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

#include <stdio.h>
#include <time.h>
#include <atbuiltin_rwlock.h>

#define NUMBER_OF_THREADS 100
#define NUMBER_OF_LOOPS 1000000

#ifdef ATBUILTIN_RWLOCK_READ_PRIORITY_TEST
#define OPTION_OF_RWLOCKATTR ATBUILTIN_RWLOCK_READ_PRIORITY
#else
#ifdef ATBUILTIN_RWLOCK_NO_PRIORITY_TEST
#define OPTION_OF_RWLOCKATTR ATBUILTIN_RWLOCK_NO_PRIORITY
#else
#define OPTION_OF_RWLOCKATTR ATBUILTIN_RWLOCK_WRITE_PRIORITY
#endif
#endif

atbuiltin_rwlock_t rwlock;

void *worker_thread(void *arg)
{
  int i, ret;
  int worker_id = *((int *) arg);
#ifdef ATBUILTIN_RWLOCK_W100_TEST
  if ((worker_id % NUMBER_OF_THREADS) < NUMBER_OF_THREADS) /* 100% write */
#else
#ifdef ATBUILTIN_RWLOCK_R100_TEST
  if ((worker_id % NUMBER_OF_THREADS) < 0) /* 100% read */
#else
  if ((worker_id % NUMBER_OF_THREADS) < NUMBER_OF_THREADS / 10) /* 10% write 90% read */
#endif
#endif
  {
    for (i = 0; i < NUMBER_OF_LOOPS; i++)
    {
      if (!(ret = atbuiltin_rwlock_wlock(&rwlock)))
      {
        atbuiltin_rwlock_wunlock(&rwlock);
      } else {
        printf("write lock thread [%d] got %d\n", worker_id, ret);
      }
    }
  } else {
    for (i = 0; i < NUMBER_OF_LOOPS; i++)
    {
      if (!(ret = atbuiltin_rwlock_rlock(&rwlock)))
      {
        atbuiltin_rwlock_runlock(&rwlock);
      } else {
        printf("read lock thread [%d] got %d\n", worker_id, ret);
      }
    }
  }
}

int main(int argc, char **argv)
{
  time_t timer;
  struct tm *date;
  int worker_id[NUMBER_OF_THREADS];
  int i;
  pthread_t threads[NUMBER_OF_THREADS];
  pthread_attr_t pthread_attr;
  atbuiltin_rwlock_attr_t attr;

  pthread_attr_init(&pthread_attr);
  atbuiltin_rwlockattr_init(&attr);
  atbuiltin_rwlockattr_settype_priority(&attr, OPTION_OF_RWLOCKATTR);
  atbuiltin_rwlock_init(&rwlock, &attr);

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  for (i = 0; i < NUMBER_OF_THREADS; i++)
  {
    worker_id[i] = i;
    if (pthread_create(&threads[i], &pthread_attr, worker_thread, &worker_id[i]))
    {
      return 1;
    }
  }

  for (i = 0; i < NUMBER_OF_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  pthread_attr_destroy(&pthread_attr);
  atbuiltin_rwlock_destroy(&rwlock);
  atbuiltin_rwlockattr_destroy(&attr);
  return 0;
}
