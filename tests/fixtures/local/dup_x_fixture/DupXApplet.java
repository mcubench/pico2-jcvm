package dupx.fixture;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.ISO7816;

public final class DupXApplet extends Applet {
  private DupXApplet() {
    if (dupXProbe() != (short) 10) {
      ISOException.throwIt((short) 0x6F01);
    }
    register();
  }

  public static void install(byte[] buffer, short offset, byte length)
      throws ISOException {
    new DupXApplet();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) {
      return;
    }
    byte[] buffer = apdu.getBuffer();
    if (buffer[ISO7816.OFFSET_INS] == (byte) 0x01) {
      try {
        invalidDupXProbe();
      } catch (RuntimeException expected) {
        return;
      }
      ISOException.throwIt((short) 0x6F02);
    }
    if (buffer[ISO7816.OFFSET_INS] == (byte) 0x02) {
      return;
    }
    if (buffer[ISO7816.OFFSET_INS] == (byte) 0x03) {
      invalidDupXMatrix();
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }

  private static short dupXProbe() {
    return (short) 10;
  }

  private static short invalidDupXProbe() {
    return (short) 30;
  }

  private static void invalidDupXMatrix() {
  }
}
