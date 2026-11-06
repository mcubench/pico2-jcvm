package dupx.fixture;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

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
    ISOException.throwIt((short) 0x6D00);
  }

  private static short dupXProbe() {
    return (short) 10;
  }
}
