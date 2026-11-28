package lostatic.client;

import javacard.framework.APDU;
import javacard.framework.Applet;
import lostatic.tables.Tables;

/** Reads the static array of a package that was loaded but never installed. */
public final class ClientApplet extends Applet {
  private ClientApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ClientApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    try {
      buffer[0] = Tables.TABLE[1];
    } catch (NullPointerException uninitialized) {
      buffer[0] = (byte) 0xEE;
    }
    apdu.setOutgoingAndSend((short) 0, (short) 1);
  }
}
