atbuiltin_rwlock
================

RW lock functions using atomic builtins

### Structs ###

* atbuiltin_rwlock_attr_t
The rwlock attribute object for initializing atbuiltin_rwlock_t.

* atbuiltin_rwlock_t
The rwlock object for initializing atbuiltin_rwlock_t.

### Functions ###

* int atbuiltin_rwlockattr_init(atbuiltin_rwlock_attr_t *attr);
This function is for initializing atbuiltin_rwlock_attr_t.

* int atbuiltin_rwlockattr_destroy(atbuiltin_rwlock_attr_t *attr);
This function is for destoroying atbuiltin_rwlock_attr_t.

* int atbuiltin_rwlockattr_setpshared_cond(atbuiltin_rwlock_attr_t *attr, int pshared);
This function is for setting the process-shared attribute in atbuiltin_rwlock_attr_t. You can set same value of pthread_condattr_setpshared for pshared.

* int atbuiltin_rwlockattr_getpshared_cond(atbuiltin_rwlock_attr_t *attr, int *pshared);
This function is for getting the process-shared attribute in atbuiltin_rwlock_attr_t. You will get same value of pthread_condattr_getpshared for pshared.

* int atbuiltin_rwlockattr_settype_mutex(atbuiltin_rwlock_attr_t *attr, int kind);
This function is for setting the mutex type attribute in atbuiltin_rwlock_attr_t. You can set same value of pthread_mutexattr_settype for kind.

* int atbuiltin_rwlockattr_gettype_mutex(atbuiltin_rwlock_attr_t *attr, int *kind);
This function is for getting the mutex type attribute in atbuiltin_rwlock_attr_t. You will get same value of pthread_mutexattr_gettype for kind.

* int atbuiltin_rwlockattr_settype_priority(atbuiltin_rwlock_attr_t *attr, int priority);
This function is for setting the priority attribute in atbuiltin_rwlock_attr_t. You can set the following value for priority.
1. ATBUILTIN_RWLOCK_READ_PRIORITY
2. ATBUILTIN_RWLOCK_NO_PRIORITY
3. ATBUILTIN_RWLOCK_WRITE_PRIORITY

* int atbuiltin_rwlockattr_gettype_priority(atbuiltin_rwlock_attr_t *attr, int *priority);
This function is for getting the priority attribute in atbuiltin_rwlock_attr_t. You will get the following value for priority.
1. ATBUILTIN_RWLOCK_READ_PRIORITY
2. ATBUILTIN_RWLOCK_NO_PRIORITY
3. ATBUILTIN_RWLOCK_WRITE_PRIORITY

* int atbuiltin_rwlockattr_settype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int interval);
This function is for setting the interval attribute in atbuiltin_rwlock_attr_t. You can set nanosecond for interval.

* int atbuiltin_rwlockattr_gettype_write_lock_interval(atbuiltin_rwlock_attr_t *attr, unsigned long long int *interval);
This function is for getting the interval attribute in atbuiltin_rwlock_attr_t. You will get nanosecond for interval.

* int atbuiltin_rwlock_init(atbuiltin_rwlock_t *lock, const atbuiltin_rwlock_attr_t *attr);
This function is for initializing atbuiltin_rwlock_t.

* int atbuiltin_rwlock_destroy(atbuiltin_rwlock_t *lock);
This function is for destoroying atbuiltin_rwlock_t.

* int atbuiltin_rwlock_tryrlock(atbuiltin_rwlock_t *lock);
This function is for getting read lock. If it does not get lock immediately, it returns EBUSY. Return value of this function is same of pthread_mutex_trylock.

* int atbuiltin_rwlock_timedrlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
This function is for getting read lock with timeout. If it does not get lock before timeout, it returns ETIMEDOUT. Return value of this function is same of pthread_mutex_timedlock.

* int atbuiltin_rwlock_rlock(atbuiltin_rwlock_t *lock);
This function is for getting read lock. Return value of this function is same of pthread_mutex_lock.

* int atbuiltin_rwlock_runlock(atbuiltin_rwlock_t *lock);
This function is for releasing read lock. Return value of this function is same of pthread_mutex_unlock.

* int atbuiltin_rwlock_trywlock(atbuiltin_rwlock_t *lock);
This function is for getting write lock. If it does not get lock immediately, it returns EBUSY. Return value of this function is same of pthread_mutex_trylock.

* int atbuiltin_rwlock_timedwlock(atbuiltin_rwlock_t *lock, const struct timespec *timeout);
This function is for getting write lock with timeout. If it does not get lock before timeout, it returns ETIMEDOUT. Return value of this function is same of pthread_mutex_timedlock.

* int atbuiltin_rwlock_wlock(atbuiltin_rwlock_t *lock);
This function is for getting write lock. Return value of this function is same of pthread_mutex_lock.

* int atbuiltin_rwlock_wunlock(atbuiltin_rwlock_t *lock);
This function is for releasing write lock. Return value of this function is same of pthread_mutex_unlock.

### Performance test results ###
##### Test machine's enviroments #####
* CPU: AMD Phenom(tm) II X6 1065T (6 core)
* MEMORY: 32GB
* OS: CentOS 6.3
* glibc: 2.12-1.132

Test source codes are in test directory.
##### Test results #####
* 100 threads 1000000 loops

1. atbuiltin_rwlock (use ATBUILTIN_RWLOCK_READ_PRIORITY)
  * rlock 100%, wlock   0% :  9 seconds
  * rlock  90%, wlock  10% : 15 seconds
  * rlock   0%, wlock 100% : 30 seconds

2. atbuiltin_rwlock (use ATBUILTIN_RWLOCK_NO_PRIORITY)
  * rlock 100%, wlock   0% :  8 seconds
  * rlock  90%, wlock  10% : 15 seconds
  * rlock   0%, wlock 100% : 29 seconds

3. atbuiltin_rwlock (use ATBUILTIN_RWLOCK_WRITE_PRIORITY)
  * rlock 100%, wlock   0% :  7 seconds
  * rlock  90%, wlock  10% :  9 seconds
  * rlock   0%, wlock 100% : 27 seconds

4. pthread_rwlock (use PTHREAD_RWLOCK_PREFER_READER_NP)
  * rlock 100%, wlock   0% : 23 seconds
  * rlock  90%, wlock  10% : 25 seconds
  * rlock   0%, wlock 100% : 33 seconds

5. pthread_rwlock (use PTHREAD_RWLOCK_PREFER_WRITER_NP)
  * rlock 100%, wlock   0% : 24 seconds
  * rlock  90%, wlock  10% : 24 seconds
  * rlock   0%, wlock 100% : 31 seconds

6. pthread_rwlock (use PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
  * rlock 100%, wlock   0% : 23 seconds
  * rlock  90%, wlock  10% : 25 seconds
  * rlock   0%, wlock 100% : 34 seconds

### Limitations ###
If over the following number of threads use this functions at same time, please set -DATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY option for building.
* 8bit: 127 threads
* 16bit: 32767 threads
* 32bit: 2147483647 threads

Limitations of 64bit and with -DATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY are 9223372036854775807 threads.
