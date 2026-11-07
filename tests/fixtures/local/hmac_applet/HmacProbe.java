package hmacprobe;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.security.HMACKey;
import javacard.security.KeyBuilder;
import javacard.security.Signature;

/** CAP fixture for checking native HMAC behavior, not an algorithm provider. */
public final class HmacProbe extends Applet {
  private HmacProbe() { register(); }

  public static void install(byte[] data, short offset, byte length) {
    new HmacProbe();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte algorithm = buffer[ISO7816.OFFSET_P1];
    byte mode = buffer[ISO7816.OFFSET_P2];
    short length = apdu.setIncomingAndReceive();
    byte[] message = new byte[length];
    for (short i = 0; i < length; i++)
      message[i] = buffer[(short)(ISO7816.OFFSET_CDATA + i)];

    HMACKey key = (HMACKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_HMAC, (short)160, false);
    byte[] keyBytes = new byte[20];
    for (short i = 0; i < keyBytes.length; i++) keyBytes[i] = (byte)0x0b;
    key.setKey(keyBytes, (short)0, (short)keyBytes.length);
    byte[] keyReadback = new byte[20];
    byte keyLength = key.getKey(keyReadback, (short)0);

    Signature signature = Signature.getInstance(algorithm, false);
    signature.init(key, Signature.MODE_SIGN);
    byte[] mac = new byte[signature.getLength()];
    if (mode == 1 && length > 2) {
      signature.update(message, (short)0, (short)2);
      signature.sign(message, (short)2, (short)(length - 2), mac, (short)0);
    } else {
      signature.sign(message, (short)0, length, mac, (short)0);
    }

    signature.init(key, Signature.MODE_VERIFY);
    boolean accepted = signature.verify(message, (short)0, length,
                                        mac, (short)0, (short)mac.length);
    mac[0] ^= 1;
    signature.init(key, Signature.MODE_VERIFY);
    boolean rejected = !signature.verify(message, (short)0, length,
                                         mac, (short)0, (short)mac.length);
    mac[0] ^= 1;

    byte[] result = new byte[(short)(mac.length + 3)];
    for (short i = 0; i < mac.length; i++) result[i] = mac[i];
    result[mac.length] = accepted ? (byte)1 : (byte)0;
    result[(short)(mac.length + 1)] = rejected ? (byte)1 : (byte)0;
    result[(short)(mac.length + 2)] =
        keyLength == 20 && keyReadback[0] == (byte)0x0b &&
        keyReadback[19] == (byte)0x0b ? (byte)1 : (byte)0;
    apdu.setOutgoing();
    apdu.setOutgoingLength((short)result.length);
    apdu.sendBytesLong(result, (short)0, (short)result.length);
  }
}
