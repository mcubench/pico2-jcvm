package cipherlife;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
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

  private static byte testCipherBufferSurvivesAbort() {
    AESKey key = (AESKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
    byte[] keyBytes = new byte[16];
    for (short i = 0; i < 16; i++) keyBytes[i] = (byte)(0xf0 + i);
    byte[] plaintext = {0, 1, 2, 3, 4, 5, 6, 7,
                        8, 9, 10, 11, 12, 13, 14, 15};
    byte[] expected = {(byte)0xf7, (byte)0xcb, (byte)0x9d, (byte)0xf8,
        (byte)0xa5, 0x7d, (byte)0x8c, 0x78, 0x47, 0x58, (byte)0xaa,
        (byte)0xe0, (byte)0x8d, (byte)0xd9, (byte)0x99, 0x68};
    byte[] output = new byte[16];
    byte[] iv = new byte[16];
    key.setKey(keyBytes, (short)0);
    Cipher cipher = Cipher.getInstance(
        Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
    cipher.init(key, Cipher.MODE_ENCRYPT, iv, (short)0, (short)iv.length);
    retainedKey = key;
    retainedCipher = cipher;
    JCSystem.beginTransaction();
    short updateLength = retainedCipher.update(plaintext, (short)0, (short)15,
                                               output, (short)0);
    JCSystem.abortTransaction();
    short finalLength = retainedCipher.doFinal(plaintext, (short)15, (short)1,
                                               output, (short)0);
    if (updateLength != 0 || finalLength != 16) return (byte)0x71;
    for (short i = 0; i < 16; i++) {
      if (output[i] != expected[i]) return (byte)0x72;
    }

    byte[] twoBlocks = {0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31};
    byte[] expectedSecondBlock = {
        (byte)0xa7, (byte)0x8f, (byte)0xf1, (byte)0xbe, 0x46, 0x34,
        0x40, (byte)0xc0, (byte)0x9a, 0x59, (byte)0xbd, (byte)0xf7,
        (byte)0x97, (byte)0x98, (byte)0xf6, 0x7b};
    cipher = Cipher.getInstance(Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
    cipher.init(key, Cipher.MODE_ENCRYPT, iv, (short)0, (short)iv.length);
    retainedCipher = cipher;
    JCSystem.beginTransaction();
    updateLength = retainedCipher.update(twoBlocks, (short)0, (short)16,
                                         output, (short)0);
    JCSystem.abortTransaction();
    finalLength = retainedCipher.doFinal(twoBlocks, (short)16, (short)16,
                                         output, (short)0);
    if (updateLength != 16 || finalLength != 16) return (byte)0x73;
    for (short i = 0; i < 16; i++) {
      if (output[i] != expectedSecondBlock[i]) return (byte)0x74;
    }

    try {
      retainedRsaKey = (RSAPrivateKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_RSA_PRIVATE, KeyBuilder.LENGTH_RSA_512, false);
      setRsaKeyMaterial();
    } catch (CryptoException exception) {
      return (byte)0x81;
    }
    byte[] rsaInput = new byte[64];
    byte[] rsaOutput = new byte[64];
    byte[] expectedRsaOutput = new byte[64];
    for (short i = 0; i < 64; i++) {
      rsaInput[i] = (byte)i;
      expectedRsaOutput[i] = (byte)i;
    }
    try {
      retainedRsaCipher = Cipher.getInstance(Cipher.ALG_RSA_NOPAD, false);
    } catch (CryptoException exception) {
      return (byte)0x82;
    }
    try {
      retainedRsaCipher.init(retainedRsaKey, Cipher.MODE_DECRYPT);
    } catch (CryptoException exception) {
      return (byte)0x83;
    }
    JCSystem.beginTransaction();
    try {
      updateLength = retainedRsaCipher.update(rsaInput, (short)0, (short)32,
                                               rsaOutput, (short)0);
    } catch (CryptoException exception) {
      JCSystem.abortTransaction();
      return (byte)0x84;
    }
    JCSystem.abortTransaction();
    try {
      finalLength = retainedRsaCipher.doFinal(rsaInput, (short)32, (short)32,
                                               rsaOutput, (short)0);
    } catch (CryptoException exception) {
      return (byte)(0x90 | exception.getReason());
    }
    if (updateLength != 0 || finalLength != 64) return (byte)0x75;
    for (short i = 0; i < 64; i++) {
      if (rsaOutput[i] != expectedRsaOutput[i]) return (byte)0x76;
    }
    retainedCipher = retainedRsaCipher;
    return 1;
  }

  private CipherLifecycleProbe() { register(); }

  private static void setRsaKeyMaterial() {
    byte[] modulus = new byte[64];
    modulus[0] = 0x7f;
    for (short i = 1; i < 64; i++) modulus[i] = (byte)0xff;
    byte[] exponent = new byte[64];
    exponent[63] = 0x01;
    retainedRsaKey.setModulus(modulus, (short)0, (short)modulus.length);
    retainedRsaKey.setExponent(exponent, (short)0, (short)exponent.length);
  }

  private static byte testRsaCipherAfterRestore() {
    try {
      retainedRsaCipher.init(retainedRsaKey, Cipher.MODE_DECRYPT);
      byte[] input = new byte[64];
      byte[] output = new byte[64];
      for (short i = 0; i < 64; i++) input[i] = (byte)i;
      short length = retainedRsaCipher.doFinal(input, (short)0, (short)64,
                                                output, (short)0);
      if (length != 64) return (byte)0x77;
      for (short i = 0; i < 64; i++) {
        if (output[i] != input[i]) return (byte)0x78;
      }
      return 1;
    } catch (CryptoException exception) {
      return (byte)(0x40 | exception.getReason());
    }
  }

  private static byte testBufferedCipherInput(byte[] apduBuffer,
                                               short inputLength) {
    if (inputLength != 15) return (byte)0x79;
    try {
      retainedCipher = Cipher.getInstance(
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
      byte[] iv = new byte[16];
      retainedCipher.init(retainedKey, Cipher.MODE_ENCRYPT,
                          iv, (short)0, (short)iv.length);
      short produced = retainedCipher.update(
          apduBuffer, ISO7816.OFFSET_CDATA, inputLength,
          new byte[16], (short)0);
      return produced == 0 ? (byte)1 : (byte)0x7a;
    } catch (CryptoException exception) {
      return (byte)(0x40 | exception.getReason());
    }
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
    } else if (instruction == (byte)0x0E) {
      try {
        result = testCipherBufferSurvivesAbort();
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x6e;
      }
    } else if (instruction == (byte)0x0F) {
      try {
        retainedCipher.doFinal(new byte[0], (short)0, (short)0,
                               new byte[16], (short)0);
        result = 0x75;
      } catch (CryptoException exception) {
        result = (byte)(0x40 | exception.getReason());
      } catch (Exception exception) {
        result = 0x6f;
      }
    } else if (instruction == (byte)0x10) {
      result = testRsaCipherAfterRestore();
    } else if (instruction == (byte)0x11) {
      result = testBufferedCipherInput(buffer, apdu.setIncomingAndReceive());
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
