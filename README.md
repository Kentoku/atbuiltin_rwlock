atbuiltin_rwlock
================

RW lock functions using atomic builtins

### Performance test results ###
##### Test machine's enviroments #####
* CPU: AMD Phenom(tm) II X6 1065T (6 core)
* MEMORY: 32GB
* OS: CentOS 6.3
* glibc: 2.12-1.132

Test source codes are in test directory.
##### Test results #####
* 100 threads 1000000 loops

1. pthread_rwlock (use PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
  * rlock 100%, wlock   0% : 23 seconds
  * rlock  90%, wlock  10% : 25 seconds
  * rlock   0%, wlock 100% : 34 seconds

2. atbuiltin_rwlock (use PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
  * rlock 100%, wlock   0% :  5 seconds
  * rlock  90%, wlock  10% :  7 seconds
  * rlock   0%, wlock 100% : 27 seconds

### Limitations ###
If over the following number of threads use this functions at same time, please set -DATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY option for building.
* 8bit: 127 threads
* 16bit: 32767 threads
* 32bit: 2147483647 threads

Limitations of 64bit and with -DATBUILTIN_RWLOCK_USE_LONG_LONG_FOR_LOCK_BODY are 9223372036854775807 threads.
