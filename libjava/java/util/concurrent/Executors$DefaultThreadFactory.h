
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_util_concurrent_Executors$DefaultThreadFactory__
#define __java_util_concurrent_Executors$DefaultThreadFactory__

#pragma interface

#include <java/lang/Object.h>

class java::util::concurrent::Executors$DefaultThreadFactory : public ::java::lang::Object
{

public: // actually package-private
  Executors$DefaultThreadFactory();
public:
  virtual ::java::lang::Thread * newThread(::java::lang::Runnable *);
public: // actually package-private
  static ::java::util::concurrent::atomic::AtomicInteger * poolNumber;
  ::java::lang::ThreadGroup * __attribute__((aligned(__alignof__( ::java::lang::Object)))) group;
  ::java::util::concurrent::atomic::AtomicInteger * threadNumber;
  ::java::lang::String * namePrefix;
public:
  static ::java::lang::Class class$;
};

#endif // __java_util_concurrent_Executors$DefaultThreadFactory__
