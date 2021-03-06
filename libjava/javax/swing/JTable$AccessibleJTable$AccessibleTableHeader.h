
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_swing_JTable$AccessibleJTable$AccessibleTableHeader__
#define __javax_swing_JTable$AccessibleJTable$AccessibleTableHeader__

#pragma interface

#include <java/lang/Object.h>
#include <gcj/array.h>

extern "Java"
{
  namespace javax
  {
    namespace accessibility
    {
        class Accessible;
        class AccessibleTable;
    }
    namespace swing
    {
        class JTable$AccessibleJTable;
        class JTable$AccessibleJTable$AccessibleTableHeader;
      namespace table
      {
          class JTableHeader;
      }
    }
  }
}

class javax::swing::JTable$AccessibleJTable$AccessibleTableHeader : public ::java::lang::Object
{

  JTable$AccessibleJTable$AccessibleTableHeader(::javax::swing::JTable$AccessibleJTable *, ::javax::swing::table::JTableHeader *);
public:
  virtual ::javax::accessibility::Accessible * getAccessibleCaption();
  virtual void setAccessibleCaption(::javax::accessibility::Accessible *);
  virtual ::javax::accessibility::Accessible * getAccessibleSummary();
  virtual void setAccessibleSummary(::javax::accessibility::Accessible *);
  virtual jint getAccessibleRowCount();
  virtual jint getAccessibleColumnCount();
  virtual ::javax::accessibility::Accessible * getAccessibleAt(jint, jint);
  virtual jint getAccessibleRowExtentAt(jint, jint);
  virtual jint getAccessibleColumnExtentAt(jint, jint);
  virtual ::javax::accessibility::AccessibleTable * getAccessibleRowHeader();
  virtual void setAccessibleRowHeader(::javax::accessibility::AccessibleTable *);
  virtual ::javax::accessibility::AccessibleTable * getAccessibleColumnHeader();
  virtual void setAccessibleColumnHeader(::javax::accessibility::AccessibleTable *);
  virtual ::javax::accessibility::Accessible * getAccessibleRowDescription(jint);
  virtual void setAccessibleRowDescription(jint, ::javax::accessibility::Accessible *);
  virtual ::javax::accessibility::Accessible * getAccessibleColumnDescription(jint);
  virtual void setAccessibleColumnDescription(jint, ::javax::accessibility::Accessible *);
  virtual jboolean isAccessibleSelected(jint, jint);
  virtual jboolean isAccessibleRowSelected(jint);
  virtual jboolean isAccessibleColumnSelected(jint);
  virtual JArray< jint > * getSelectedAccessibleRows();
  virtual JArray< jint > * getSelectedAccessibleColumns();
public: // actually package-private
  JTable$AccessibleJTable$AccessibleTableHeader(::javax::swing::JTable$AccessibleJTable *, ::javax::swing::table::JTableHeader *, ::javax::swing::JTable$AccessibleJTable$AccessibleTableHeader *);
private:
  ::javax::swing::table::JTableHeader * __attribute__((aligned(__alignof__( ::java::lang::Object)))) header;
public: // actually package-private
  ::javax::swing::JTable$AccessibleJTable * this$1;
public:
  static ::java::lang::Class class$;
};

#endif // __javax_swing_JTable$AccessibleJTable$AccessibleTableHeader__
