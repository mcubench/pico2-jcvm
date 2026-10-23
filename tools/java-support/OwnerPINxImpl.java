/*
 * Java Card 3.0.5 API implementation support.
 * Copyright (C) 2020, Oracle and/or its affiliates. All rights reserved.
 */

package javacard.framework;

/** Owner PIN implementation with an adjustable try limit. */
public final class OwnerPINxImpl extends OwnerPINxBase implements OwnerPINx {

  public OwnerPINxImpl(byte tryLimit, byte maxPINSize) throws PINException {
    super(tryLimit, maxPINSize);
  }
}
