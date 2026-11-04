package cipherlife;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.security.AESKey;
import javacard.security.CryptoException;
import javacard.security.KeyBuilder;
import javacard.security.Key;
import javacard.security.RSAPrivateKey;
import javacardx.crypto.Cipher;

/** Minimal physical test for transient-key lifetime retained by Cipher. */
public final class CipherLifecycleProbe extends Applet {
  private static AESKey retainedKey;
  private static Cipher retainedCipher;
  private static RSAPrivateKey retainedRsaKey;
  private static Cipher retainedRsaCipher;

  private CipherLifecycleProbe() { register(); }

  private static void setRsaKeyMaterial() {
    byte[] modulus = new byte[64];
    modulus[0] = 0x7f;
    byte[] exponent = new byte[] {0x01, 0x00, 0x01};
    retainedRsaKey.setModulus(modulus, (short)0, (short)modulus.length);
    retainedRsaKey.setExponent(exponent, (short)0, (short)exponent.length);
  }

  public static void install(byte[] data, short offset, byte length) {
    new CipherLifecycleProbe();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte instruction = buffer[ISO7816.OFFSET_INS];
    byte result;
    if (instruction == (byte)0x0A) {
      try {
        Key temporaryKey = KeyBuilder.buildKey(
            KeyBuilder.TYPE_AES_TRANSIENT_DESELECT,
            KeyBuilder.LENGTH_AES_128, false);
        result = (byte)(temporaryKey == null ? 0x6A : 1);
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x67;
      }
    } else if (instruction == (byte)0x09) {
      try {
        AESKey temporaryKey = (AESKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_AES_TRANSIENT_DESELECT,
            KeyBuilder.LENGTH_AES_128, false);
        result = (byte)(temporaryKey == null ? 0x69 : 1);
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x68;
      }
    } else if (instruction == (byte)0x01) {
      try {
        retainedKey = (AESKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_AES_TRANSIENT_DESELECT,
            KeyBuilder.LENGTH_AES_128, false);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x61;
      }
    } else if (instruction == (byte)0x02) {
      try {
        retainedKey.setKey(new byte[16], (short)0);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x62;
      }
    } else if (instruction == (byte)0x03) {
      try {
        retainedCipher = Cipher.getInstance(
            Cipher.ALG_AES_BLOCK_128_ECB_NOPAD, false);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x63;
      }
    } else if (instruction == (byte)0x04) {
      try {
        retainedCipher.init(retainedKey, Cipher.MODE_ENCRYPT);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x64;
      }
    } else if (instruction == (byte)0x05) {
      try {
        retainedCipher.doFinal(new byte[16], (short)0, (short)16,
                               new byte[16], (short)0);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x65;
      }
    } else if (instruction == (byte)0x06) {
      try {
        retainedRsaKey = (RSAPrivateKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_DESELECT,
            KeyBuilder.LENGTH_RSA_512, false);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x66;
      }
    } else if (instruction == (byte)0x07) {
      try {
        setRsaKeyMaterial();
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x67;
      }
    } else if (instruction == (byte)0x08) {
      try {
        retainedRsaCipher = Cipher.getInstance(Cipher.ALG_RSA_NOPAD, false);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x68;
      }
    } else if (instruction == (byte)0x0B) {
      try {
        retainedRsaCipher.init(retainedRsaKey, Cipher.MODE_DECRYPT);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x6B;
      }
    } else if (instruction == (byte)0x0C) {
      try {
        retainedRsaCipher.doFinal(new byte[64], (short)0, (short)64,
                                  new byte[64], (short)0);
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x6C;
      }
    } else if (instruction == (byte)0x0D) {
      try {
        setRsaKeyMaterial();
        result = 1;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x6D;
      }
    } else {
      ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
      return;
    }
    buffer[0] = result;
    apdu.setOutgoingAndSend((short)0, (short)1);
  }
}
