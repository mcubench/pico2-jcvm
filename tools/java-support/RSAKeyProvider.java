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
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > modulus.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < modulus.length; i++) modulus[i] = 0;
    short start = (short)(modulus.length - length);
    for (short i = 0; i < length; i++)
      modulus[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[0] = length;
  }

  public short getModulus(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[0] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[0])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(modulus.length - lengths[0]);
    for (short i = 0; i < lengths[0]; i++)
      buffer[(short)(offset + i)] = modulus[(short)(start + i)];
    return lengths[0];
  }
  public void setExponent(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > exponent.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < exponent.length; i++) exponent[i] = 0;
    short start = (short)(exponent.length - length);
    for (short i = 0; i < length; i++)
      exponent[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[1] = length;
  }

  public short getExponent(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[1] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[1])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(exponent.length - lengths[1]);
    for (short i = 0; i < lengths[1]; i++)
      buffer[(short)(offset + i)] = exponent[(short)(start + i)];
    return lengths[1];
  }
  public void setP(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > p.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < p.length; i++) p[i] = 0;
    short start = (short)(p.length - length);
    for (short i = 0; i < length; i++)
      p[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[2] = length;
  }

  public short getP(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[2] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[2])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(p.length - lengths[2]);
    for (short i = 0; i < lengths[2]; i++)
      buffer[(short)(offset + i)] = p[(short)(start + i)];
    return lengths[2];
  }
  public void setQ(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > q.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < q.length; i++) q[i] = 0;
    short start = (short)(q.length - length);
    for (short i = 0; i < length; i++)
      q[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[3] = length;
  }

  public short getQ(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[3] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[3])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(q.length - lengths[3]);
    for (short i = 0; i < lengths[3]; i++)
      buffer[(short)(offset + i)] = q[(short)(start + i)];
    return lengths[3];
  }
  public void setDP1(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > dp.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < dp.length; i++) dp[i] = 0;
    short start = (short)(dp.length - length);
    for (short i = 0; i < length; i++)
      dp[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[4] = length;
  }

  public short getDP1(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[4] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[4])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(dp.length - lengths[4]);
    for (short i = 0; i < lengths[4]; i++)
      buffer[(short)(offset + i)] = dp[(short)(start + i)];
    return lengths[4];
  }
  public void setDQ1(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > dq.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < dq.length; i++) dq[i] = 0;
    short start = (short)(dq.length - length);
    for (short i = 0; i < length; i++)
      dq[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[5] = length;
  }

  public short getDQ1(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[5] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[5])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(dq.length - lengths[5]);
    for (short i = 0; i < lengths[5]; i++)
      buffer[(short)(offset + i)] = dq[(short)(start + i)];
    return lengths[5];
  }
  public void setPQ(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length <= 0 || offset > buffer.length - length ||
        length > pq.length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    for (short i = 0; i < pq.length; i++) pq[i] = 0;
    short start = (short)(pq.length - length);
    for (short i = 0; i < length; i++)
      pq[(short)(start + i)] = buffer[(short)(offset + i)];
    lengths[6] = length;
  }

  public short getPQ(byte[] buffer, short offset) {
    if (buffer == null) throw new NullPointerException();
    if (lengths[6] == 0) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (offset < 0 || offset > buffer.length - lengths[6])
      throw new ArrayIndexOutOfBoundsException();
    short start = (short)(pq.length - lengths[6]);
    for (short i = 0; i < lengths[6]; i++)
      buffer[(short)(offset + i)] = pq[(short)(start + i)];
    return lengths[6];
  }

}
