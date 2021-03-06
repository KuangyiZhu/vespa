// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.data.access;


import java.util.Map;

/**
 * This is a generic API for accessing structured, generic, schemaless data.
 * An inspector is a handle to a value that has one of 8 specific types:
 * EMPTY, the 5 scalar types BOOL, LONG, DOUBLE, STRING, or DATA, the
 * simple list-like ARRAY container and the struct-like OBJECT container.
 * Instrospection methods are available, but you can also use accessors
 * with a default value if you expect a certain type and just want your
 * default value if some field doesn't exist or was of the wrong type.
 **/
public interface Inspector extends Inspectable {

    /**
     * Check if the inspector is valid.
     * If you try to access a field or array entry that does not exist,
     * you will get an invalid Inspector returned.
     */
    public boolean valid();

    /** Get the type of an inspector */
    public Type type();

    /** Get the number of entries in an ARRAY (always returns 0 for non-arrays) */
    public int entryCount();

    /** Get the number of fields in an OBJECT (always returns 0 for non-objects) */
    public int fieldCount();

    /** Access the inspector's value if it's a BOOLEAN; otherwise throws exception */
    public boolean asBool();

    /** Access the inspector's value if it's a LONG (or DOUBLE); otherwise throws exception */
    public long asLong();

    /** Access the inspector's value if it's a DOUBLE (or LONG); otherwise throws exception */
    public double asDouble();

    /** Access the inspector's value if it's a STRING; otherwise throws exception */
    public String asString();

    /**
     * Access the inspector's value (in utf-8 representation) if it's
     * a STRING; otherwise throws exception
     **/
    public byte[] asUtf8();

    /** Access the inspector's value if it's DATA; otherwise throws exception */
    public byte[] asData();

    /** Get the inspector's value (or the supplied default), never throws */
    public boolean asBool(boolean defaultValue);

    /** Get the inspector's value (or the supplied default), never throws */
    public long asLong(long defaultValue);

    /** Get the inspector's value (or the supplied default), never throws */
    public double asDouble(double defaultValue);

    /** Get the inspector's value (or the supplied default), never throws */
    public String asString(String defaultValue);

    /** Get the inspector's value (or the supplied default), never throws */
    public byte[] asUtf8(byte[] defaultValue);

    /** Get the inspector's value (or the supplied default), never throws */
    public byte[] asData(byte[] defaultValue);

    /**
     * Traverse an array value, performing callbacks for each entry.
     *
     * If the current Inspector is connected to an array value,
     * perform callbacks to the given traverser for each entry
     * contained in the array.  Otherwise a no-op.
     * @param at traverser callback object.
     **/
    @SuppressWarnings("overloads")
    public void traverse(ArrayTraverser at);

    /**
     * Traverse an object value, performing callbacks for each field.
     *
     * If the current Inspector is connected to an object value,
     * perform callbacks to the given traverser for each field
     * contained in the object.  Otherwise a no-op.
     * @param ot traverser callback object.
     **/
    @SuppressWarnings("overloads")
    public void traverse(ObjectTraverser ot);

    /**
     * Access an array entry.
     *
     * If the current Inspector doesn't connect to an array value,
     * or the given array index is out of bounds, the returned
     * Inspector will be invalid.
     * @param idx array index.
     * @return a new Inspector for the entry value.
     **/
    public Inspector entry(int idx);

    /**
     * Access an field in an object.
     *
     * If the current Inspector doesn't connect to an object value, or
     * the object value does not contain a field with the given symbol
     * name, the returned Inspector will be invalid.
     * @param name symbol name.
     * @return a new Inspector for the field value.
     **/
    public Inspector field(String name);

    /**
     * Convert an array to an iterable list.  Other types will just
     * return an empty list.
     **/
    public Iterable<Inspector> entries();

    /**
     * Convert an object to an iterable list of (name, value) pairs.
     * Other types will just return an empty list.
     **/
    public Iterable<Map.Entry<String,Inspector>> fields();
}
