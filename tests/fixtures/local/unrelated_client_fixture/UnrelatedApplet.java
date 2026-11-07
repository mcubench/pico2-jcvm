package unrelated;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

public final class UnrelatedApplet extends Applet {
  private UnrelatedApplet() {
    register();
  }

  public static void install(byte[] parameters, short offset, byte length) {
    new UnrelatedApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] == 0x10) {
      buffer[0] = (byte) 0x5A;
      apdu.setOutgoingAndSend((short) 0, (short) 1);
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }
}
