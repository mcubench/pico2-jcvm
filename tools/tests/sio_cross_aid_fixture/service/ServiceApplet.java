package sio.service;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.framework.Util;
import javacard.security.MessageDigest;
import javacard.security.AESKey;
import javacard.security.KeyBuilder;
import javacard.security.Signature;
import javacardx.crypto.Cipher;
import javacardx.security.SensitiveResult;
import sio.contract.Probe;

public final class ServiceApplet extends Applet implements Probe {
  private static final byte[] CLIENT_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x04, 0x01
  };
  private static final byte[] SERVICE_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x03, 0x01
  };
  private static Object retainedForeignStatic;
  private Object retainedForeignInstance;

  private ServiceApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ServiceApplet();
  }

  public Shareable getShareableInterfaceObject(AID clientAID, byte parameter) {
    if (parameter != 0x5A || clientAID == null ||
        !clientAID.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length)) {
      return null;
    }
    return this;
  }

  public short exercise(byte marker, short offset, short wide, AID callerAID,
                        byte[] nullable) {
    AID caller = JCSystem.getPreviousContextAID();
    AID self = JCSystem.getAID();
    if (marker != (byte) 0xA5 || offset != (short) 0x1234 ||
        wide != (short) 0x2345 || callerAID == null || nullable != null ||
        caller == null || self == null ||
        !callerAID.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length) ||
        !caller.equals(CLIENT_AID, (short) 0, (byte) CLIENT_AID.length) ||
        !self.equals(SERVICE_AID, (short) 0, (byte) SERVICE_AID.length)) {
      return (short) 0x6F01;
    }
    return (short) 0x4567;
  }

  public int exerciseWide(int value) {
    return value == 0x12345678 ? 0x76543210 : 0x6F010001;
  }

  public short sensitiveCopy() {
    byte[] source = { 0x12, 0x34 };
    byte[] destination = { 0, 0 };
    return Util.arrayCopyNonAtomic(source, (short)0, destination, (short)0,
                                   (short)2);
  }

  public short sensitiveFailedCopy() {
    byte[] source = { 0x12, 0x34 };
    byte[] destination = { 0, 0 };
    try {
      Util.arrayCopyNonAtomic(source, (short)1, destination, (short)0,
                              (short)2);
      return (short)0x6A2C;
    } catch (ArrayIndexOutOfBoundsException expected) {
      return (short)0x0123;
    }
  }

  public short sensitiveAIDResults() {
    AID self = JCSystem.getAID();
    SensitiveResult.assertEquals(self);
    AID previous = JCSystem.getPreviousContextAID();
    SensitiveResult.assertEquals(previous);
    return self.equals(SERVICE_AID, (short)0, (byte)SERVICE_AID.length) &&
                   previous != null &&
                   previous.equals(CLIENT_AID, (short)0,
                                   (byte)CLIENT_AID.length)
        ? (short)0x6A31 : (short)0x6A32;
  }

  public short noSensitiveApi() {
    return (short)0x6A33;
  }

  public short sensitiveDigestResult() {
    MessageDigest digest = MessageDigest.getInstance(
        MessageDigest.ALG_SHA_256, false);
    byte[] input = { 0x12, 0x34 };
    byte[] output = new byte[32];
    byte[] expected = {
      0x3a, 0x10, 0x3a, 0x4e, 0x57, 0x29, (byte)0xad, 0x68,
      (byte)0xc0, 0x2a, 0x67, (byte)0x8a, (byte)0xe3, (byte)0x9a,
      (byte)0xcc, (byte)0xfb, (byte)0xc0, (byte)0xae, 0x20, (byte)0x80,
      (byte)0x96, 0x43, 0x74, 0x01, (byte)0xb7, (byte)0xce, (byte)0xab,
      0x63, (byte)0xcc, (byte)0xa0, 0x62, 0x2f
    };
    short length = digest.doFinal(input, (short)0, (short)input.length,
                                  output, (short)0);
    if (length != expected.length) return (short)0;
    for (short index = 0; index < length; index++) {
      if (output[index] != expected[index]) return (short)0;
    }
    return length;
  }

  public short sensitiveDigestFailure() {
    MessageDigest digest = MessageDigest.getInstance(
        MessageDigest.ALG_SHA_256, false);
    byte[] input = { 0x12, 0x34 };
    byte[] output = new byte[32];
    try {
      digest.doFinal(input, (short)1, (short)2, output, (short)0);
      return (short)0x6A3C;
    } catch (ArrayIndexOutOfBoundsException expected) {
      return (short)0x6A3D;
    }
  }

  public short sensitiveCipherResult() {
    try {
      AESKey key = (AESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
      byte[] keyData = new byte[16];
      key.setKey(keyData, (short)0);
      Cipher cipher = Cipher.getInstance(
          Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
      cipher.init(key, Cipher.MODE_ENCRYPT);
      byte[] input = new byte[16];
      byte[] output = new byte[32];
      short updated = cipher.update(input, (short)0, (short)16,
                                    output, (short)0);
      if (updated != 16) return (short)0x6A41;
      try {
        SensitiveResult.assertEquals(updated);
      } catch (SecurityException missingUpdateResult) {
        return (short)0x6A45;
      }
      return cipher.doFinal(input, (short)0, (short)16, output, (short)16);
    } catch (SecurityException runtimeSecurityFailure) {
      return (short)0x6A46;
    }
  }

  public short sensitiveSignatureResult() {
    try {
      AESKey key = (AESKey)KeyBuilder.buildKey(
          KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
      byte[] keyData = new byte[16];
      key.setKey(keyData, (short)0);
      byte[] input = new byte[16];
      byte[] mac = new byte[16];
      Signature signer = Signature.getInstance(
          Signature.ALG_AES_MAC_128_NOPAD, false);
      signer.init(key, Signature.MODE_SIGN);
      short macLength = signer.sign(input, (short)0, (short)input.length,
                                    mac, (short)0);
      try {
        SensitiveResult.assertEquals(macLength);
      } catch (SecurityException missingSignResult) {
        return (short)0x6A53;
      }
      Signature verifier = Signature.getInstance(
          Signature.ALG_AES_MAC_128_NOPAD, false);
      verifier.init(key, Signature.MODE_VERIFY);
      boolean valid = verifier.verify(input, (short)0, (short)input.length,
                                      mac, (short)0, macLength);
      return valid ? (short)1 : (short)0;
    } catch (SecurityException signatureSecurityFailure) {
      return (short)0x6A54;
    }
  }

  public short sensitiveSignatureMismatchResult() {
    AESKey key = (AESKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_AES, KeyBuilder.LENGTH_AES_128, false);
    byte[] keyData = new byte[16];
    key.setKey(keyData, (short)0);
    byte[] input = new byte[16];
    byte[] mac = new byte[16];
    Signature signer = Signature.getInstance(
        Signature.ALG_AES_MAC_128_NOPAD, false);
    signer.init(key, Signature.MODE_SIGN);
    short macLength = signer.sign(input, (short)0, (short)input.length,
                                  mac, (short)0);
    mac[0] = (byte)(mac[0] ^ 1);
    Signature verifier = Signature.getInstance(
        Signature.ALG_AES_MAC_128_NOPAD, false);
    verifier.init(key, Signature.MODE_VERIFY);
    boolean valid = verifier.verify(input, (short)0, (short)input.length,
                                    mac, (short)0, macLength);
    return valid ? (short)0 : (short)1;
  }

  public short throwSecurityException() {
    throw new SecurityException();
  }

  public short inspectPrivateArray(byte[] value) {
    byte[] forwarded = value;
    try {
      return (short) forwarded.length;
    } catch (SecurityException expected) {
      return (short) 0x5A31;
    }
  }

  public short inspectPrivateArrayNative(byte[] value) {
    byte[] destination = new byte[1];
    try {
      Util.arrayCopyNonAtomic(value, (short) 0, destination, (short) 0,
                              (short) 1);
      return (short) 0x6B36;
    } catch (SecurityException expected) {
      return (short) 0x6B37;
    }
  }

  public short inspectGlobalReference(Object[] values) {
    try {
      Object value = values[0];
      if (value == null) {
        return (short)0x6B01;
      }
      // Client and service applet instances commonly occupy the same compact
      // slot in their separate package heaps. Owner provenance must keep
      // those distinct objects unequal across this SIO boundary.
      if (value == this) {
        return (short)0x6B30;
      }
      value.equals(this);
      return (short)0x6B02;
    } catch (SecurityException expected) {
      return (short)0x6B31;
    }
  }

  public short retainForeignReference(Object[] values) {
    Object foreign = values[0];
    boolean staticStored = false;
    boolean instanceStored = false;
    boolean arrayStored = false;
    try {
      retainedForeignStatic = foreign;
      staticStored = retainedForeignStatic == foreign;
    } catch (SecurityException denied) {
      // A normal applet object is not a temporary JCRE object.
    }
    try {
      retainedForeignInstance = foreign;
      instanceStored = retainedForeignInstance == foreign;
    } catch (SecurityException denied) {
      // The receiver is owned here; only the stored value is foreign.
    }
    try {
      Object[] local = JCSystem.makeTransientObjectArray(
          (short)1, JCSystem.CLEAR_ON_RESET);
      local[0] = foreign;
      arrayStored = local[0] == foreign;
    } catch (SecurityException denied) {
      // Applet-owned arrays may retain non-temporary foreign object refs.
    }
    short failures = 0;
    if (!staticStored) failures |= 1;
    if (!instanceStored) failures |= 2;
    if (!arrayStored) failures |= 4;
    return (short)(0x6B40 | failures);
  }

  public byte[] echoArray(byte[] value) {
    return value;
  }

  public boolean sameReference(byte[] reference) {
    if (reference == null) {
      return true;
    }
    return reference == reference;
  }

  public boolean sameReferences(byte[] left, byte[] right) {
    return left == right;
  }

  public short inspectForeignCast(Object value) {
    try {
      Probe ignored = (Probe) value;
      return (short)0x6B33;
    } catch (SecurityException expected) {
      return (short)0x6B32;
    }
  }

  public short inspectForeignInstanceof(Object value) {
    try {
      return value instanceof Probe ? (short)0x6B34 : (short)0x6B35;
    } catch (SecurityException expected) {
      return (short)0x6B32;
    }
  }

  public void process(APDU apdu) {}
}
