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

atbuiltin_rwlock_t rwlock;
volatile bool rlocking;
volatile bool wlocking;

void *worker_thread(void *arg)
{
  int i;
  int worker_id = *((int *) arg);
  if ((worker_id % 100) < 10)
  {
    for (i = 0; i < 1000000; i++)
    {
      if (!atbuiltin_rwlock_wlock(&rwlock))
      {
        wlocking = true;
        if (rlocking)
          printf("read locked after write locking\n");
        wlocking = false;
        atbuiltin_rwlock_wunlock(&rwlock);
      } else {
        printf("write lock timeout %d\n", worker_id);
      }
    }
  } else {
    for (i = 0; i < 1000000; i++)
    {
      if (!atbuiltin_rwlock_rlock(&rwlock))
      {
        rlocking = true;
        if (wlocking)
          printf("write locked after read locking\n");
        rlocking = false;
        atbuiltin_rwlock_runlock(&rwlock);
      } else {
        printf("read lock timeout %d\n", worker_id);
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
  pthread_attr_t pthread_attr;
  atbuiltin_rwlock_attr_t attr;

  rlocking = false;
  wlocking = false;
  pthread_attr_init(&pthread_attr);
  atbuiltin_rwlockattr_init(&attr);
  atbuiltin_rwlockattr_settype_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  atbuiltin_rwlock_init(&rwlock, &attr);

  timer = time(NULL);
  printf("%s\n", ctime(&timer));
  for (i = 0; i < 100; i++)
  {
    worker_id[i] = i;
    if (pthread_create(&threads[i], &pthread_attr, worker_thread, &worker_id[i]))
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
  pthread_attr_destroy(&pthread_attr);
  atbuiltin_rwlock_destroy(&rwlock);
  atbuiltin_rwlockattr_destroy(&attr);
  return 0;
}
