/*
 * Java Card 3.0.5 API implementation support.
 * Copyright (C) 2020, Oracle and/or its affiliates. All rights reserved.
 */

package javacard.framework;

/** Owner PIN implementation requiring a per-reset try predecrement. */
public final class OwnerPINxWithPredecrementImpl extends OwnerPINxBase
    implements OwnerPINxWithPredecrement {

  private byte[] predecremented;

  public OwnerPINxWithPredecrementImpl(byte tryLimit, byte maxPINSize)
      throws PINException {
    super(tryLimit, maxPINSize);
    this.predecremented = JCSystem.makeTransientByteArray(
        (short)1, JCSystem.CLEAR_ON_RESET);
  }

  public byte decrementTriesRemaining() {
    setValidatedFlag(false);
    byte remaining = getTriesRemaining();
    if (remaining > 0) {
      setTriesRemainingForSubclass((byte)(remaining - 1));
    }
    this.predecremented[0] = 1;
    return getTriesRemaining();
  }

  public boolean check(byte[] pin, short offset, byte length)
      throws PINException, ArrayIndexOutOfBoundsException, NullPointerException {
    if (this.predecremented[0] == 0) {
      PINException.throwIt(PINException.ILLEGAL_STATE);
    }

    this.predecremented[0] = 0;

    // OwnerPIN.check() performs the actual validation and decrements on
    // failure. Restore one attempt immediately before that call so the
    // already-consumed predecrement is not counted twice. A blocked PIN stays
    // at zero and follows OwnerPIN's normal blocked behavior.
    byte remaining = getTriesRemaining();
    if (remaining > 0) {
      setTriesRemainingForSubclass((byte)(remaining + 1));
    }
    return checkPIN(pin, offset, length);
  }

  public void update(byte[] pin, short offset, byte length)
      throws PINException {
    this.predecremented[0] = 0;
    super.update(pin, offset, length);
  }

  public void setTryLimit(byte limit) {
    this.predecremented[0] = 0;
    super.setTryLimit(limit);
  }

  public void setTriesRemaining(byte remaining) {
    this.predecremented[0] = 0;
    super.setTriesRemaining(remaining);
  }

  public void reset() {
    this.predecremented[0] = 0;
    super.reset();
  }

  public void resetAndUnblock() {
    this.predecremented[0] = 0;
    super.resetAndUnblock();
  }
}
