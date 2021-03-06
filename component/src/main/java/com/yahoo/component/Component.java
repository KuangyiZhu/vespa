// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.component;


/**
 * A named, versioned, identifiable component.
 * <p>
 * Components can by default be ordered by their id order. Their identity is defined by the id.
 * Prefer extending AbstractComponent instead of implementing this interface directly.
 *
 * @author bratseth
 */
public interface Component extends Comparable<Component> {

    /** Initializes this. Always called from a constructor or the framework. Do not call. */
    void initId(ComponentId id);

    /** Returns the id of this component */
    ComponentId getId();

}
