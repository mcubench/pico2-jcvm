package samecap;

import javacard.framework.APDU;
import javacard.framework.AID;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.framework.SystemException;

public final class WriterApplet extends Applet implements Probe {
  private static final byte[] READER_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x05, 0x02
  };

  private WriterApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new WriterApplet();
  }

  public Shareable getShareableInterfaceObject(AID clientAID,
                                                byte parameter) {
    if (parameter == 0x51 && clientAID != null &&
        clientAID.equals(READER_AID, (short) 0, (byte) READER_AID.length)) {
      return this;
    }
    return null;
  }

  public boolean isShared(byte[] candidate) {
    try {
      byte[] transientBytes = JCSystem.makeTransientByteArray(
          (short) 1, JCSystem.CLEAR_ON_DESELECT);
      return candidate == SharedState.bytes && transientBytes != null &&
          JCSystem.isTransient(transientBytes) == JCSystem.CLEAR_ON_DESELECT;
    } catch (SystemException rejected) {
      return false;
    }
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] == 0x30) {
      SharedState.deletionProbe = new byte[16];
      SharedState.deletionProbe[0] = 0x5a;
      buffer[0] = 1;
      apdu.setOutgoingAndSend((short) 0, (short) 1);
      return;
    }
    if (buffer[1] == 0x31) {
      SharedState.deletionProbe = null;
      buffer[0] = JCSystem.isObjectDeletionSupported() ? (byte) 1 : 0;
      if (buffer[0] != 0) JCSystem.requestObjectDeletion();
      apdu.setOutgoingAndSend((short) 0, (short) 1);
      return;
    }
    if (buffer[1] == 0x10) {
      SharedState.counter++;
      SharedState.bytes[0]++;
      SharedState.marker++;
      SharedState.integer++;
      buffer[0] = (byte) (SharedState.zero >> 8);
      buffer[1] = (byte) SharedState.zero;
      buffer[2] = (byte) (SharedState.counter >> 8);
      buffer[3] = (byte) SharedState.counter;
      buffer[4] = SharedState.bytes[0];
      buffer[5] = SharedState.marker;
      buffer[6] = (byte) (SharedState.integer >> 24);
      buffer[7] = (byte) (SharedState.integer >> 16);
      buffer[8] = (byte) (SharedState.integer >> 8);
      buffer[9] = (byte) SharedState.integer;
      apdu.setOutgoingAndSend((short) 0, (short) 10);
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }
}
