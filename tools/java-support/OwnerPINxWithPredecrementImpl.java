/*
 * Java Card 3.0.5 API implementation support.
 * Copyright (C) 2020, Oracle and/or its affiliates. All rights reserved.
 */

package javacard.framework;

// Preserve the ROM class, field and method tokens for stored PINs.
// The ROM generator binds these declarations to native_owner_pin.cpp.
public final class OwnerPINxWithPredecrementImpl extends OwnerPINxBase
    implements OwnerPINxWithPredecrement {
  private byte[] predecremented;

  public OwnerPINxWithPredecrementImpl(byte tryLimit, byte maxPINSize) {
    super(tryLimit, maxPINSize);
  }

  public byte decrementTriesRemaining() { return 0; }
  public boolean check(byte[] pin, short offset, byte length) { return false; }
  public void update(byte[] pin, short offset, byte length) {}
  public void setTryLimit(byte limit) {}
  public void setTriesRemaining(byte remaining) {}
  public void reset() {}
  public void resetAndUnblock() {}
}
