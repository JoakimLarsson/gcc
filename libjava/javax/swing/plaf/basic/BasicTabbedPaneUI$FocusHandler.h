
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_swing_plaf_basic_BasicTabbedPaneUI$FocusHandler__
#define __javax_swing_plaf_basic_BasicTabbedPaneUI$FocusHandler__

#pragma interface

#include <java/awt/event/FocusAdapter.h>
extern "Java"
{
  namespace java
  {
    namespace awt
    {
      namespace event
      {
          class FocusEvent;
      }
    }
  }
  namespace javax
  {
    namespace swing
    {
      namespace plaf
      {
        namespace basic
        {
            class BasicTabbedPaneUI;
            class BasicTabbedPaneUI$FocusHandler;
        }
      }
    }
  }
}

class javax::swing::plaf::basic::BasicTabbedPaneUI$FocusHandler : public ::java::awt::event::FocusAdapter
{

public:
  BasicTabbedPaneUI$FocusHandler(::javax::swing::plaf::basic::BasicTabbedPaneUI *);
  virtual void focusGained(::java::awt::event::FocusEvent *);
  virtual void focusLost(::java::awt::event::FocusEvent *);
public: // actually package-private
  ::javax::swing::plaf::basic::BasicTabbedPaneUI * __attribute__((aligned(__alignof__( ::java::awt::event::FocusAdapter)))) this$0;
public:
  static ::java::lang::Class class$;
};

#endif // __javax_swing_plaf_basic_BasicTabbedPaneUI$FocusHandler__
