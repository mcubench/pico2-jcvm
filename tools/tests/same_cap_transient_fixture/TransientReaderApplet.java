package samecaptransient;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;

public final class TransientReaderApplet extends Applet {
  private TransientReaderApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    TransientState.initialize();
    new TransientReaderApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[1] != 0x20) ISOException.throwIt((short)0x6d00);
    buffer[0] = TransientState.deselectBytes[0];
    buffer[1] = TransientState.resetBytes[0];
    apdu.setOutgoingAndSend((short)0, (short)2);
  }
}
