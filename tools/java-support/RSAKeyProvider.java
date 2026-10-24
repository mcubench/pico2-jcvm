package fr.gouv.ssi.nativeimpl;

import javacard.security.KeyBuilder;
import javacard.security.RSAPrivateCrtKey;
import javacard.security.RSAPrivateKey;
import javacard.security.RSAPublicKey;
import javacard.framework.JCSystem;

/** Storage layout for Java Card RSA public, private and CRT key objects. */
public final class RSAKeyProvider implements RSAPublicKey, RSAPrivateKey, RSAPrivateCrtKey {
  private byte type;
  private short size;
  private byte[] modulus;
  private byte[] exponent;
  private byte[] p;
  private byte[] q;
  private byte[] dp;
  private byte[] dq;
  private byte[] pq;
  private short[] lengths;

  public RSAKeyProvider(byte type, short size) {
    this.type = type;
    this.size = size;
    short bytes = (short)((size + 7) / 8);
    short halfBytes = (short)((bytes + 1) / 2);
    boolean transientKey = type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_DESELECT ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT;
    byte event = type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_DESELECT ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT
        ? JCSystem.CLEAR_ON_DESELECT : JCSystem.CLEAR_ON_RESET;
    this.modulus = keyComponent(bytes, transientKey, event);
    this.exponent = keyComponent(bytes, transientKey, event);
    this.p = keyComponent(halfBytes, transientKey, event);
    this.q = keyComponent(halfBytes, transientKey, event);
    this.dp = keyComponent(halfBytes, transientKey, event);
    this.dq = keyComponent(halfBytes, transientKey, event);
    this.pq = keyComponent(halfBytes, transientKey, event);
    this.lengths = transientKey
        ? JCSystem.makeTransientShortArray((short)7, event)
        : new short[(short)7];
  }

  private static byte[] keyComponent(short length, boolean transientKey,
                                     byte event) {
    return transientKey ? JCSystem.makeTransientByteArray(length, event)
        : new byte[length];
  }

  public native boolean isInitialized();
  public native void clearKey();
  public native byte getType();
  public native short getSize();

  public native void setModulus(byte[] buffer, short offset, short length);
  public native short getModulus(byte[] buffer, short offset);
  public native void setExponent(byte[] buffer, short offset, short length);
  public native short getExponent(byte[] buffer, short offset);
  public native void setP(byte[] buffer, short offset, short length);
  public native short getP(byte[] buffer, short offset);
  public native void setQ(byte[] buffer, short offset, short length);
  public native short getQ(byte[] buffer, short offset);
  public native void setDP1(byte[] buffer, short offset, short length);
  public native short getDP1(byte[] buffer, short offset);
  public native void setDQ1(byte[] buffer, short offset, short length);
  public native short getDQ1(byte[] buffer, short offset);
  public native void setPQ(byte[] buffer, short offset, short length);
  public native short getPQ(byte[] buffer, short offset);
}
