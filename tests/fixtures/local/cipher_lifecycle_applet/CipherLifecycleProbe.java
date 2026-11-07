package cipherlife;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.security.AESKey;
import javacard.security.CryptoException;
import javacard.security.DESKey;
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
  private static AESKey retainedSensitiveKey;
  private static Cipher retainedSensitiveCipher;

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

  private static byte testGrownCbcBufferSurvivesAbort() {
    byte[] input = new byte[49];
    byte[] output = new byte[64];
    byte[] expectedLastBlock = {
        0x48, 0x33, 0x6b, 0x3b, (byte)0xe2, 0x63, (byte)0xe5, (byte)0x96,
        (byte)0x89, (byte)0x84, 0x6c, (byte)0xad, 0x1e, (byte)0xaf,
        0x5d, 0x21};
    byte[] iv = new byte[16];
    for (short i = 0; i < 49; i++) input[i] = (byte)i;
    try {
      retainedCipher = Cipher.getInstance(
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
      retainedCipher.init(retainedKey, Cipher.MODE_ENCRYPT,
                          iv, (short)0, (short)iv.length);
      JCSystem.beginTransaction();
      short updateLength;
      try {
        updateLength = retainedCipher.update(input, (short)0, (short)49,
                                             output, (short)0);
      } catch (CryptoException exception) {
        JCSystem.abortTransaction();
        return (byte)0x7b;
      }
      JCSystem.abortTransaction();
      byte[] finalInput = new byte[15];
      for (short i = 0; i < 15; i++) finalInput[i] = (byte)(49 + i);
      short finalLength = retainedCipher.doFinal(
          finalInput, (short)0, (short)15, output, (short)0);
      if (updateLength != 48 || finalLength != 16) return (byte)0x7c;
      for (short i = 0; i < 16; i++) {
        if (output[i] != expectedLastBlock[i]) return (byte)0x7d;
      }
      return 1;
    } catch (CryptoException exception) {
      if (JCSystem.getTransactionDepth() != 0) JCSystem.abortTransaction();
      return (byte)(0x40 | exception.getReason());
    }
  }

  private static byte testCtrBufferSurvivesAbort() {
    byte[] input = new byte[32];
    byte[] output = new byte[32];
    byte[] iv = new byte[16];
    byte[] expected = {
        (byte)0xf7, (byte)0xca, (byte)0x9f, (byte)0xfb, (byte)0xa1, 0x78,
        (byte)0x8a, 0x7f, 0x4f, 0x51, (byte)0xa0, (byte)0xeb,
        (byte)0x81, (byte)0xd4, (byte)0x97, 0x67, 0x7b, 0x6a, 0x4d,
        0x7c, 0x3b, (byte)0x88, 0x25, (byte)0xee, (byte)0x81, (byte)0x8d,
        0x0c, (byte)0xae, (byte)0x4c, (byte)0xf0, 0x2c, 0x5b};
    for (short i = 0; i < 16; i++) iv[i] = (byte)i;
    for (short i = 0; i < 32; i++) input[i] = (byte)i;
    try {
      retainedCipher = Cipher.getInstance(Cipher.ALG_AES_CTR, false);
      retainedCipher.init(retainedKey, Cipher.MODE_ENCRYPT,
                          iv, (short)0, (short)iv.length);
      JCSystem.beginTransaction();
      short firstLength;
      try {
        firstLength = retainedCipher.update(input, (short)0, (short)13,
                                            output, (short)0);
      } catch (CryptoException exception) {
        JCSystem.abortTransaction();
        return (byte)0x7e;
      }
      JCSystem.abortTransaction();
      short finalLength = retainedCipher.doFinal(input, (short)13, (short)19,
                                                  output, (short)0);
      if (firstLength != 13 || finalLength != 19) return (byte)0x7f;
      for (short i = 0; i < 19; i++) {
        if (output[i] != expected[(short)(13 + i)]) return (byte)0x7a;
      }
      return 1;
    } catch (CryptoException exception) {
      if (JCSystem.getTransactionDepth() != 0) JCSystem.abortTransaction();
      return (byte)(0x40 | exception.getReason());
    }
  }

  private static byte testSensitiveWorkspaceNotPersisted(byte[] apduBuffer,
                                                          short inputLength) {
    if (inputLength != 13) return (byte)0x6d;
    try {
      byte[] keyBytes = JCSystem.makeTransientByteArray(
          (short)16, JCSystem.CLEAR_ON_RESET);
      byte[] iv = JCSystem.makeTransientByteArray(
          (short)16, JCSystem.CLEAR_ON_RESET);
      byte[] output = JCSystem.makeTransientByteArray(
          (short)16, JCSystem.CLEAR_ON_RESET);
      for (short i = 0; i < 16; i++) {
        keyBytes[i] = (byte)(0xb0 + i);
        iv[i] = (byte)(0x60 + i);
      }
      retainedSensitiveKey = (AESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_AES_TRANSIENT_RESET,
          KeyBuilder.LENGTH_AES_128, false);
      retainedSensitiveKey.setKey(keyBytes, (short)0);
      retainedSensitiveCipher = Cipher.getInstance(Cipher.ALG_AES_CTR, false);
      retainedSensitiveCipher.init(retainedSensitiveKey, Cipher.MODE_ENCRYPT,
                                   iv, (short)0, (short)iv.length);
      short produced = retainedSensitiveCipher.update(
          apduBuffer, ISO7816.OFFSET_CDATA, inputLength, output, (short)0);
      return produced == inputLength ? (byte)1 : (byte)0x6c;
    } catch (CryptoException exception) {
      return (byte)(0x40 | exception.getReason());
    }
  }

  private static byte testEmptyFinalAfterAlignedUpdate() {
    byte[] aesExpected = {
        (byte)0xf7, (byte)0xcb, (byte)0x9d, (byte)0xf8, (byte)0xa5, 0x7d,
        (byte)0x8c, 0x78, 0x47, 0x58, (byte)0xaa, (byte)0xe0, (byte)0x8d,
        (byte)0xd9, (byte)0x99, 0x68};
    byte[] aesExpectedSecond = {
        (byte)0xa7, (byte)0x8f, (byte)0xf1, (byte)0xbe, 0x46, 0x34,
        0x40, (byte)0xc0, (byte)0x9a, 0x59, (byte)0xbd, (byte)0xf7,
        (byte)0x97, (byte)0x98, (byte)0xf6, 0x7b};
    byte[] desExpected = {
        (byte)0xde, 0x60, 0x5c, (byte)0xc9, (byte)0xf0, (byte)0x8f,
        0x67, 0x6f, (byte)0xc5, 0x32, 0x62, 0x15, (byte)0xd1, 0x4a,
        (byte)0xa9, 0x30};
    byte[] desExpectedSecond = {
        (byte)0xc5, 0x32, 0x62, 0x15, (byte)0xd1, 0x4a, (byte)0xa9, 0x30};
    byte[] keyBytes = {
        0x13, 0x34, 0x57, 0x79, (byte)0x9b, (byte)0xbc,
        (byte)0xdf, (byte)0xf1};
    try {
      DESKey desKey = (DESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_DES, KeyBuilder.LENGTH_DES, false);
      desKey.setKey(keyBytes, (short)0);
      byte[] algorithms = {
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, Cipher.ALG_AES_CBC_ISO9797_M1,
          Cipher.ALG_DES_CBC_NOPAD, Cipher.ALG_DES_CBC_ISO9797_M1};
      for (short algorithmIndex = 0; algorithmIndex < 4; algorithmIndex++) {
        boolean aes = algorithmIndex < 2;
        short blockSize = (short)(aes ? 16 : 8);
        Key key = aes ? retainedKey : desKey;
        byte[] plaintext = new byte[(short)(blockSize * 2)];
        byte[] iv = new byte[blockSize];
        byte[] output = new byte[(short)(blockSize * 3)];
        for (short i = 0; i < plaintext.length; i++) plaintext[i] = (byte)i;
        Cipher cipher = Cipher.getInstance(algorithms[algorithmIndex], false);
        cipher.init(key, Cipher.MODE_ENCRYPT, iv, (short)0, blockSize);
        short firstUpdate = cipher.update(plaintext, (short)0, blockSize,
                                          output, (short)0);
        short secondUpdate = cipher.update(plaintext, blockSize, blockSize,
                                           output, blockSize);
        short finalLength = cipher.doFinal(
            new byte[0], (short)0, (short)0, output,
            (short)(blockSize * 2));
        if (firstUpdate != blockSize || secondUpdate != blockSize ||
            finalLength != 0) return (byte)0x65;
        byte[] firstExpected = aes ? aesExpected : desExpected;
        byte[] secondExpected = aes ? aesExpectedSecond : desExpectedSecond;
        for (short i = 0; i < blockSize; i++) {
          if (output[i] != firstExpected[i])
            return (byte)(0x70 + algorithmIndex);
          if (output[(short)(blockSize + i)] != secondExpected[i])
            return (byte)(0x74 + algorithmIndex);
        }
        short reusedLength = cipher.doFinal(plaintext, (short)0, blockSize,
                                             output, (short)0);
        if (reusedLength != blockSize) return (byte)0x67;
        for (short i = 0; i < blockSize; i++)
          if (output[i] != firstExpected[i]) return (byte)0x68;
      }
      return 1;
    } catch (CryptoException exception) {
      return (byte)(0x40 | exception.getReason());
    }
  }

  private static byte verifyInvalidMethod2Tail(Key key, byte noPadAlgorithm,
                                                byte method2Algorithm,
                                                short blockSize,
                                                byte[] plaintext) {
    byte[] iv = new byte[blockSize];
    byte[] ciphertext = new byte[(short)(blockSize * 2)];
    Cipher noPadding = Cipher.getInstance(noPadAlgorithm, false);
    noPadding.init(key, Cipher.MODE_ENCRYPT, iv, (short)0, blockSize);
    short ciphertextLength = noPadding.doFinal(
        plaintext, (short)0, (short)plaintext.length, ciphertext, (short)0);
    if (ciphertextLength != plaintext.length) return (byte)0x69;

    Cipher method2 = Cipher.getInstance(method2Algorithm, false);
    method2.init(key, Cipher.MODE_DECRYPT, iv, (short)0, blockSize);
    try {
      method2.doFinal(ciphertext, (short)0, ciphertextLength,
                      new byte[(short)(blockSize * 2)], (short)0);
      return (byte)0x6a;
    } catch (CryptoException exception) {
      if (exception.getReason() != CryptoException.ILLEGAL_USE)
        return (byte)(0x40 | exception.getReason());
    }

    Cipher paddedEncrypt = Cipher.getInstance(method2Algorithm, false);
    paddedEncrypt.init(key, Cipher.MODE_ENCRYPT, iv, (short)0, blockSize);
    byte[] validCiphertext = new byte[blockSize];
    short validLength = paddedEncrypt.doFinal(
        new byte[0], (short)0, (short)0, validCiphertext, (short)0);
    if (validLength != blockSize) return (byte)0x6b;
    method2.init(key, Cipher.MODE_DECRYPT, iv, (short)0, blockSize);
    short resultLength = method2.doFinal(validCiphertext, (short)0,
        validLength, new byte[blockSize], (short)0);
    return resultLength == 0 ? (byte)1 : (byte)0x6c;
  }

  private static byte testMethod2PaddingBounds() {
    try {
      AESKey aesKey = (AESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
      byte[] aesKeyBytes = new byte[16];
      for (short i = 0; i < 16; i++) aesKeyBytes[i] = (byte)(0x10 + i);
      aesKey.setKey(aesKeyBytes, (short)0);
      DESKey desKey = (DESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_DES, KeyBuilder.LENGTH_DES, false);
      byte[] desKeyBytes = {0x13, 0x34, 0x57, 0x79, (byte)0x9b, (byte)0xbc,
                            (byte)0xdf, (byte)0xf1};
      desKey.setKey(desKeyBytes, (short)0);

      byte[] aesEarlyMarker = new byte[32];
      aesEarlyMarker[0] = (byte)0x80;
      byte[] aesMissingMarker = new byte[32];
      aesMissingMarker[31] = 1;
      byte[] aesAllZero = new byte[32];
      byte[] desEarlyMarker = new byte[16];
      desEarlyMarker[0] = (byte)0x80;
      byte[] desMissingMarker = new byte[16];
      desMissingMarker[15] = 1;
      byte[] desAllZero = new byte[16];

      byte result = verifyInvalidMethod2Tail(aesKey,
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD,
          Cipher.ALG_AES_CBC_ISO9797_M2, (short)16, aesEarlyMarker);
      if (result != 1) return (byte)(0x70 | (result & 0x0f));
      result = verifyInvalidMethod2Tail(aesKey,
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD,
          Cipher.ALG_AES_CBC_ISO9797_M2, (short)16, aesMissingMarker);
      if (result != 1) return (byte)(0x71 | (result & 0x0f));
      result = verifyInvalidMethod2Tail(aesKey,
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD,
          Cipher.ALG_AES_CBC_ISO9797_M2, (short)16, aesAllZero);
      if (result != 1) return (byte)(0x72 | (result & 0x0f));
      result = verifyInvalidMethod2Tail(desKey,
          Cipher.ALG_DES_CBC_NOPAD, Cipher.ALG_DES_CBC_ISO9797_M2,
          (short)8, desEarlyMarker);
      if (result != 1) return (byte)(0x73 | (result & 0x0f));
      result = verifyInvalidMethod2Tail(desKey,
          Cipher.ALG_DES_CBC_NOPAD, Cipher.ALG_DES_CBC_ISO9797_M2,
          (short)8, desMissingMarker);
      if (result != 1) return (byte)(0x74 | (result & 0x0f));
      result = verifyInvalidMethod2Tail(desKey,
          Cipher.ALG_DES_CBC_NOPAD, Cipher.ALG_DES_CBC_ISO9797_M2,
          (short)8, desAllZero);
      if (result != 1) return (byte)(0x75 | (result & 0x0f));
      return 1;
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
    } else if (instruction == (byte)0x12) {
      result = testGrownCbcBufferSurvivesAbort();
    } else if (instruction == (byte)0x13) {
      result = testCtrBufferSurvivesAbort();
    } else if (instruction == (byte)0x14) {
      result = testSensitiveWorkspaceNotPersisted(
          buffer, apdu.setIncomingAndReceive());
    } else if (instruction == (byte)0x15) {
      result = testEmptyFinalAfterAlignedUpdate();
    } else if (instruction == (byte)0x16) {
      result = testMethod2PaddingBounds();
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
