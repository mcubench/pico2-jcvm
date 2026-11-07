package capupgrade;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

public final class UpgradeApplet extends Applet {
  private static byte staticByte = (byte) 0x22;
  private static short staticShort = (short) 0x3344;
  private byte instanceByte = (byte) 0x55;
  private short instanceShort = (short) 0x6677;

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
      buffer[0] = (byte) 0xB2;
      buffer[1] = staticByte;
      buffer[2] = (byte) (staticShort >> 8);
      buffer[3] = (byte) staticShort;
      buffer[4] = instanceByte;
      buffer[5] = (byte) (instanceShort >> 8);
      buffer[6] = (byte) instanceShort;
      apdu.setOutgoingAndSend((short) 0, (short) 7);
      return;
    }
    if (buffer[1] == 0x11) {
      staticByte = (byte) 0x71;
      staticShort = (short) 0x7283;
      instanceByte = (byte) 0x91;
      instanceShort = (short) 0xA2B3;
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }
}
