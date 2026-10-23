/*
 * Java Card 3.0.5 API implementation support.
 * Copyright (C) 2020, Oracle and/or its affiliates. All rights reserved.
 */

package javacard.framework;

/** Shared implementation for the Java Card 3.0.5 OwnerPINx variants. */
public abstract class OwnerPINxBase extends OwnerPIN implements OwnerPINx {

  OwnerPINxBase(byte tryLimit, byte maxPINSize) throws PINException {
    super(tryLimit, maxPINSize);
  }

  public byte getTryLimit() {
    return getTryLimitForSubclass();
  }

  public void setTryLimit(byte limit) {
    if (limit < 1) {
      PINException.throwIt(PINException.ILLEGAL_VALUE);
    }
    setTryLimitForSubclass(limit);
  }

  public void setTriesRemaining(byte remaining) {
    if (remaining < 0 || remaining > getTryLimit()) {
      PINException.throwIt(PINException.ILLEGAL_VALUE);
    }
    setTriesRemainingForSubclass(remaining);
  }
}
