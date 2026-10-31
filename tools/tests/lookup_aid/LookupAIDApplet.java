package lookup;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.AID;
import javacard.framework.ISO7816;

public final class LookupAIDApplet extends Applet {
  private static AID previous;

  private LookupAIDApplet() {
    register();
  }

  public static void install(byte[] parameters, short offset, byte length) {
    new LookupAIDApplet();
  }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    if (buffer[ISO7816.OFFSET_INS] == (byte)0x12) {
      short incoming = apdu.setIncomingAndReceive();
      short total = apdu.getIncomingLength();
      short offsetCdata = apdu.getOffsetCdata();
      byte validCla = apdu.isValidCLA() ? (byte)1 : (byte)0;
      buffer[0] = (byte)incoming;
      buffer[1] = (byte)total;
      buffer[2] = (byte)offsetCdata;
      buffer[3] = validCla;
      apdu.setOutgoingAndSend((short)0, (short)4);
      return;
    }
    short incoming = apdu.setIncomingAndReceive();
    // The fixture uses a short APDU, so its one-byte Lc is authoritative.
    short length = (short)(buffer[ISO7816.OFFSET_LC] & 0xFF);
    if (incoming != length || length < 5 || length > 16) {
      ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    }

    AID found = JCSystem.lookupAID(buffer, ISO7816.OFFSET_CDATA,
                                   (byte)length);
    short outgoing = 1;
    byte result = 0;
    if (found != null) {
      result = found == previous ? (byte)2 : (byte)1;
      previous = found;
      outgoing += found.getBytes(buffer, (short)1);
    }
    buffer[0] = result;
    apdu.setOutgoingAndSend((short)0, outgoing);
  }
}
