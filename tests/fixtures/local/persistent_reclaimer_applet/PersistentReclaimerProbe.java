package algtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;

/** Small card-side fixture for the native persistent-tag reclaimer. */
public final class PersistentReclaimerProbe extends Applet {
  private byte[] retained;

  private PersistentReclaimerProbe() {
    register();
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    new PersistentReclaimerProbe();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte command = buffer[ISO7816.OFFSET_P1];
    switch (command) {
      case 1: {
        retained = new byte[16];
        retained[0] = 0x2a;
        buffer[0] = retained[0];
        break;
      }
      case 2:
        retained = null;
        buffer[0] = 0;
        break;
      case 3:
        buffer[0] = retained == null ? 0 : retained[0];
        break;
      case 4:
        retained = new byte[8];
        retained[0] = 0x55;
        buffer[0] = retained[0];
        break;
      case 5:
        buffer[0] = retained == null ? 0 : retained[0];
        break;
      case 6:
        retained = null;
        buffer[0] = JCSystem.isObjectDeletionSupported() ? (byte) 1 : 0;
        JCSystem.requestObjectDeletion();
        break;
      default:
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
    }
    apdu.setOutgoingAndSend((short)0, (short)1);
  }
}
