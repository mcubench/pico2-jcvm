package sio.service;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
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

  public void process(APDU apdu) {}
}
