package samecap;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

public final class WriterApplet extends Applet {
  private WriterApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new WriterApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] == 0x10) {
      SharedState.counter++;
      SharedState.bytes[0]++;
      buffer[0] = (byte) (SharedState.counter >> 8);
      buffer[1] = (byte) SharedState.counter;
      buffer[2] = SharedState.bytes[0];
      apdu.setOutgoingAndSend((short) 0, (short) 3);
      return;
    }
    ISOException.throwIt((short) 0x6D00);
  }
}
