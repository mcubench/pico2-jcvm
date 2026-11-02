package samecap;

import javacard.framework.APDU;
import javacard.framework.AID;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.Shareable;

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
    return candidate == SharedState.bytes;
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
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
