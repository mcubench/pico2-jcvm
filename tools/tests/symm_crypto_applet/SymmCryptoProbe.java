package symmtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.security.AESKey;
import javacard.security.DESKey;
import javacard.security.Key;
import javacard.security.KeyBuilder;
import javacard.security.MessageDigest;
import javacard.security.RSAPrivateCrtKey;
import javacard.security.RSAPrivateKey;
import javacard.security.RSAPublicKey;
import javacard.security.Signature;
import javacardx.crypto.Cipher;

/** Test applet that exposes one deterministic cipher or MAC operation per APDU. */
public final class SymmCryptoProbe extends Applet {
  private SymmCryptoProbe() { register(); }

  public static void install(byte[] data, short offset, byte length) {
    new SymmCryptoProbe();
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte algorithm = buffer[ISO7816.OFFSET_P1];
    byte operation = buffer[ISO7816.OFFSET_P2];
    short inputLength = apdu.setIncomingAndReceive();
    short inputOffset = ISO7816.OFFSET_CDATA;
    byte[] input = new byte[inputLength];
    for (short i = 0; i < inputLength; i++) input[i] = buffer[(short)(inputOffset + i)];
    byte[] output;
    short outputLength;
    byte instruction = buffer[ISO7816.OFFSET_INS];
    if (instruction == (byte)0x12) {
      output = new byte[(short)64];
      outputLength = runDigest(algorithm, input, inputLength, output);
    } else if (instruction == (byte)0x13) {
      output = new byte[(short)1];
      output[0] = runRsaKey(algorithm) ? (byte)1 : (byte)0;
      outputLength = (short)1;
    } else if (instruction == (byte)0x10) {
      output = operation == 1 && !isNoPadCipher(algorithm) &&
          !isIso9797M1(algorithm)
          ? new byte[(short)3] : new byte[(short)(inputLength + 32)];
      outputLength = runCipher(algorithm, operation, input, inputLength, output);
    } else {
      output = new byte[(short)32];
      outputLength = runMac(algorithm, operation, input, inputLength, output);
    }
    apdu.setOutgoing();
    apdu.setOutgoingLength(outputLength);
    apdu.sendBytesLong(output, (short)0, outputLength);
  }

  private static short runDigest(byte algorithm, byte[] input,
                                 short inputLength, byte[] output) {
    MessageDigest digest = MessageDigest.getInstance(algorithm, false);
    short split = (short)(inputLength / 2);
    if (split > 0) digest.update(input, (short)0, split);
    return digest.doFinal(input, split, (short)(inputLength - split), output,
                          (short)0);
  }

  private static boolean runRsaKey(byte selector) {
    byte[] modulus = new byte[(short)64];
    byte[] exponent = {(byte)0x01, (byte)0x00, (byte)0x01};
    for (short i = 0; i < modulus.length; i++) modulus[i] = (byte)(i + 1);
    modulus[0] = (byte)0x7f;
    if (selector == 0 || selector == 1 || selector == 3) {
      byte type = selector == 0 ? KeyBuilder.TYPE_RSA_PUBLIC :
          selector == 3 ? KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_RESET :
          KeyBuilder.TYPE_RSA_PRIVATE;
      Key key = KeyBuilder.buildKey(type, KeyBuilder.LENGTH_RSA_512, false);
      if (selector == 0) {
        RSAPublicKey rsa = (RSAPublicKey)key;
        rsa.setModulus(modulus, (short)0, (short)modulus.length);
        rsa.setExponent(exponent, (short)0, (short)exponent.length);
        if (!rsa.isInitialized()) return false;
        byte[] output = new byte[(short)64];
        return rsa.getModulus(output, (short)0) == 64 &&
            equal(modulus, output, (short)64) &&
            rsa.getExponent(output, (short)0) == 3 &&
            output[0] == 1 && output[1] == 0 && output[2] == 1;
      }
      RSAPrivateKey rsa = (RSAPrivateKey)key;
      rsa.setModulus(modulus, (short)0, (short)modulus.length);
      rsa.setExponent(modulus, (short)0, (short)modulus.length);
      if (!rsa.isInitialized()) return false;
      byte[] output = new byte[(short)64];
      return rsa.getModulus(output, (short)0) == 64 &&
          equal(modulus, output, (short)64) &&
          rsa.getExponent(output, (short)0) == 64 &&
          equal(modulus, output, (short)64);
    }
    if (selector == 2 || selector == 4) {
      byte type = selector == 2 ? KeyBuilder.TYPE_RSA_CRT_PRIVATE :
          KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT;
      RSAPrivateCrtKey rsa = (RSAPrivateCrtKey)KeyBuilder.buildKey(
          type, KeyBuilder.LENGTH_RSA_512, false);
      byte[] component = new byte[(short)32];
      for (short i = 0; i < component.length; i++) component[i] = (byte)(i + 2);
      rsa.setP(component, (short)0, (short)component.length);
      rsa.setQ(component, (short)0, (short)component.length);
      rsa.setDP1(component, (short)0, (short)component.length);
      rsa.setDQ1(component, (short)0, (short)component.length);
      rsa.setPQ(component, (short)0, (short)component.length);
      if (!rsa.isInitialized()) return false;
      byte[] output = new byte[(short)32];
      return rsa.getP(output, (short)0) == 32 && equal(component, output, (short)32) &&
          rsa.getQ(output, (short)0) == 32 && equal(component, output, (short)32) &&
          rsa.getDP1(output, (short)0) == 32 && equal(component, output, (short)32) &&
          rsa.getDQ1(output, (short)0) == 32 && equal(component, output, (short)32) &&
          rsa.getPQ(output, (short)0) == 32 && equal(component, output, (short)32);
    }
    return false;
  }

  private static boolean equal(byte[] left, byte[] right, short length) {
    for (short i = 0; i < length; i++) if (left[i] != right[i]) return false;
    return true;
  }

  private static short runCipher(byte algorithm, byte operation, byte[] input,
                                 short inputLength, byte[] output) {
    boolean aes = algorithm == 13 || algorithm == 14 || algorithm >= 22;
    short keyLength = (short)(aes ? KeyBuilder.LENGTH_AES_128 : KeyBuilder.LENGTH_DES);
    Key key = KeyBuilder.buildKey(aes ? KeyBuilder.TYPE_AES : KeyBuilder.TYPE_DES,
                                  keyLength, false);
    if (aes) {
      byte[] keyBytes = new byte[16];
      for (short i = 0; i < keyBytes.length; i++) keyBytes[i] = (byte)(i + 1);
      ((AESKey)key).setKey(keyBytes, (short)0);
    } else {
      byte[] keyBytes = {
          (byte)0x13, (byte)0x34, (byte)0x57, (byte)0x79,
          (byte)0x9b, (byte)0xbc, (byte)0xdf, (byte)0xf1};
      ((DESKey)key).setKey(keyBytes, (short)0);
    }
    Cipher cipher = Cipher.getInstance(algorithm, false);
    byte mode = operation == 0 ? Cipher.MODE_ENCRYPT : Cipher.MODE_DECRYPT;
    if (isCbc(algorithm)) {
      byte[] iv = new byte[(short)(aes ? 16 : 8)];
      cipher.init(key, mode, iv, (short)0, (short)iv.length);
    } else {
      cipher.init(key, mode);
    }
    return cipher.doFinal(input, (short)0, inputLength, output, (short)0);
  }

  private static boolean isCbc(byte algorithm) {
    return algorithm == 1 || algorithm == 2 || algorithm == 3 || algorithm == 4 ||
        algorithm == 13 || algorithm == 22 || algorithm == 23 || algorithm == 24;
  }

  private static boolean isNoPadCipher(byte algorithm) {
    return algorithm == 1 || algorithm == 5 || algorithm == 13 || algorithm == 14;
  }

  private static boolean isIso9797M1(byte algorithm) {
    return algorithm == 2 || algorithm == 6 || algorithm == 22 || algorithm == 25;
  }

  private static short runMac(byte algorithm, byte operation, byte[] input,
                              short inputLength, byte[] output) {
    boolean aes = algorithm == Signature.ALG_AES_MAC_128_NOPAD ||
        algorithm == Signature.ALG_AES_CMAC_128;
    short keyLength = (short)(aes ? KeyBuilder.LENGTH_AES_128 :
        (algorithm == Signature.ALG_DES_MAC4_ISO9797_1_M2_ALG3 ||
         algorithm == Signature.ALG_DES_MAC8_ISO9797_1_M2_ALG3 ||
         algorithm == Signature.ALG_DES_MAC4_ISO9797_1_M1_ALG3 ||
         algorithm == Signature.ALG_DES_MAC8_ISO9797_1_M1_ALG3
             ? KeyBuilder.LENGTH_DES3_2KEY : KeyBuilder.LENGTH_DES));
    Key key = KeyBuilder.buildKey(aes ? KeyBuilder.TYPE_AES : KeyBuilder.TYPE_DES,
                                  keyLength, false);
    if (aes) {
      byte[] keyBytes = {
          (byte)0x2b, (byte)0x7e, (byte)0x15, (byte)0x16,
          (byte)0x28, (byte)0xae, (byte)0xd2, (byte)0xa6,
          (byte)0xab, (byte)0xf7, (byte)0x15, (byte)0x88,
          (byte)0x09, (byte)0xcf, (byte)0x4f, (byte)0x3c};
      ((AESKey)key).setKey(keyBytes, (short)0);
    } else {
      byte[] keyBytes = new byte[keyLength == KeyBuilder.LENGTH_DES ? (short)8 : (short)16];
      for (short i = 0; i < keyBytes.length; i++) keyBytes[i] = (byte)(i + 1);
      ((DESKey)key).setKey(keyBytes, (short)0);
    }
    Signature signature = Signature.getInstance(algorithm, false);
    signature.init(key, operation == 0 ? Signature.MODE_SIGN : Signature.MODE_VERIFY);
    if (operation == 0)
      return signature.sign(input, (short)0, inputLength, output, (short)0);
    short signatureLength = signature.getLength();
    short messageLength = (short)(inputLength - signatureLength);
    if (messageLength < 0) ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    output[0] = signature.verify(input, (short)0, messageLength, input,
        messageLength, signatureLength) ? (byte)1 : (byte)0;
    return 1;
  }
}
