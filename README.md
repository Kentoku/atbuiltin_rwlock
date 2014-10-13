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
  * rlock 100%, wlock   0% :  7 seconds
  * rlock  90%, wlock  10% : 10 seconds
  * rlock   0%, wlock 100% : 30 seconds
