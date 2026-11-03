package sio.service;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.framework.Util;
import sio.contract.Probe;

public final class ServiceApplet extends Applet implements Probe {
  private static final byte[] CLIENT_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x04, 0x01
  };
  private static final byte[] SERVICE_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x03, 0x01
  };

  private ServiceApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ServiceApplet();
  }

  public Shareable getShareableInterfaceObject(AID clientAID, byte parameter) {
    if (parameter != 0x5A || clientAID == null ||
        !clientAID.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length)) {
      return null;
    }
    return this;
  }

  public short exercise(byte marker, short offset, short wide, AID callerAID,
                        byte[] nullable) {
    AID caller = JCSystem.getPreviousContextAID();
    AID self = JCSystem.getAID();
    if (marker != (byte) 0xA5 || offset != (short) 0x1234 ||
        wide != (short) 0x2345 || callerAID == null || nullable != null ||
        caller == null || self == null ||
        !callerAID.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length) ||
        !caller.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length) ||
        !self.equals(SERVICE_AID, (short) 0, (byte) SERVICE_AID.length)) {
      return (short) 0x6F01;
    }
    return (short) 0x4567;
  }

  public int exerciseWide(int value) {
    return value == 0x12345678 ? 0x76543210 : 0x6F010001;
  }

  public short throwSecurityException() {
    throw new SecurityException();
  }

  public short inspectPrivateArray(byte[] value) {
    byte[] forwarded = value;
    try {
      return (short) forwarded.length;
    } catch (SecurityException expected) {
      return (short) 0x5A31;
    }
  }

  public short inspectPrivateArrayNative(byte[] value) {
    byte[] destination = new byte[1];
    try {
      Util.arrayCopyNonAtomic(value, (short) 0, destination, (short) 0,
                              (short) 1);
      return (short) 0x6B36;
    } catch (SecurityException expected) {
      return (short) 0x6B37;
    }
  }

  public short inspectGlobalReference(Object[] values) {
    try {
      Object value = values[0];
      if (value == null) {
        return (short)0x6B01;
      }
      // Client and service applet instances commonly occupy the same compact
      // slot in their separate package heaps. Owner provenance must keep
      // those distinct objects unequal across this SIO boundary.
      if (value == this) {
        return (short)0x6B30;
      }
      value.equals(this);
      return (short)0x6B02;
    } catch (SecurityException expected) {
      return (short)0x6B31;
    }
  }

  public byte[] echoArray(byte[] value) {
    return value;
  }

  public boolean sameReference(byte[] reference) {
    if (reference == null) {
      return true;
    }
    return reference == reference;
  }

  public boolean sameReferences(byte[] left, byte[] right) {
    return left == right;
  }

  public short inspectForeignCast(Object value) {
    try {
      Probe ignored = (Probe) value;
      return (short)0x6B33;
    } catch (SecurityException expected) {
      return (short)0x6B32;
    }
  }

  public short inspectForeignInstanceof(Object value) {
    try {
      return value instanceof Probe ? (short)0x6B34 : (short)0x6B35;
    } catch (SecurityException expected) {
      return (short)0x6B32;
    }
  }

  public void process(APDU apdu) {}
}
