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
    Shareable shareable = JCSystem.getAppletShareableInterfaceObject(server,
                                                                     (byte) 0x5A);
    AID callerAID = JCSystem.getAID();
    Probe probe = (Probe) shareable;
    short result = probe.exercise((byte) 0xA5, (short) 0x1234,
                                  (short) 0x2345, callerAID, null);
    buffer[0] = (byte) (result >> 8);
    buffer[1] = (byte) result;
    apdu.setOutgoingAndSend((short) 0, (short) 2);
  }
}
