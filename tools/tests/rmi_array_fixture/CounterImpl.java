package rmi.fixture;

import java.lang.ArrayIndexOutOfBoundsException;
import javacard.framework.AID;
import javacard.framework.JCSystem;
import javacard.framework.ISOException;
import javacard.framework.Util;
import javacard.framework.service.CardRemoteObject;
import javacardx.security.SensitiveResult;

public final class CounterImpl extends CardRemoteObject implements Counter {
  private short balance;
  private Counter child;

  public short add(short amount) {
    balance += amount;
    return balance;
  }

  public short value() {
    return balance;
  }

  public boolean verifySensitiveResult() {
    AID aid = JCSystem.getAID();
    SensitiveResult.assertEquals(aid);

    if (!aid.equals(aid)) return false;
    SensitiveResult.assertTrue();

    byte[] source = {1, 2};
    byte[] destination = {0, 0};
    short end = Util.arrayCopyNonAtomic(source, (short)0, destination,
                                        (short)0, (short)2);
    SensitiveResult.assertEquals((short)2);
    if (end != 2 || destination[0] != 1 || destination[1] != 2) return false;

    if (Util.arrayCompare(source, (short)0, source, (short)0, (short)2) != 0) {
      return false;
    }
    SensitiveResult.assertZero();

    boolean wrongValueRejected = false;
    try {
      SensitiveResult.assertPositive();
    } catch (java.lang.SecurityException expected) {
      wrongValueRejected = true;
    }
    SensitiveResult.assertZero();

    SensitiveResult.reset();
    boolean resetStateRejected = false;
    try {
      SensitiveResult.assertTrue();
    } catch (java.lang.SecurityException expected) {
      resetStateRejected = true;
    }
    return wrongValueRejected && resetStateRejected;
  }

  public Counter child() {
    if (child == null) child = new CounterImpl();
    return child;
  }

  public short allocationPressure() {
    short checksum = 0;
    for (short index = 0; index < 256; index++) {
      byte[] discarded = new byte[64];
      discarded[0] = (byte)index;
      checksum += discarded[0];
    }
    return checksum;
  }

  public byte[] echoBytes(byte[] value) {
    return value;
  }

  public short sumBytes(byte[] value) {
    if (value == null) return (short)-1;
    short sum = 0;
    for (short index = 0; index < value.length; index++) {
      sum += value[index];
    }
    return sum;
  }

  public short sumEight(byte[] a, byte[] b, byte[] c, byte[] d, byte[] e,
                        byte[] f, byte[] g, byte[] h) {
    return (short)(a[0] + b[0] + c[0] + d[0] + e[0] + f[0] + g[0] + h[0]);
  }

  public short[] echoShorts(short[] value) {
    return value;
  }

  public boolean[] invert(boolean[] value) {
    if (value == null) return null;
    for (short index = 0; index < value.length; index++) {
      value[index] = !value[index];
    }
    return value;
  }

  public byte arrayAt(byte[] value, short index) {
    return value[index];
  }

  public byte arrayAtOrDefault(byte[] value, short index) {
    try {
      return value[index];
    } catch (ArrayIndexOutOfBoundsException expected) {
      return (byte)0x55;
    }
  }

  public short invalidMemoryType() {
    return JCSystem.getAvailableMemory((byte)3);
  }

  public void throwIso() {
    ISOException.throwIt((short)0x6A82);
  }

  public void throwCustomRuntime() {
    throw new CustomRuntimeException();
  }

  public void throwCustomChecked() throws CustomCheckedException {
    throw new CustomCheckedException();
  }
}
