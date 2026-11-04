package sio.service;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.framework.Util;
import javacard.security.CryptoException;
import javacard.security.MessageDigest;
import javacard.security.AESKey;
import javacard.security.KeyBuilder;
import javacard.security.Signature;
import javacardx.crypto.Cipher;
import javacardx.security.SensitiveResult;
import fr.gouv.ssi.nativeimpl.NativeImplementation;
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

  private boolean sensitiveResultUnassigned() {
    try {
      SensitiveResult.assertEquals((short) 0x7777);
      return false;
    } catch (SecurityException expected) {
      return true;
    }
  }

  public short validateNonAtomicUtil() {
    byte[] source = { 1, 2 };
    byte[] destination = { 3, 4 };
    try {
      Util.arrayCopyNonAtomic(source, (short) 0, destination, (short) 0,
                              (short) -1);
      return (short) 0x6A71;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (destination[0] != 3 || destination[1] != 4 ||
          !sensitiveResultUnassigned()) return (short) 0x6A72;
    }
    try {
      Util.arrayCopyNonAtomic(source, (short) 1, destination, (short) 0,
                              (short) 2);
      return (short) 0x6A73;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (destination[0] != 3 || destination[1] != 4 ||
          !sensitiveResultUnassigned()) return (short) 0x6A74;
    }
    try {
      Util.arrayCopyNonAtomic(source, (short) -1, destination, (short) 0,
                              (short) 1);
      return (short) 0x6A81;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (destination[0] != 3 || destination[1] != 4 ||
          !sensitiveResultUnassigned()) return (short) 0x6A82;
    }
    try {
      Util.arrayCopyNonAtomic(source, (short) 0, destination, (short) 1,
                              (short) 2);
      return (short) 0x6A75;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (destination[0] != 3 || destination[1] != 4 ||
          !sensitiveResultUnassigned()) return (short) 0x6A76;
    }
    byte[] invalidOverlap = { 7, 8, 9, 10 };
    try {
      Util.arrayCopyNonAtomic(invalidOverlap, (short) 0, invalidOverlap,
                              (short) 3, (short) 2);
      return (short) 0x6A83;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (invalidOverlap[0] != 7 || invalidOverlap[1] != 8 ||
          invalidOverlap[2] != 9 || invalidOverlap[3] != 10 ||
          !sensitiveResultUnassigned()) return (short) 0x6A84;
    }
    try {
      Util.arrayCopyNonAtomic(null, (short) 0, destination, (short) 0,
                              (short) 0);
      return (short) 0x6A77;
    } catch (NullPointerException expected) {
      if (!sensitiveResultUnassigned()) return (short) 0x6A78;
    }
    try {
      Util.arrayCopyNonAtomic(source, (short) 0, null, (short) 0,
                              (short) 0);
      return (short) 0x6A87;
    } catch (NullPointerException expected) {
      if (!sensitiveResultUnassigned()) return (short) 0x6A88;
    }
    if (Util.arrayCopyNonAtomic(source, (short) 2, destination, (short) 2,
                                (short) 0) != 2) return (short) 0x6A79;
    SensitiveResult.assertEquals((short) 2);

    byte[] overlap = { 1, 2, 3, 4 };
    Util.arrayCopyNonAtomic(overlap, (short) 0, overlap, (short) 1,
                            (short) 3);
    if (overlap[0] != 1 || overlap[1] != 1 || overlap[2] != 2 ||
        overlap[3] != 3) return (short) 0x6A7A;

    byte[] fill = { 5, 6 };
    try {
      Util.arrayFillNonAtomic(fill, (short) 0, (short) -1, (byte) 9);
      return (short) 0x6A7B;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (fill[0] != 5 || fill[1] != 6 ||
          !sensitiveResultUnassigned()) return (short) 0x6A7C;
    }
    try {
      Util.arrayFillNonAtomic(fill, (short) -1, (short) 1, (byte) 9);
      return (short) 0x6A85;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (fill[0] != 5 || fill[1] != 6 ||
          !sensitiveResultUnassigned()) return (short) 0x6A86;
    }
    try {
      Util.arrayFillNonAtomic(fill, (short) 1, (short) 2, (byte) 9);
      return (short) 0x6A89;
    } catch (ArrayIndexOutOfBoundsException expected) {
      if (fill[0] != 5 || fill[1] != 6 ||
          !sensitiveResultUnassigned()) return (short) 0x6A8A;
    }
    try {
      Util.arrayFillNonAtomic(null, (short) 0, (short) 0, (byte) 9);
      return (short) 0x6A7D;
    } catch (NullPointerException expected) {
      if (!sensitiveResultUnassigned()) return (short) 0x6A7E;
    }
    if (Util.arrayFillNonAtomic(fill, (short) 2, (short) 0, (byte) 9) != 2)
      return (short) 0x6A7F;
    SensitiveResult.assertEquals((short) 2);
    return (short) 0x6A70;
  }

  public short sensitiveCopy() {
    byte[] source = { 0x12, 0x34 };
    byte[] destination = { 0, 0 };
    return Util.arrayCopyNonAtomic(source, (short)0, destination, (short)0,
                                   (short)2);
  }

  public short sensitiveObjectResult() {
    NativeImplementation.setSensitiveResultObject(this);
    return (short) 0x6A57;
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

  public short throwCryptoException() {
    CryptoException.throwIt((short) 0x1234);
    return (short) 0;
  }

  public short throwArrayIndexOutOfBoundsException() {
    throw new ArrayIndexOutOfBoundsException();
  }

  public short inspectInheritedInterfaces() {
    Object inherited = new InheritedMarker();
    if (!(inherited instanceof LocalMarker)) return (short) 0x6A68;
    if (!(inherited instanceof sio.contract.RootMarker)) {
      return (short) 0x6A69;
    }
    LocalMarker local = (LocalMarker) inherited;
    sio.contract.RootMarker imported = (sio.contract.RootMarker) inherited;
    return local != null && imported != null ? (short) 0x6A6A : (short) 0x6A6B;
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
      // Applet-owned arrays cannot route foreign owner roots through GC.
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
