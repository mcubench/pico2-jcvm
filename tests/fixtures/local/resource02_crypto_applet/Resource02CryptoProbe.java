package resource02;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.security.AESKey;
import javacard.security.KeyBuilder;
import javacard.security.RSAPublicKey;
import javacardx.crypto.Cipher;

/** Persistent RSA/AES Cipher.init loop for RESOURCE-02 storage measurements. */
public final class Resource02CryptoProbe extends Applet {
  private static final byte INS_SETUP_AES = (byte)0x30;
  private static final byte INS_INIT_AES = (byte)0x31;
  private static final byte INS_SETUP_RSA = (byte)0x32;
  private static final byte INS_INIT_RSA = (byte)0x33;

  private static final byte[] AES_KEY_BYTES = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  private static final byte[] RSA_MODULUS = new byte[256];
  private static final byte[] RSA_EXPONENT = {1, 0, 1};
  private static final byte[] EMPTY_RESPONSE = new byte[1];

  private static AESKey aesKey;
  private static Cipher aesCipher;
  private static RSAPublicKey rsaKey;
  private static Cipher rsaCipher;

  private Resource02CryptoProbe() { register(); }

  public static void install(byte[] data, short offset, byte length) {
    new Resource02CryptoProbe();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte instruction = buffer[ISO7816.OFFSET_INS];
    byte count = buffer[ISO7816.OFFSET_P2];
    apdu.setIncomingAndReceive();
    switch (instruction) {
      case INS_SETUP_AES:
        setupAes();
        break;
      case INS_INIT_AES:
        repeatAesInit(count);
        break;
      case INS_SETUP_RSA:
        setupRsa();
        break;
      case INS_INIT_RSA:
        repeatRsaInit(count);
        break;
      default:
        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
    apdu.setOutgoing();
    apdu.setOutgoingLength((short)0);
    apdu.sendBytesLong(EMPTY_RESPONSE, (short)0, (short)0);
  }

  private static void setupAes() {
    aesKey = (AESKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
    aesKey.setKey(AES_KEY_BYTES, (short)0);
    aesCipher = Cipher.getInstance(Cipher.ALG_AES_BLOCK_128_ECB_NOPAD, false);
    aesCipher.init(aesKey, Cipher.MODE_ENCRYPT);
  }

  private static void repeatAesInit(byte count) {
    if (aesKey == null || aesCipher == null || count <= 0)
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    for (short i = 0; i < count; i++)
      aesCipher.init(aesKey, Cipher.MODE_ENCRYPT);
  }

  private static void setupRsa() {
    for (short i = 0; i < RSA_MODULUS.length; i++)
      RSA_MODULUS[i] = (byte)(0xA5 ^ i);
    RSA_MODULUS[0] = (byte)0x80;
    RSA_MODULUS[(short)(RSA_MODULUS.length - 1)] = 1;
    rsaKey = (RSAPublicKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_RSA_PUBLIC, KeyBuilder.LENGTH_RSA_2048, false);
    rsaKey.setModulus(RSA_MODULUS, (short)0, (short)RSA_MODULUS.length);
    rsaKey.setExponent(RSA_EXPONENT, (short)0, (short)RSA_EXPONENT.length);
    rsaCipher = Cipher.getInstance(Cipher.ALG_RSA_NOPAD, false);
    rsaCipher.init(rsaKey, Cipher.MODE_ENCRYPT);
  }

  private static void repeatRsaInit(byte count) {
    if (rsaKey == null || rsaCipher == null || count <= 0)
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    for (short i = 0; i < count; i++)
      rsaCipher.init(rsaKey, Cipher.MODE_ENCRYPT);
  }
}
