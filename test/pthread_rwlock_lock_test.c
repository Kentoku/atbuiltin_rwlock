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
#include <pthread.h>

#define NUMBER_OF_THREADS 100
#define NUMBER_OF_LOOPS 1000000
/*
#define OPTION_OF_RWLOCKATTR PTHREAD_RWLOCK_PREFER_READER_NP
#define OPTION_OF_RWLOCKATTR PTHREAD_RWLOCK_PREFER_WRITER_NP
*/
#define OPTION_OF_RWLOCKATTR PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP

pthread_rwlock_t rwlock;
volatile bool rlocking;
volatile bool wlocking;

void *worker_thread(void *arg)
{
  int i, ret;
  int worker_id = *((int *) arg);
/*  if ((worker_id % NUMBER_OF_THREADS) < NUMBER_OF_THREADS) *//* 100% write */
/*  if ((worker_id % NUMBER_OF_THREADS) < 0) *//* 100% read */
  if ((worker_id % NUMBER_OF_THREADS) < NUMBER_OF_THREADS / 10) /* 10% write 90% read */
  {
    for (i = 0; i < NUMBER_OF_LOOPS; i++)
    {
      if (!(ret = pthread_rwlock_wrlock(&rwlock)))
      {
        wlocking = true;
        if (rlocking)
          printf("read locked after write locking\n");
        wlocking = false;
        pthread_rwlock_unlock(&rwlock);
      } else {
        printf("write lock thread [%d] got %d\n", worker_id, ret);
      }
    }
  } else {
    for (i = 0; i < NUMBER_OF_LOOPS; i++)
    {
      if (!(ret = pthread_rwlock_rdlock(&rwlock)))
      {
        rlocking = true;
        if (wlocking)
          printf("write locked after read locking\n");
        rlocking = false;
        pthread_rwlock_unlock(&rwlock);
      } else {
        printf("read lock thread [%d] got %d\n", worker_id, ret);
      }
    }
  }
  printf("%d is finished\n", worker_id);
}

int main(int argc, char **argv)
{
  time_t timer;
  struct tm *date;
  int worker_id[NUMBER_OF_THREADS];
  int i;
  pthread_t threads[NUMBER_OF_THREADS];
  pthread_attr_t attr;
  pthread_rwlockattr_t rwlockattr;

  pthread_attr_init(&attr);

  if (pthread_rwlockattr_init(&rwlockattr) < 0)
  {
    printf("pthread_rwlockattr_init\n");
    return 1;
  }
  if (pthread_rwlockattr_setkind_np(&rwlockattr, OPTION_OF_RWLOCKATTR) < 0)
  {
    printf("pthread_rwlockattr_setkind_np\n");
    return 1;
  }
  if (pthread_rwlock_init(&rwlock, &rwlockattr) < 0)
  {
    printf("pthread_rwlock_init\n");
    return 1;
  }

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  for (i = 0; i < NUMBER_OF_THREADS; i++)
  {
    worker_id[i] = i;
    if (pthread_create(&threads[i], &attr, worker_thread, &worker_id[i]))
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
  pthread_rwlock_destroy(&rwlock);
  return 0;
}
