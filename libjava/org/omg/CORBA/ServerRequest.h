
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __org_omg_CORBA_ServerRequest__
#define __org_omg_CORBA_ServerRequest__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace org
  {
    namespace omg
    {
      namespace CORBA
      {
          class Any;
          class Context;
          class NVList;
          class ServerRequest;
      }
    }
  }
}

class org::omg::CORBA::ServerRequest : public ::java::lang::Object
{

public:
  ServerRequest();
  virtual ::org::omg::CORBA::Context * ctx() = 0;
  virtual ::java::lang::String * operation();
  virtual void arguments(::org::omg::CORBA::NVList *);
  virtual void set_result(::org::omg::CORBA::Any *);
  virtual void set_exception(::org::omg::CORBA::Any *);
  virtual void result(::org::omg::CORBA::Any *);
  virtual void except(::org::omg::CORBA::Any *);
  virtual void params(::org::omg::CORBA::NVList *);
  virtual ::java::lang::String * op_name();
  static ::java::lang::Class class$;
};

#endif // __org_omg_CORBA_ServerRequest__
