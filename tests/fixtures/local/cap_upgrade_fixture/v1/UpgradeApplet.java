package capupgrade;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

public final class UpgradeApplet extends Applet {
  private static byte staticByte = (byte) 0x12;
  private byte instanceByte = (byte) 0x34;

  private UpgradeApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new UpgradeApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] == 0x10) {
      buffer[0] = (byte) 0xA1;
      buffer[1] = staticByte;
      buffer[2] = instanceByte;
      apdu.setOutgoingAndSend((short) 0, (short) 3);
      return;
    }
    if (buffer[1] == 0x11) {
      staticByte = (byte) 0x55;
      instanceByte = (byte) 0x66;
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }
}
