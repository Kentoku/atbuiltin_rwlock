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

pthread_rwlock_t rwlock;

void *worker_thread(void *arg)
{
  int i, ret;
  int worker_id = *((int *) arg);
/*  if ((worker_id % 100) < 100) *//* 100% write */
/*  if ((worker_id % 100) < 0) *//* 100% read */
  if ((worker_id % 100) < 10) /* 10% write 90% read */
  {
    for (i = 0; i < 1000000; i++)
    {
      if (!(ret = pthread_rwlock_wrlock(&rwlock)))
      {
        pthread_rwlock_unlock(&rwlock);
      } else {
        printf("write lock thread [%d] got %d\n", worker_id, ret);
      }
    }
  } else {
    for (i = 0; i < 1000000; i++)
    {
      if (!(ret = pthread_rwlock_rdlock(&rwlock)))
      {
        pthread_rwlock_unlock(&rwlock);
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
  int worker_id[100];
  int i;
  pthread_t threads[100];
  pthread_attr_t attr;
  pthread_rwlockattr_t rwlockattr;

  pthread_attr_init(&attr);

  if (pthread_rwlockattr_init(&rwlockattr) < 0) {
    printf("pthread_rwlockattr_init\n");
    return 1;
  }
  if (pthread_rwlockattr_setkind_np(&rwlockattr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) < 0) {
    printf("pthread_rwlockattr_setkind_np\n");
    return 1;
  }
  if (pthread_rwlock_init(&rwlock, &rwlockattr) < 0) {
    printf("pthread_rwlock_init\n");
    return 1;
  }

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  for (i = 0; i < 100; i++)
  {
    worker_id[i] = i;
    if (pthread_create(&threads[i], &attr, worker_thread, &worker_id[i]))
    {
      return 1;
    }
  }

  for (i = 0; i < 100; i++)
  {
    pthread_join(threads[i], NULL);
  }

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  pthread_rwlock_destroy(&rwlock);
  return 0;
}
