/* sun.reflect.annotation.AnnotationInvocationHandler
   Copyright (C) 2006, 2007
   Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */

package sun.reflect.annotation;

import java.io.Serializable;
import java.lang.annotation.Annotation;
import java.lang.annotation.AnnotationTypeMismatchException;
import java.lang.annotation.IncompleteAnnotationException;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.Iterator;
import java.util.Map;

/**
 * This class exists for serialization compatibility with the JDK.
 * VMs can choose to implement annotations by constructing proxies
 * with this invocation handler, but that is not required.
 * If a different strategy for proxy objects is chosen, they can
 * have a writeReplace method to substitute a Proxy based on this
 * invocation handler is used for serialization.
 */
public final class AnnotationInvocationHandler
  implements InvocationHandler, Serializable
{
  private static final long serialVersionUID = 6182022883658399397L;
  private final Class<? extends Annotation> type;
  private final Map<String, ?> memberValues;

  /**
   * Construct a new invocation handler for an annotation proxy.
   * Note that the VM is responsible for filling the memberValues map
   * with the default values of all the annotation members.
   */
  public AnnotationInvocationHandler(Class<? extends Annotation> type, Map memberValues)
  {
    this.type = type;
    this.memberValues = (Map<String, ?>)memberValues;
  }

  public static Annotation create(Class<? extends Annotation> type, Map memberValues)
  {
    for (Method m : type.getDeclaredMethods())
      {
	String name = m.getName();
	if (! memberValues.containsKey(name))
	  {
	    // FIXME: what to do about exceptions here?
	    memberValues.put(name, m.getDefaultValue());
	  }
      }
    AnnotationInvocationHandler handler
      = new AnnotationInvocationHandler(type, memberValues);
    return (Annotation) Proxy.newProxyInstance(type.getClassLoader(),
					       new Class[] { type },
					       handler);
  }

  /**
   * Compare an instance of AnnotationInvocationHandler with another object.
   * Note that the other object does not have to be an
   * AnnotationInvocationHandler, any implementation of the annotation
   * interface is allowed to be compared for equality.
   * Note that this makes the equals method asymmetric, but this behavior
   * is specified by Annotation.equals and identical to the JDK.
   *
   * This method is public for use by other parts of the VM. Some VMs
   * (can) use different representations of annotations that reuse this
   * method.
   */
  public boolean equals(Object proxy, Object other)
  {
    if (type.isInstance(other))
      {
	try
	  {
	    Method[] methods = type.getDeclaredMethods();
	    if (methods.length == memberValues.size())
	      {
		for (int i = 0; i < methods.length; i++)
		  {
		    String key = methods[i].getName();
		    Object val = methods[i].invoke(other, (Object[])null);
		    Object thisVal
		      = invoke(proxy,
			       methods[i],
			       (Object[])null);
		    if (! deepEquals(thisVal, val))
		      {
			return false;
		      }
		  }
		return true;
	      }
	  }
	catch (Throwable _)
	  {
	    // Ignore exception, like the JDK
	  }
      }
    return false;
  }

  private static boolean deepEquals(Object o1, Object o2)
  {
    if (o1 == o2)
      return true;

    if (o1 == null || o2 == null)
      return false;

    if (o1 instanceof boolean[] && o2 instanceof boolean[])
      return Arrays.equals((boolean[]) o1, (boolean[]) o2);

    if (o1 instanceof byte[] && o2 instanceof byte[])
      return Arrays.equals((byte[]) o1, (byte[]) o2);

    if (o1 instanceof char[] && o2 instanceof char[])
      return Arrays.equals((char[]) o1, (char[]) o2);

    if (o1 instanceof short[] && o2 instanceof short[])
      return Arrays.equals((short[]) o1, (short[]) o2);

    if (o1 instanceof int[] && o2 instanceof int[])
      return Arrays.equals((int[]) o1, (int[]) o2);

    if (o1 instanceof float[] && o2 instanceof float[])
      return Arrays.equals((float[]) o1, (float[]) o2);

    if (o1 instanceof long[] && o2 instanceof long[])
      return Arrays.equals((long[]) o1, (long[]) o2);

    if (o1 instanceof double[] && o2 instanceof double[])
      return Arrays.equals((double[]) o1, (double[]) o2);

    if (o1 instanceof Object[] && o2 instanceof Object[])
      return Arrays.equals((Object[]) o1, (Object[]) o2);

    return o1.equals(o2);
  }

  private static int deepHashCode(Object obj)
  {
    if (obj instanceof boolean[])
      return Arrays.hashCode((boolean[]) obj);

    if (obj instanceof byte[])
      return Arrays.hashCode((byte[]) obj);

    if (obj instanceof char[])
      return Arrays.hashCode((char[]) obj);

    if (obj instanceof short[])
      return Arrays.hashCode((short[]) obj);

    if (obj instanceof int[])
      return Arrays.hashCode((int[]) obj);

    if (obj instanceof float[])
      return Arrays.hashCode((float[]) obj);

    if (obj instanceof long[])
      return Arrays.hashCode((long[]) obj);

    if (obj instanceof double[])
      return Arrays.hashCode((double[]) obj);

    if (obj instanceof Object[])
      return Arrays.hashCode((Object[]) obj);

    return obj.hashCode();
  }

  /**
   * Compute the hashCode for an annotation. Note that the algorithm is
   * specified by Annotation.hashCode.
   *
   * This method is public for use by other parts of the VM. Some VMs
   * (can) use different representations of annotations that reuse this
   * method.
   */
  public int hashCode()
  {
    int h = 0;
    Iterator iter = memberValues.keySet().iterator();
    while (iter.hasNext())
      {
	Object key = iter.next();
	try
	  {
	    Object val
	      = invoke(null,
		       type.getDeclaredMethod((String)key, (Class[])null),
		       (Object[])null);
	    h += deepHashCode(val) ^ 127 * key.hashCode();
	  }
	catch (Throwable _)
	  {
	  }
      }
    return h;
  }

  private static String deepToString(Object obj)
  {
    if (obj instanceof Object[])
      return Arrays.toString((Object[]) obj);

    return obj.toString();
  }

  /**
   * This method is public for use by other parts of the VM. Some VMs
   * (can) use different representations of annotations that reuse this
   * method.
   */
  public String toString()
  {
    StringBuffer sb = new StringBuffer();
    sb.append('@').append(type.getName()).append('(');
    String sep = "";
    Iterator iter = memberValues.keySet().iterator();
    while (iter.hasNext())
      {
	Object key = iter.next();
	Object val = memberValues.get(key);
	sb.append(sep).append(key).append('=').append(deepToString(val));
	sep = ", ";
      }
    sb.append(')');
    return sb.toString();
  }


  private static Class getBoxedReturnType(Method method)
  {
    Class returnType = method.getReturnType();

    if (returnType == boolean.class)
      return Boolean.class;

    if (returnType == byte.class)
      return Byte.class;

    if (returnType == char.class)
      return Character.class;

    if (returnType == short.class)
      return Short.class;

    if (returnType == int.class)
      return Integer.class;

    if (returnType == float.class)
      return Float.class;

    if (returnType == long.class)
      return Long.class;

    if (returnType == double.class)
      return Double.class;

    return returnType;
  }

  // This is slightly awkward.  When the value of an annotation is an
  // array, libgcj constructs an Object[], but the value() method
  // returns an arrays of the appropriate primitive type.  We should
  // perhaps save the resulting array rather than the Object[].

  private Object coerce(Object val, Class dstType)
    throws ArrayStoreException
  {
    if (! val.getClass().isArray())
      return val;

    Object[] srcArray = (Object[])val;
    final int len = srcArray.length;

    if (dstType.getComponentType().isPrimitive())
      {
	if (dstType == boolean[].class)
	  {
	    boolean[] dst = new boolean[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Boolean)srcArray[i];
	    return dst;
	  }

	if (dstType == byte[].class)
	  {
	    byte[] dst = new byte[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Byte)srcArray[i];
	    return dst;
	  }

	if (dstType == char[].class)
	  {
	    char[] dst = new char[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Character)srcArray[i];
	    return dst;
	  }

	if (dstType == short[].class)
	  {
	    short[] dst = new short[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Short)srcArray[i];
	    return dst;
	  }

	if (dstType == int[].class)
	  {
	    int[] dst = new int[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Integer)srcArray[i];
	    return dst;
	  }

	if (dstType == long[].class)
	  {
	    long[] dst = new long[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Long)srcArray[i];
	    return dst;
	  }

	if (dstType == float[].class)
	  {
	    float[] dst = new float[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Float)srcArray[i];
	    return dst;
	  }

	if (dstType == double[].class)
	  {
	    double[] dst = new double[len];
	    for (int i = 0; i < len; i++)
	      dst[i] = (Double)srcArray[i];
	    return dst;
	  }
      }

    Object dst = Array.newInstance(dstType.getComponentType(), len);
    System.arraycopy((Object)srcArray, 0, dst, 0, len);
    return dst;
  }

  public Object invoke(Object proxy, Method method, Object[] args)
    throws Throwable
  {
    String methodName = method.getName().intern();

    if (args == null || args.length == 0)
      {
	if (methodName == "toString")
	  {
	    return toString();
	  }
	else if (methodName == "hashCode")
	  {
	    return Integer.valueOf(hashCode());
	  }
	else if (methodName == "annotationType")
	  {
	    return type;
	  }
	else
	  {
	    Object val = memberValues.get(methodName);
	    if (val == null)
	      {
		throw new IncompleteAnnotationException(type, methodName);
	      }
	    try
	      {
		if (val.getClass().isArray())
		  val = coerce((Object[])val, method.getReturnType());
	      }
	    catch (ArrayStoreException _)
	      {
		throw new AnnotationTypeMismatchException
		  (method, val.getClass().getName());
	      }
	    if (! getBoxedReturnType(method).isInstance(val))
	      throw (new AnnotationTypeMismatchException
		     (method, val.getClass().getName()));
	    return val;
	  }
      }
    else if (args.length == 1)
      {
	if (methodName == "equals")
	  {
	    return Boolean.valueOf(equals(proxy, args[0]));
	  }
      }
    throw new InternalError("Invalid annotation proxy");
  }
}
