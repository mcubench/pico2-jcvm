package algtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.TransactionException;
import javacard.security.MessageDigest;

public final class JCTransactionProbe extends Applet {
  private byte value = 0x11;
  private final MessageDigest sha256 =
      MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false);
  private final byte[] shaInput = {0x61, 0x62, 0x63};
  private final byte[] shaDigest = new byte[32];

  private JCTransactionProbe() {
    register();
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    new JCTransactionProbe();
  }

  public boolean select() {
    return true;
  }

  public void deselect() {}

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte command = buffer[ISO7816.OFFSET_P1];
    short length = 1;
    switch (command) {
      case 1:
        JCSystem.beginTransaction();
        buffer[0] = JCSystem.getTransactionDepth();
        JCSystem.commitTransaction();
        buffer[1] = JCSystem.getTransactionDepth();
        length = 2;
        break;
      case 2:
        try { JCSystem.commitTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        break;
      case 3:
        try { JCSystem.abortTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        break;
      case 4:
        JCSystem.beginTransaction();
        try { JCSystem.beginTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        JCSystem.abortTransaction();
        buffer[1] = JCSystem.getTransactionDepth();
        length = 2;
        break;
      case 5:
        JCSystem.beginTransaction();
        value = 0x22;
        JCSystem.abortTransaction();
        buffer[0] = value;
        break;
      case 6:
        JCSystem.beginTransaction();
        value = 0x33;
        JCSystem.commitTransaction();
        buffer[0] = value;
        break;
      case 7:
        buffer[0] = value;
        break;
      case 8:
        sha256.doFinal(shaInput, (short)0, (short)3,
                       shaDigest, (short)0);
        for (short i = 0; i < (short)shaDigest.length; i++) {
          buffer[i] = shaDigest[i];
        }
        length = (short)shaDigest.length;
        break;
      default:
        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
    apdu.setOutgoingAndSend((short)0, length);
  }
}
