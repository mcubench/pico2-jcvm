package algtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.security.KeyBuilder;
import javacard.security.RSAPublicKey;

/** Small card-side fixture for the native persistent-tag reclaimer. */
public final class PersistentReclaimerProbe extends Applet {
  private byte[] retained;
  private RSAPublicKey retainedRsaKey;
  private static RSAPublicKey retainedStaticRsaKey;

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
      case 7: {
        short keyLength;
        short modulusLength;
        switch (buffer[ISO7816.OFFSET_P2]) {
          case 0:
            keyLength = KeyBuilder.LENGTH_RSA_512;
            modulusLength = 64;
            break;
          case 1:
            keyLength = KeyBuilder.LENGTH_RSA_1024;
            modulusLength = 128;
            break;
          case 2:
            keyLength = KeyBuilder.LENGTH_RSA_2048;
            modulusLength = 256;
            break;
          case 3:
            keyLength = KeyBuilder.LENGTH_RSA_4096;
            modulusLength = 512;
            break;
          default:
            ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
            return;
        }
        byte[] modulus = new byte[modulusLength];
        for (short i = 0; i < modulusLength; i++) modulus[i] = (byte)(i + 1);
        modulus[0] = (byte)0x80;
        byte[] exponent = {0x01, 0x00, 0x01};
        retainedRsaKey = (RSAPublicKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_RSA_PUBLIC, keyLength, false);
        retainedRsaKey.setModulus(modulus, (short)0, (short)modulus.length);
        retainedRsaKey.setExponent(exponent, (short)0, (short)exponent.length);
        buffer[0] = retainedRsaKey.isInitialized() ? (byte)0x37 : 0;
        break;
      }
      case 8:
        retainedRsaKey = null;
        buffer[0] = JCSystem.isObjectDeletionSupported() ? (byte) 1 : 0;
        JCSystem.requestObjectDeletion();
        break;
      case 9: {
        byte[] modulus = new byte[64];
        for (short i = 0; i < 64; i++) modulus[i] = (byte)(i + 1);
        modulus[0] = 0x7f;
        byte[] exponent = {0x01, 0x00, 0x01};
        retainedRsaKey = (RSAPublicKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_RSA_PUBLIC, KeyBuilder.LENGTH_RSA_512, false);
        retainedRsaKey.setModulus(modulus, (short)0, (short)modulus.length);
        retainedRsaKey.setExponent(exponent, (short)0, (short)exponent.length);
        retainedStaticRsaKey = retainedRsaKey;
        buffer[0] = retainedRsaKey.isInitialized() ? (byte)0x37 : 0;
        break;
      }
      case 10:
        retainedRsaKey = null;
        buffer[0] = JCSystem.isObjectDeletionSupported() ? (byte) 1 : 0;
        JCSystem.requestObjectDeletion();
        break;
      case 11:
        retainedStaticRsaKey = null;
        buffer[0] = JCSystem.isObjectDeletionSupported() ? (byte) 1 : 0;
        JCSystem.requestObjectDeletion();
        break;
      case 12:
        buffer[0] = retainedStaticRsaKey != null &&
                            retainedStaticRsaKey.isInitialized()
                        ? (byte)0x37
                        : 0;
        break;
      case 13:
        buffer[0] = retainedRsaKey == null ? (byte)0 : (byte)1;
        break;
      case 14:
        retainedRsaKey = null;
        buffer[0] = 0;
        break;
      default:
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
    }
    apdu.setOutgoingAndSend((short)0, (short)1);
  }
}
