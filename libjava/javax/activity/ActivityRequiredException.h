
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_activity_ActivityRequiredException__
#define __javax_activity_ActivityRequiredException__

#pragma interface

#include <java/rmi/RemoteException.h>
extern "Java"
{
  namespace javax
  {
    namespace activity
    {
        class ActivityRequiredException;
    }
  }
}

class javax::activity::ActivityRequiredException : public ::java::rmi::RemoteException
{

public:
  ActivityRequiredException();
  ActivityRequiredException(::java::lang::String *);
  ActivityRequiredException(::java::lang::Throwable *);
  ActivityRequiredException(::java::lang::String *, ::java::lang::Throwable *);
  static ::java::lang::Class class$;
};

#endif // __javax_activity_ActivityRequiredException__
