package javacard.security;

import javacard.framework.JCSystem;

/** Key storage for Java Card RSA public, private and CRT key interfaces. */
final class RSAKeyProvider implements RSAPublicKey, RSAPrivateKey, RSAPrivateCrtKey {
  private final byte type;
  private final short size;
  private final byte[] modulus;
  private final byte[] exponent;
  private final byte[] p;
  private final byte[] q;
  private final byte[] dp;
  private final byte[] dq;
  private final byte[] pq;
  private final short[] lengths;

  RSAKeyProvider(byte type, short size) {
    this.type = type;
    this.size = size;
    short bytes = (short)((size + 7) / 8);
    short half = (short)((bytes + 1) / 2);
    byte event = (type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_DESELECT ||
                  type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT)
        ? JCSystem.CLEAR_ON_DESELECT : JCSystem.CLEAR_ON_RESET;
    boolean transientKey = type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_RSA_PRIVATE_TRANSIENT_DESELECT ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT;
    modulus = allocate(bytes, transientKey, event);
    exponent = allocate(bytes, transientKey, event);
    p = allocate(half, transientKey, event);
    q = allocate(half, transientKey, event);
    dp = allocate(half, transientKey, event);
    dq = allocate(half, transientKey, event);
    pq = allocate(half, transientKey, event);
    lengths = transientKey
        ? JCSystem.makeTransientShortArray((short)7, event) : new short[7];
  }

  private static byte[] allocate(short length, boolean transientKey, byte event) {
    return transientKey ? JCSystem.makeTransientByteArray(length, event)
                        : new byte[length];
  }

  public boolean isInitialized() {
    if (type == KeyBuilder.TYPE_RSA_PUBLIC)
      return lengths[0] != 0 && lengths[1] != 0;
    if (type == KeyBuilder.TYPE_RSA_CRT_PRIVATE ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT)
      return lengths[2] != 0 && lengths[3] != 0 && lengths[4] != 0 &&
          lengths[5] != 0 && lengths[6] != 0;
    return lengths[0] != 0 && lengths[1] != 0;
  }

  public void clearKey() {
    clear(modulus); clear(exponent); clear(p); clear(q); clear(dp); clear(dq); clear(pq);
    for (short i = 0; i < lengths.length; i++) lengths[i] = 0;
  }

  private static void clear(byte[] value) {
    for (short i = 0; i < value.length; i++) value[i] = 0;
  }

  public byte getType() { return type; }
  public short getSize() { return size; }

  public void setModulus(byte[] buffer, short offset, short length) {
    set(modulus, (short)0, buffer, offset, length);
  }
  public void setExponent(byte[] buffer, short offset, short length) {
    set(exponent, (short)1, buffer, offset, length);
  }
  public short getModulus(byte[] buffer, short offset) {
    return get(modulus, (short)0, buffer, offset);
  }
  public short getExponent(byte[] buffer, short offset) {
    return get(exponent, (short)1, buffer, offset);
  }
  public void setP(byte[] buffer, short offset, short length) {
    set(p, (short)2, buffer, offset, length);
  }
  public void setQ(byte[] buffer, short offset, short length) {
    set(q, (short)3, buffer, offset, length);
  }
  public void setDP1(byte[] buffer, short offset, short length) {
    set(dp, (short)4, buffer, offset, length);
  }
  public void setDQ1(byte[] buffer, short offset, short length) {
    set(dq, (short)5, buffer, offset, length);
  }
  public void setPQ(byte[] buffer, short offset, short length) {
    set(pq, (short)6, buffer, offset, length);
  }
  public short getP(byte[] buffer, short offset) { return get(p, (short)2, buffer, offset); }
  public short getQ(byte[] buffer, short offset) { return get(q, (short)3, buffer, offset); }
  public short getDP1(byte[] buffer, short offset) { return get(dp, (short)4, buffer, offset); }
  public short getDQ1(byte[] buffer, short offset) { return get(dq, (short)5, buffer, offset); }
  public short getPQ(byte[] buffer, short offset) { return get(pq, (short)6, buffer, offset); }

  private void set(byte[] destination, short component, byte[] source,
                   short offset, short length) {
    if (source == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > source.length - length ||
        length > destination.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    clear(destination);
    short start = (short)(destination.length - length);
    for (short i = 0; i < length; i++) destination[(short)(start + i)] = source[(short)(offset + i)];
    lengths[component] = length;
  }

  private short get(byte[] source, short component, byte[] destination, short offset) {
    if (destination == null) throw new NullPointerException();
    if (lengths[component] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > destination.length - lengths[component])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(source.length - lengths[component]);
    for (short i = 0; i < lengths[component]; i++)
      destination[(short)(offset + i)] = source[(short)(start + i)];
    return lengths[component];
  }
}
