package samecap;

import javacard.framework.APDU;
import javacard.framework.AID;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.ISOException;

public final class ReaderApplet extends Applet {
  private static final byte[] WRITER_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x05, 0x01
  };

  private ReaderApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ReaderApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] == 0x21) {
      AID writerAID = JCSystem.lookupAID(WRITER_AID, (short) 0,
                                         (byte) WRITER_AID.length);
      Probe writer = (Probe) JCSystem.getAppletShareableInterfaceObject(
          writerAID, (byte) 0x51);
      buffer[0] = (byte) (writer != null && writer.isShared(SharedState.bytes)
                              ? 1
                              : 0);
      apdu.setOutgoingAndSend((short) 0, (short) 1);
      return;
    }
    if (buffer[1] == 0x20) {
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
