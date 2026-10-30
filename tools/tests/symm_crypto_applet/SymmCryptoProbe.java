package symmtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.security.AESKey;
import javacard.security.DESKey;
import javacard.security.ECPrivateKey;
import javacard.security.ECPublicKey;
import javacard.security.CryptoException;
import javacard.security.Key;
import javacard.security.KeyBuilder;
import javacard.security.KeyAgreement;
import javacard.security.KeyPair;
import javacard.security.MessageDigest;
import javacard.security.RSAPrivateCrtKey;
import javacard.security.RSAPrivateKey;
import javacard.security.RSAPublicKey;
import javacard.security.Signature;
import javacard.security.SignatureMessageRecovery;
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
    } else if (instruction == (byte)0x14) {
      output = new byte[(short)64];
      outputLength = runRsaCipher(algorithm, input, output);
    } else if (instruction == (byte)0x15) {
      output = new byte[(short)128];
      outputLength = runRsaSignature(algorithm, operation, input, output);
    } else if (instruction == (byte)0x16) {
      output = new byte[(short)1];
      output[0] = runEcKey(algorithm);
      outputLength = (short)1;
    } else if (instruction == (byte)0x17) {
      output = new byte[(short)1];
      output[0] = runEcdsa(algorithm, input, inputLength);
      outputLength = (short)1;
    } else if (instruction == (byte)0x18) {
      output = new byte[(short)133];
      byte options = inputLength > 0 ? input[0] : operation;
      outputLength = runKeyAgreement(algorithm, options, output);
    } else if (instruction == (byte)0x19) {
      output = new byte[(short)1];
      output[0] = runEcKeyPair(algorithm);
      outputLength = (short)1;
    } else if (instruction == (byte)0x1a) {
      output = new byte[(short)1];
      output[0] = runRsaKeyPair(algorithm);
      outputLength = (short)1;
    } else if (instruction == (byte)0x10) {
      output = (operation == 1 || operation == 5) && !isNoPadCipher(algorithm) &&
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

  private static byte runEcKey(byte selector) {
    short length = selector == 0 ? KeyBuilder.LENGTH_EC_FP_256 :
        KeyBuilder.LENGTH_EC_FP_521;
    byte type = selector == 1 ? KeyBuilder.TYPE_EC_FP_PRIVATE_TRANSIENT_RESET :
        selector == 2 ? KeyBuilder.TYPE_EC_FP_PRIVATE_TRANSIENT_DESELECT :
        KeyBuilder.TYPE_EC_FP_PRIVATE;
    ECPrivateKey key = (ECPrivateKey)KeyBuilder.buildKey(type, length, false);
    if (key.isInitialized()) return (byte)10;
    short bytes = (short)((length + 7) / 8);
    byte[] field = new byte[bytes];
    byte[] a = new byte[bytes];
    byte[] b = new byte[bytes];
    byte[] generator = new byte[(short)(1 + 2 * bytes)];
    byte[] order = new byte[bytes];
    byte[] scalar = new byte[bytes];
    for (short i = 0; i < bytes; i++) {
      field[i] = (byte)(0x11 + i);
      a[i] = (byte)(0x22 + i);
      b[i] = (byte)(0x33 + i);
      order[i] = (byte)(0x44 + i);
      scalar[i] = (byte)(0x55 + i);
    }
    generator[0] = 4;
    for (short i = 1; i < generator.length; i++) generator[i] = (byte)(0x66 + i);
    key.setFieldFP(field, (short)0, bytes);
    key.setA(a, (short)0, bytes);
    key.setB(b, (short)0, bytes);
    key.setG(generator, (short)0, (short)generator.length);
    key.setR(order, (short)0, bytes);
    key.setK((short)1);
    key.setS(scalar, (short)0, bytes);
    if (!key.isInitialized() || key.getK() != 1) return (byte)20;
    byte[] result = new byte[(short)133];
    if (key.getField(result, (short)0) != bytes || !equal(field, result, bytes)) return (byte)31;
    if (key.getA(result, (short)0) != bytes || !equal(a, result, bytes)) return (byte)32;
    if (key.getB(result, (short)0) != bytes || !equal(b, result, bytes)) return (byte)33;
    if (key.getG(result, (short)0) != generator.length ||
        !equal(generator, result, (short)generator.length)) return (byte)34;
    if (key.getR(result, (short)0) != bytes || !equal(order, result, bytes)) return (byte)35;
    if (key.getS(result, (short)0) != bytes || !equal(scalar, result, bytes)) return (byte)36;
    ECPrivateKey copy = (ECPrivateKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_EC_FP_PRIVATE, length, false);
    copy.copyDomainParametersFrom(key);
    boolean copied = copy.getK() == 1 && copy.getField(result, (short)0) == bytes &&
        equal(field, result, bytes) && copy.getG(result, (short)0) == generator.length &&
        equal(generator, result, (short)generator.length) && !copy.isInitialized();
    key.clearKey();
    if (!copied) return (byte)40;
    if (key.isInitialized()) return (byte)41;
    return 1;
  }

  private static byte runEcdsa(byte algorithm, byte[] message, short messageLength) {
    byte[] p = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff};
    byte[] a = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xfc};
    byte[] b = {(byte)0x5a,(byte)0xc6,0x35,(byte)0xd8,(byte)0xaa,0x3a,(byte)0x93,(byte)0xe7,(byte)0xb3,(byte)0xeb,(byte)0xbd,0x55,0x76,(byte)0x98,(byte)0x86,(byte)0xbc,0x65,0x1d,0x06,(byte)0xb0,(byte)0xcc,0x53,(byte)0xb0,(byte)0xf6,0x3b,(byte)0xce,0x3c,0x3e,0x27,(byte)0xd2,0x60,0x4b};
    byte[] g = {4,0x6b,0x17,(byte)0xd1,(byte)0xf2,(byte)0xe1,0x2c,0x42,0x47,(byte)0xf8,(byte)0xbc,(byte)0xe6,(byte)0xe5,0x63,(byte)0xa4,0x40,(byte)0xf2,0x77,0x03,0x7d,(byte)0x81,0x2d,(byte)0xeb,0x33,(byte)0xa0,(byte)0xf4,(byte)0xa1,0x39,0x45,(byte)0xd8,(byte)0x98,(byte)0xc2,(byte)0x96,0x4f,(byte)0xe3,0x42,(byte)0xe2,(byte)0xfe,0x1a,0x7f,(byte)0x9b,(byte)0x8e,(byte)0xe7,(byte)0xeb,0x4a,0x7c,0x0f,(byte)0x9e,0x16,0x2b,(byte)0xce,0x33,0x57,0x6b,0x31,0x5e,(byte)0xce,(byte)0xcb,(byte)0xb6,0x40,0x68,0x37,(byte)0xbf,0x51,(byte)0xf5};
    byte[] n = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xbc,(byte)0xe6,(byte)0xfa,(byte)0xad,(byte)0xa7,0x17,(byte)0x9e,(byte)0x84,(byte)0xf3,(byte)0xb9,(byte)0xca,(byte)0xc2,(byte)0xfc,0x63,0x25,0x51};
    byte[] scalar = {1};
    ECPrivateKey privateKey = (ECPrivateKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_EC_FP_PRIVATE, KeyBuilder.LENGTH_EC_FP_256, false);
    ECPublicKey publicKey = (ECPublicKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_EC_FP_PUBLIC, KeyBuilder.LENGTH_EC_FP_256, false);
    setEcDomain(privateKey, p, a, b, g, n);
    setEcDomain(publicKey, p, a, b, g, n);
    privateKey.setS(scalar, (short)0, (short)scalar.length);
    publicKey.setW(g, (short)0, (short)g.length);
    Signature signature = Signature.getInstance(algorithm, false);
    signature.init(privateKey, Signature.MODE_SIGN);
    byte[] signed = new byte[(short)80];
    short signedLength = signature.sign(message, (short)0, messageLength,
        signed, (short)0);
    signature.init(publicKey, Signature.MODE_VERIFY);
    return signature.verify(message, (short)0, messageLength, signed,
        (short)0, signedLength) ? (byte)1 : (byte)2;
  }

  private static void setEcDomain(ECPrivateKey key, byte[] p, byte[] a,
      byte[] b, byte[] g, byte[] n) {
    setEcDomain(key, p, a, b, g, n, true);
  }

  private static void setEcDomain(ECPrivateKey key, byte[] p, byte[] a,
      byte[] b, byte[] g, byte[] n, boolean initializeCofactor) {
    key.setFieldFP(p, (short)0, (short)p.length);
    key.setA(a, (short)0, (short)a.length);
    key.setB(b, (short)0, (short)b.length);
    key.setG(g, (short)0, (short)g.length);
    key.setR(n, (short)0, (short)n.length);
    if (initializeCofactor) key.setK((short)1);
  }

  private static void setEcDomain(ECPublicKey key, byte[] p, byte[] a,
      byte[] b, byte[] g, byte[] n) {
    key.setFieldFP(p, (short)0, (short)p.length);
    key.setA(a, (short)0, (short)a.length);
    key.setB(b, (short)0, (short)b.length);
    key.setG(g, (short)0, (short)g.length);
    key.setR(n, (short)0, (short)n.length);
    key.setK((short)1);
  }

  private static short runKeyAgreement(byte algorithm, byte options,
                                       byte[] output) {
    byte[] p = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff};
    byte[] a = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xfc};
    byte[] b = {(byte)0x5a,(byte)0xc6,0x35,(byte)0xd8,(byte)0xaa,0x3a,(byte)0x93,(byte)0xe7,(byte)0xb3,(byte)0xeb,(byte)0xbd,0x55,0x76,(byte)0x98,(byte)0x86,(byte)0xbc,0x65,0x1d,0x06,(byte)0xb0,(byte)0xcc,0x53,(byte)0xb0,(byte)0xf6,0x3b,(byte)0xce,0x3c,0x3e,0x27,(byte)0xd2,0x60,0x4b};
    byte[] g = {4,0x6b,0x17,(byte)0xd1,(byte)0xf2,(byte)0xe1,0x2c,0x42,0x47,(byte)0xf8,(byte)0xbc,(byte)0xe6,(byte)0xe5,0x63,(byte)0xa4,0x40,(byte)0xf2,0x77,0x03,0x7d,(byte)0x81,0x2d,(byte)0xeb,0x33,(byte)0xa0,(byte)0xf4,(byte)0xa1,0x39,0x45,(byte)0xd8,(byte)0x98,(byte)0xc2,(byte)0x96,0x4f,(byte)0xe3,0x42,(byte)0xe2,(byte)0xfe,0x1a,0x7f,(byte)0x9b,(byte)0x8e,(byte)0xe7,(byte)0xeb,0x4a,0x7c,0x0f,(byte)0x9e,0x16,0x2b,(byte)0xce,0x33,0x57,0x6b,0x31,0x5e,(byte)0xce,(byte)0xcb,(byte)0xb6,0x40,0x68,0x37,(byte)0xbf,0x51,(byte)0xf5};
    byte[] n = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xbc,(byte)0xe6,(byte)0xfa,(byte)0xad,(byte)0xa7,0x17,(byte)0x9e,(byte)0x84,(byte)0xf3,(byte)0xb9,(byte)0xca,(byte)0xc2,(byte)0xfc,0x63,0x25,0x51};
    ECPrivateKey privateKey = (ECPrivateKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_EC_FP_PRIVATE, KeyBuilder.LENGTH_EC_FP_256, false);
    setEcDomain(privateKey, p, a, b, g, n, options == 0);
    byte[] scalar = {1};
    privateKey.setS(scalar, (short)0, (short)1);
    KeyAgreement agreement = KeyAgreement.getInstance(algorithm, false);
    try {
      agreement.init(privateKey);
    } catch (CryptoException exception) {
      if (options != 0) {
        output[0] = (byte)exception.getReason();
        return (short)1;
      }
      throw exception;
    }
    return agreement.generateSecret(g, (short)0, (short)g.length, output,
        (short)0);
  }

  private static byte runEcKeyPair(byte selector) {
    short size;
    switch (selector) {
    case 0: size = KeyBuilder.LENGTH_EC_FP_192; break;
    case 1: size = KeyBuilder.LENGTH_EC_FP_224; break;
    case 2: size = KeyBuilder.LENGTH_EC_FP_256; break;
    case 3: size = KeyBuilder.LENGTH_EC_FP_384; break;
    case 4: size = KeyBuilder.LENGTH_EC_FP_521; break;
    case 5: size = KeyBuilder.LENGTH_EC_FP_128; break;
    case 6: size = KeyBuilder.LENGTH_EC_FP_160; break;
    default: return (byte)0xff;
    }
    KeyPair pair;
    try {
      pair = new KeyPair(KeyPair.ALG_EC_FP, size);
    } catch (CryptoException exception) {
      return (byte)(0x10 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x1f;
    }
    try {
      pair.genKeyPair();
    } catch (CryptoException exception) {
      return (byte)(0x20 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x2f;
    }
    ECPrivateKey privateKey;
    ECPublicKey publicKey;
    try {
      privateKey = (ECPrivateKey)pair.getPrivate();
      publicKey = (ECPublicKey)pair.getPublic();
      if (!privateKey.isInitialized() || !publicKey.isInitialized()) return (byte)0x30;
    } catch (CryptoException exception) {
      return (byte)(0x30 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x3f;
    }
    try {
      byte[] message = {(byte)0x4a,(byte)0x43,(byte)0x56,(byte)0x4d};
      Signature signature = Signature.getInstance(Signature.ALG_ECDSA_SHA_256,
          false);
      try {
        signature.init(privateKey, Signature.MODE_SIGN);
      } catch (CryptoException exception) {
        return (byte)(0x50 | exception.getReason());
      }
      byte[] signed = new byte[(short)160];
      short signedLength;
      try {
        signedLength = signature.sign(message, (short)0, (short)message.length,
            signed, (short)0);
      } catch (CryptoException exception) {
        return (byte)(0x60 | exception.getReason());
      }
      try {
        signature.init(publicKey, Signature.MODE_VERIFY);
      } catch (CryptoException exception) {
        return (byte)(0x70 | exception.getReason());
      }
      try {
        return signature.verify(message, (short)0, (short)message.length, signed,
            (short)0, signedLength) ? (byte)1 : (byte)0;
      } catch (CryptoException exception) {
        return (byte)(0x80 | exception.getReason());
      }
    } catch (CryptoException exception) {
      return (byte)(0x40 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x4f;
    }
  }

  private static byte runRsaKeyPair(byte selector) {
    byte algorithm;
    short size;
    switch (selector) {
    case 0: algorithm = KeyPair.ALG_RSA; size = KeyBuilder.LENGTH_RSA_512; break;
    case 1: algorithm = KeyPair.ALG_RSA_CRT; size = KeyBuilder.LENGTH_RSA_512; break;
    case 2: algorithm = KeyPair.ALG_RSA; size = KeyBuilder.LENGTH_RSA_1024; break;
    case 3: algorithm = KeyPair.ALG_RSA_CRT; size = KeyBuilder.LENGTH_RSA_1024; break;
    case 4: algorithm = KeyPair.ALG_RSA; size = KeyBuilder.LENGTH_RSA_3072; break;
    case 5: algorithm = KeyPair.ALG_RSA_CRT; size = KeyBuilder.LENGTH_RSA_3072; break;
    default: return (byte)0xff;
    }
    KeyPair pair;
    try {
      pair = new KeyPair(algorithm, size);
      pair.genKeyPair();
    } catch (CryptoException exception) {
      return (byte)(0x10 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x1f;
    }
    RSAPublicKey publicKey;
    Key privateKey;
    try {
      publicKey = (RSAPublicKey)pair.getPublic();
      privateKey = (Key)pair.getPrivate();
      if (!publicKey.isInitialized() || !privateKey.isInitialized()) return (byte)0x20;
      byte[] modulus = new byte[(short)(size / 8)];
      publicKey.getModulus(modulus, (short)0);
      byte[] exponent = new byte[(short)(size / 8)];
      publicKey.getExponent(exponent, (short)0);
      if (!containsNonzero(modulus) || exponent[(short)(exponent.length - 1)] != 1 ||
          exponent[(short)(exponent.length - 2)] != 0 ||
          exponent[(short)(exponent.length - 3)] != 1) return (byte)0x21;
      if (algorithm == KeyPair.ALG_RSA) {
        RSAPrivateKey rsaPrivate = (RSAPrivateKey)privateKey;
        byte[] privateExponent = new byte[(short)(size / 8)];
        rsaPrivate.getExponent(privateExponent, (short)0);
        if (!containsNonzero(privateExponent)) return (byte)0x22;
        Signature signature = Signature.getInstance(Signature.ALG_RSA_SHA_PKCS1,
            false);
        byte[] message = {(byte)0x4a,(byte)0x43,(byte)0x56,(byte)0x4d};
        byte[] signed = new byte[(short)(size / 8)];
        signature.init(rsaPrivate, Signature.MODE_SIGN);
        short signedLength = signature.sign(message, (short)0,
            (short)message.length, signed, (short)0);
        signature.init(publicKey, Signature.MODE_VERIFY);
        if (!signature.verify(message, (short)0, (short)message.length, signed,
            (short)0, signedLength)) return (byte)0x23;
      } else {
        RSAPrivateCrtKey crt = (RSAPrivateCrtKey)privateKey;
        byte[] part = new byte[(short)(size / 16)];
        if (crt.getP(part, (short)0) <= 0 || !containsNonzero(part) ||
            crt.getQ(part, (short)0) <= 0 || !containsNonzero(part) ||
            crt.getDP1(part, (short)0) <= 0 || !containsNonzero(part) ||
            crt.getDQ1(part, (short)0) <= 0 || !containsNonzero(part) ||
            crt.getPQ(part, (short)0) <= 0 || !containsNonzero(part)) return (byte)0x24;
        if (size <= KeyBuilder.LENGTH_RSA_1024) {
          byte[] message = {(byte)0x43,(byte)0x52,(byte)0x54,(byte)0x21};
          byte[] signed = new byte[(short)(size / 8)];
          Signature signature = Signature.getInstance(
              Signature.ALG_RSA_SHA_PKCS1, false);
          try {
            signature.init(crt, Signature.MODE_SIGN);
          } catch (CryptoException exception) {
            return (byte)(0x40 | exception.getReason());
          }
          short signedLength;
          try {
            signedLength = signature.sign(message, (short)0,
                (short)message.length, signed, (short)0);
          } catch (CryptoException exception) {
            return (byte)(0x50 | exception.getReason());
          }
          try {
            signature.init(publicKey, Signature.MODE_VERIFY);
          } catch (CryptoException exception) {
            return (byte)(0x70 | exception.getReason());
          }
          try {
            if (!signature.verify(message, (short)0, (short)message.length,
                signed, (short)0, signedLength)) return (byte)0x25;
          } catch (CryptoException exception) {
            return (byte)(0x80 | exception.getReason());
          }

          byte[] ciphertext = new byte[(short)(size / 8)];
          Cipher cipher = Cipher.getInstance(Cipher.ALG_RSA_PKCS1, false);
          try {
            cipher.init(publicKey, Cipher.MODE_ENCRYPT);
          } catch (CryptoException exception) {
            return (byte)(0x90 | exception.getReason());
          }
          short ciphertextLength;
          try {
            ciphertextLength = cipher.doFinal(message, (short)0,
                (short)message.length, ciphertext, (short)0);
          } catch (CryptoException exception) {
            return (byte)(0xa0 | exception.getReason());
          }
          byte[] recovered = new byte[(short)(size / 8)];
          try {
            cipher.init(crt, Cipher.MODE_DECRYPT);
          } catch (CryptoException exception) {
            return (byte)(0x60 | exception.getReason());
          }
          short recoveredLength;
          try {
            recoveredLength = cipher.doFinal(ciphertext, (short)0,
                ciphertextLength, recovered, (short)0);
          } catch (CryptoException exception) {
            return (byte)(0xb0 | exception.getReason());
          }
          if (recoveredLength != message.length ||
              !equal(message, recovered, recoveredLength)) return (byte)0x26;
        }
      }
    } catch (CryptoException exception) {
      return (byte)(0x30 | exception.getReason());
    } catch (Exception exception) {
      return (byte)0x3f;
    }
    return 1;
  }

  private static boolean containsNonzero(byte[] value) {
    byte aggregate = 0;
    for (short i = 0; i < value.length; i++) aggregate |= value[i];
    return aggregate != 0;
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

  private static short runRsaCipher(byte algorithm, byte[] parameters,
                                    byte[] output) {
    if (parameters.length < 132) ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    RSAPublicKey publicKey = (RSAPublicKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_RSA_PUBLIC, KeyBuilder.LENGTH_RSA_512, false);
    RSAPrivateKey privateKey = (RSAPrivateKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_RSA_PRIVATE, KeyBuilder.LENGTH_RSA_512, false);
    publicKey.setModulus(parameters, (short)0, (short)64);
    publicKey.setExponent(parameters, (short)64, (short)3);
    privateKey.setModulus(parameters, (short)0, (short)64);
    privateKey.setExponent(parameters, (short)67, (short)64);
    short messageLength = (short)(parameters.length - 131);
    byte[] encrypted = new byte[(short)64];
    byte[] plaintext = new byte[(short)64];
    Cipher cipher = Cipher.getInstance(algorithm, false);
    cipher.init(publicKey, Cipher.MODE_ENCRYPT);
    short encryptedLength = cipher.doFinal(parameters, (short)131,
        messageLength, encrypted, (short)0);
    cipher.init(privateKey, Cipher.MODE_DECRYPT);
    short plaintextLength = cipher.doFinal(encrypted, (short)0,
        encryptedLength, plaintext, (short)0);
    if (plaintextLength != messageLength ||
        !equal(parameters, (short)131, plaintext, (short)0, messageLength))
      ISOException.throwIt(ISO7816.SW_UNKNOWN);
    for (short i = 0; i < plaintextLength; i++) output[i] = plaintext[i];
    return plaintextLength;
  }

  private static boolean equal(byte[] left, short leftOffset, byte[] right,
                               short rightOffset, short length) {
    for (short i = 0; i < length; i++)
      if (left[(short)(leftOffset + i)] != right[(short)(rightOffset + i)])
        return false;
    return true;
  }

  private static short runRsaSignature(byte algorithm, byte operation,
                                       byte[] parameters, byte[] output) {
    if (algorithm == Signature.ALG_RSA_SHA_ISO9796_MR)
      return runRsaMessageRecovery(operation, parameters, output);
    boolean pss = algorithm == Signature.ALG_RSA_SHA_PKCS1_PSS ||
        algorithm == Signature.ALG_RSA_SHA_224_PKCS1_PSS ||
        algorithm == Signature.ALG_RSA_SHA_256_PKCS1_PSS ||
        algorithm == Signature.ALG_RSA_SHA_384_PKCS1_PSS ||
        algorithm == Signature.ALG_RSA_SHA_512_PKCS1_PSS;
    short keyLength = (short)(algorithm == Signature.ALG_RSA_SHA_ISO9796 ||
        algorithm == Signature.ALG_RSA_SHA_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_PKCS1_PSS ? 64 :
        algorithm == Signature.ALG_RSA_SHA_512_PKCS1_PSS && (operation & 4) != 0
            ? 92 : algorithm == Signature.ALG_RSA_SHA_384_PKCS1_PSS ? 112 : 96);
    short hashLength = (short)(algorithm == Signature.ALG_RSA_SHA_ISO9796 ||
        algorithm == Signature.ALG_RSA_SHA_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_PKCS1_PSS ? 20 :
        algorithm == Signature.ALG_RSA_SHA_224_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_224_PKCS1_PSS ? 28 :
        algorithm == Signature.ALG_RSA_SHA_256_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_256_PKCS1_PSS ? 32 :
        algorithm == Signature.ALG_RSA_SHA_384_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_384_PKCS1_PSS ? 48 :
        algorithm == Signature.ALG_RSA_SHA_512_PKCS1 ||
        algorithm == Signature.ALG_RSA_SHA_512_PKCS1_PSS ? 64 : 0);
    if (hashLength == 0)
      ISOException.throwIt(ISO7816.SW_FUNC_NOT_SUPPORTED);
    byte baseOperation = (byte)(operation & 3);
    boolean parameterized = (operation & 4) != 0;
    boolean signing = baseOperation == 0 || baseOperation == 2;
    boolean precomputed = baseOperation == 2 || baseOperation == 3;
    short keyTypeLength = keyLength == 64 ? KeyBuilder.LENGTH_RSA_512 :
        keyLength == 92 ? KeyBuilder.LENGTH_RSA_736 :
        keyLength == 112 ? KeyBuilder.LENGTH_RSA_896 : KeyBuilder.LENGTH_RSA_768;
    Key key;
    byte[] message;
    byte[] expected = null;
    if (signing) {
      if (parameters.length < 2 * keyLength + (precomputed ? hashLength : (short)0))
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      RSAPrivateKey privateKey = (RSAPrivateKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_RSA_PRIVATE, keyTypeLength, false);
      privateKey.setModulus(parameters, (short)0, keyLength);
      privateKey.setExponent(parameters, keyLength, keyLength);
      key = privateKey;
      short messageOffset = (short)(2 * keyLength);
      short messageLength = (short)(parameters.length - messageOffset);
      if (precomputed && messageLength != hashLength)
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      message = new byte[messageLength];
      for (short i = 0; i < messageLength; i++)
        message[i] = parameters[(short)(messageOffset + i)];
    } else {
      if (parameters.length < keyLength + 1 + 3 + keyLength +
          (precomputed ? hashLength : (short)0))
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      short exponentLength = (short)(parameters[keyLength] & 0xff);
      if (exponentLength == 0 || exponentLength > 8)
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      short messageOffset = (short)(keyLength + 1 + exponentLength);
      short signatureOffset = (short)(parameters.length - keyLength);
      short messageLength = (short)(signatureOffset - messageOffset);
      if (messageLength < 0 || (precomputed && messageLength != hashLength))
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      RSAPublicKey publicKey = (RSAPublicKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_RSA_PUBLIC, keyTypeLength, false);
      publicKey.setModulus(parameters, (short)0, keyLength);
      publicKey.setExponent(parameters, (short)(keyLength + 1), exponentLength);
      key = publicKey;
      message = new byte[messageLength];
      for (short i = 0; i < messageLength; i++)
        message[i] = parameters[(short)(messageOffset + i)];
      expected = new byte[keyLength];
      for (short i = 0; i < keyLength; i++)
        expected[i] = parameters[(short)(signatureOffset + i)];
    }
    Signature signature = Signature.getInstance(algorithm, false);
    if (parameterized) {
      if (!pss) ISOException.throwIt(ISO7816.SW_FUNC_NOT_SUPPORTED);
      byte[] pssParameters = {(byte)0, (byte)0};
      signature.init(key, signing ? Signature.MODE_SIGN : Signature.MODE_VERIFY,
          pssParameters, (short)0, (short)2);
    } else {
      signature.init(key, signing ? Signature.MODE_SIGN : Signature.MODE_VERIFY);
    }
    if (signing) {
      if (!pss) {
        if (precomputed)
          return signature.signPreComputedHash(message, (short)0,
              (short)message.length, output, (short)0);
        return signature.sign(message, (short)0, (short)message.length,
            output, (short)0);
      }
      short signatureLength;
      if (precomputed) {
        signatureLength = signature.signPreComputedHash(message, (short)0,
            (short)message.length, output, (short)0);
      } else {
        signatureLength = signature.sign(message, (short)0,
            (short)message.length, output, (short)0);
      }
      RSAPublicKey publicKey = (RSAPublicKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_RSA_PUBLIC, keyTypeLength, false);
      publicKey.setModulus(parameters, (short)0, keyLength);
      byte[] publicExponent = {(byte)1, (byte)0, (byte)1};
      publicKey.setExponent(publicExponent, (short)0, (short)3);
      if (parameterized) {
        byte[] pssParameters = {(byte)0, (byte)0};
        signature.init(publicKey, Signature.MODE_VERIFY, pssParameters,
            (short)0, (short)2);
      } else {
        signature.init(publicKey, Signature.MODE_VERIFY);
      }
      boolean valid = precomputed
          ? signature.verifyPreComputedHash(message, (short)0,
              (short)message.length, output, (short)0, signatureLength)
          : signature.verify(message, (short)0, (short)message.length,
              output, (short)0, signatureLength);
      output[0] = valid ? (byte)1 : (byte)0;
      return (short)1;
    }
    boolean valid = precomputed
        ? signature.verifyPreComputedHash(message, (short)0,
            (short)message.length, expected, (short)0, (short)expected.length)
        : signature.verify(message, (short)0, (short)message.length,
            expected, (short)0, (short)expected.length);
    output[0] = valid ? (byte)1 : (byte)0;
    return (short)1;
  }

  private static short runRsaMessageRecovery(byte operation, byte[] parameters,
                                             byte[] output) {
    final short keyLength = 64;
    final short messageOffset = 68;
    if (operation == 0) {
      if (parameters.length < 2 * keyLength)
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      RSAPrivateKey privateKey = (RSAPrivateKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_RSA_PRIVATE, KeyBuilder.LENGTH_RSA_512, false);
      privateKey.setModulus(parameters, (short)0, keyLength);
      privateKey.setExponent(parameters, keyLength, keyLength);
      SignatureMessageRecovery signature = (SignatureMessageRecovery)
          Signature.getInstance(Signature.ALG_RSA_SHA_ISO9796_MR, false);
      signature.init(privateKey, Signature.MODE_SIGN);
      short messageLength = (short)(parameters.length - 2 * keyLength);
      short[] recoveredLength = new short[1];
      short resultLength = signature.sign(parameters, (short)(2 * keyLength),
          messageLength, output, (short)0, recoveredLength, (short)0);
      output[resultLength] = (byte)(recoveredLength[0] >> 8);
      output[(short)(resultLength + 1)] = (byte)recoveredLength[0];
      return (short)(resultLength + 2);
    }
    if (operation != 1 && operation != 2)
      ISOException.throwIt(ISO7816.SW_FUNC_NOT_SUPPORTED);
    if (parameters.length < messageOffset + keyLength)
      ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    short exponentLength = (short)(parameters[keyLength] & 0xff);
    if (exponentLength == 0 || parameters.length < keyLength + 1 + exponentLength + keyLength)
      ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    short messageOffsetActual = (short)(keyLength + 1 + exponentLength);
    short signatureOffset = (short)(parameters.length - keyLength);
    short messageLength = (short)(signatureOffset - messageOffsetActual);
    RSAPublicKey publicKey = (RSAPublicKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_RSA_PUBLIC, KeyBuilder.LENGTH_RSA_512, false);
    publicKey.setModulus(parameters, (short)0, keyLength);
    publicKey.setExponent(parameters, (short)(keyLength + 1), exponentLength);
    byte[] signatureData = new byte[keyLength];
    for (short i = 0; i < keyLength; i++)
      signatureData[i] = parameters[(short)(signatureOffset + i)];
    SignatureMessageRecovery signature = (SignatureMessageRecovery)
        Signature.getInstance(Signature.ALG_RSA_SHA_ISO9796_MR, false);
    signature.init(publicKey, Signature.MODE_VERIFY);
    short recoveredLength = signature.beginVerify(signatureData, (short)0,
        keyLength);
    if (recoveredLength < 0 || recoveredLength > messageLength)
      ISOException.throwIt(ISO7816.SW_UNKNOWN);
    for (short i = 0; i < recoveredLength; i++)
      if (signatureData[i] != parameters[(short)(messageOffsetActual + i)])
        ISOException.throwIt(ISO7816.SW_UNKNOWN);
    short suffixOffset = (short)(messageOffsetActual + recoveredLength);
    short suffixLength = (short)(messageLength - recoveredLength);
    if (operation == 2 && suffixLength > 0)
      parameters[suffixOffset] ^= 1;
    signature.update(parameters, suffixOffset, suffixLength);
    output[0] = signature.verify(new byte[0], (short)0, (short)0)
        ? (byte)1 : (byte)0;
    output[1] = (byte)(recoveredLength >> 8);
    output[2] = (byte)recoveredLength;
    return (short)3;
  }

  private static short runCipher(byte algorithm, byte operation, byte[] input,
                                 short inputLength, byte[] output) {
    boolean aes = algorithm == (byte)0xF0 || algorithm == 13 || algorithm == 14 || algorithm >= 22;
    short keyLength = (short)(aes ? KeyBuilder.LENGTH_AES_128 : KeyBuilder.LENGTH_DES);
    Key key = KeyBuilder.buildKey(aes ? KeyBuilder.TYPE_AES : KeyBuilder.TYPE_DES,
                                  keyLength, false);
    if (aes) {
      byte[] keyBytes = new byte[16];
      if (algorithm == (byte)0xF0) {
        byte[] ctrKey = {(byte)0x2b, (byte)0x7e, (byte)0x15, (byte)0x16,
            (byte)0x28, (byte)0xae, (byte)0xd2, (byte)0xa6, (byte)0xab,
            (byte)0xf7, (byte)0x15, (byte)0x88, (byte)0x09, (byte)0xcf,
            (byte)0x4f, (byte)0x3c};
        for (short i = 0; i < keyBytes.length; i++) keyBytes[i] = ctrKey[i];
      } else {
        for (short i = 0; i < keyBytes.length; i++) keyBytes[i] = (byte)(i + 1);
      }
      ((AESKey)key).setKey(keyBytes, (short)0);
    } else {
      byte[] keyBytes = {
          (byte)0x13, (byte)0x34, (byte)0x57, (byte)0x79,
          (byte)0x9b, (byte)0xbc, (byte)0xdf, (byte)0xf1};
      ((DESKey)key).setKey(keyBytes, (short)0);
    }
    byte operationMode = operation >= 4 ? (byte)(operation - 4) : operation;
    Cipher cipher = algorithm == (byte)0xF0 && operation >= 4
        ? Cipher.getInstance((byte)8, Cipher.PAD_NOPAD, false)
        : Cipher.getInstance(algorithm, false);
    byte mode = (operationMode == 1 || operationMode == 3)
        ? Cipher.MODE_DECRYPT : Cipher.MODE_ENCRYPT;
    if (algorithm == (byte)0xF0) {
      if (inputLength < 16) ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      byte[] counter = new byte[16];
      for (short i = 0; i < 16; i++) counter[i] = input[i];
      cipher.init(key, mode, counter, (short)0, (short)counter.length);
      short payloadLength = (short)(inputLength - 16);
      if (operationMode >= 2) {
        short firstLength = payloadLength > 19 ? (short)19 : (short)(payloadLength / 2);
        short written = cipher.update(input, (short)16, firstLength, output, (short)0);
        return (short)(written + cipher.doFinal(input, (short)(16 + firstLength),
            (short)(payloadLength - firstLength), output, written));
      }
      return cipher.doFinal(input, (short)16, payloadLength, output, (short)0);
    }
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
    return algorithm == 1 || algorithm == 5 || algorithm == 13 || algorithm == 14 ||
        algorithm == (byte)0xF0;
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
    signature.init(key, operation == 1 ? Signature.MODE_VERIFY : Signature.MODE_SIGN);
    if (operation == 3) {
      short bufferedLength = (short)(inputLength / 2);
      signature.update(input, (short)0, bufferedLength);
      return signature.sign(input, bufferedLength,
          (short)(inputLength - bufferedLength), output, (short)0);
    }
    if (operation == 0 || operation == 2) {
      short signatureLength;
      try {
        signatureLength = signature.sign(input, (short)0, inputLength,
            output, (short)0);
      } catch (CryptoException exception) {
        output[0] = (byte)(0x50 | exception.getReason());
        return (short)1;
      }
      if (operation == 2) {
        for (short repeat = 1; repeat < 50; repeat++) {
          short repeatedLength;
          try {
            repeatedLength = signature.sign(input, (short)0, inputLength,
                output, (short)0);
          } catch (CryptoException exception) {
            output[0] = (byte)(0x60 | exception.getReason());
            return (short)1;
          }
          if (repeatedLength != signatureLength)
            ISOException.throwIt(ISO7816.SW_UNKNOWN);
        }
      }
      return signatureLength;
    }
    short signatureLength = signature.getLength();
    short messageLength = (short)(inputLength - signatureLength);
    if (messageLength < 0) ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
    output[0] = signature.verify(input, (short)0, messageLength, input,
        messageLength, signatureLength) ? (byte)1 : (byte)0;
    return 1;
  }
}
