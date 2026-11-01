package sio.client;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import sio.contract.Probe;

public final class ClientApplet extends Applet {
  private static final byte[] SERVICE_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x03, 0x01
  };

  private ClientApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ClientApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) {
      return;
    }
    byte[] buffer = apdu.getBuffer();
    AID server = JCSystem.lookupAID(SERVICE_AID, (short) 0,
                                    (byte) SERVICE_AID.length);
    boolean denial_probe = buffer[1] == 0x11;
    Shareable shareable = JCSystem.getAppletShareableInterfaceObject(
        server, denial_probe ? (byte) 0x5B : (byte) 0x5A);
    short result;
    if (denial_probe) {
      result = shareable == null ? (short) 0x6A01 : (short) 0x6A02;
    } else if (shareable == null) {
      result = (short) 0x6A04;
    } else if (buffer[1] == 0x13) {
      Probe probe = (Probe) shareable;
      int wide = probe.exerciseWide(0x12345678);
      buffer[0] = (byte) (wide >> 24);
      buffer[1] = (byte) (wide >> 16);
      buffer[2] = (byte) (wide >> 8);
      buffer[3] = (byte) wide;
      apdu.setOutgoingAndSend((short) 0, (short) 4);
      return;
    } else {
      AID callerAID = JCSystem.getAID();
      Probe probe = (Probe) shareable;
      if (buffer[1] == 0x12) {
        try {
          probe.exercise((byte) 0xA5, (short) 0x1234, (short) 0x2345,
                         callerAID, new byte[] { 0x01 });
          result = (short) 0x6A05;
        } catch (SecurityException expected) {
          result = (short) 0x6A03;
        }
      } else {
        result = probe.exercise((byte) 0xA5, (short) 0x1234,
                                (short) 0x2345, callerAID, null);
      }
    }
    buffer[0] = (byte) (result >> 8);
    buffer[1] = (byte) result;
    apdu.setOutgoingAndSend((short) 0, (short) 2);
  }
}
