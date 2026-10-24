package javacard.security;

import fr.gouv.ssi.nativeimpl.MacNativeBridge;
import javacard.framework.Util;

/** Buffered implementation of DES MAC, AES CBC-MAC, and AES-CMAC. */
final class SymmetricMacProvider extends Signature {
  private final byte algorithm;
  private final byte cipher;
  private final byte padding;
  private final short resultLength;
  private final boolean aes;
  private final byte[] iv = new byte[16];
  private byte[] keyData;
  private byte[] message = new byte[64];
  private short messageLength;
  private boolean initialized;
  private byte mode;

  SymmetricMacProvider(byte algorithm, byte cipher, byte padding,
                       short resultLength, boolean aes) {
    this.algorithm = algorithm; this.cipher = cipher; this.padding = padding;
    this.resultLength = resultLength; this.aes = aes;
  }

  public void init(Key key, byte requestedMode) throws CryptoException {
    if (key == null) throw new NullPointerException();
    if (requestedMode != MODE_SIGN && requestedMode != MODE_VERIFY)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    if (!key.isInitialized()) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    byte type = key.getType();
    if (aes ? !(key instanceof AESKey) : !(key instanceof DESKey))
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    short keyLength = (short)(key.getSize() / 8);
    if (aes ? keyLength != 16 : (keyLength != 8 && keyLength != 16 && keyLength != 24))
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    if ((algorithm == ALG_DES_MAC4_ISO9797_1_M2_ALG3 ||
         algorithm == ALG_DES_MAC8_ISO9797_1_M2_ALG3 ||
         algorithm == ALG_DES_MAC4_ISO9797_1_M1_ALG3 ||
         algorithm == ALG_DES_MAC8_ISO9797_1_M1_ALG3) && keyLength != 16)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    keyData = new byte[keyLength];
    if (aes) ((AESKey)key).getKey(keyData, (short)0);
    else ((DESKey)key).getKey(keyData, (short)0);
    for (short i = 0; i < iv.length; i++) iv[i] = 0;
    messageLength = 0; initialized = true; mode = requestedMode;
  }

  public void init(Key key, byte requestedMode, byte[] params, short offset,
                   short length) throws CryptoException {
    if (params == null) throw new NullPointerException();
    short block = (short)(aes ? 16 : 8);
    if (algorithm == ALG_AES_CMAC_128 || length != block || offset < 0 || offset > params.length - length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    init(key, requestedMode);
    for (short i = 0; i < block; i++) iv[i] = params[(short)(offset + i)];
  }

  public byte getAlgorithm() { return algorithm; }
  public void setInitialDigest(byte[] digest, short digestOffset, short digestLength,
      byte[] messageLengthBytes, short messageLengthOffset, short messageLengthLength)
      throws CryptoException {
    CryptoException.throwIt(CryptoException.ILLEGAL_USE);
  }
  public byte getMessageDigestAlgorithm() { return MessageDigest.ALG_NULL; }
  public byte getCipherAlgorithm() { return cipher; }
  public byte getPaddingAlgorithm() { return padding; }
  public short getLength() { return resultLength; }

  public void update(byte[] input, short offset, short length) {
    if (!initialized) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, offset, length); append(input, offset, length);
  }

  public short sign(byte[] input, short offset, short length, byte[] output,
                    short outputOffset) throws CryptoException {
    if (mode != MODE_SIGN) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, offset, length); check(output, outputOffset, resultLength);
    append(input, offset, length);
    byte[] full = new byte[16];
    MacNativeBridge.computeMac(algorithm, keyData, iv, message, messageLength, full);
    for (short i = 0; i < resultLength; i++) output[(short)(outputOffset + i)] = full[i];
    resetMessage(); return resultLength;
  }

  public short signPreComputedHash(byte[] hash, short offset, short length,
      byte[] output, short outputOffset) throws CryptoException {
    CryptoException.throwIt(CryptoException.ILLEGAL_USE); return 0;
  }

  public boolean verify(byte[] input, short offset, short length, byte[] signature,
                        short signatureOffset, short signatureLength) throws CryptoException {
    if (mode != MODE_VERIFY) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, offset, length); check(signature, signatureOffset, signatureLength);
    if (signatureLength != resultLength) CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    append(input, offset, length);
    byte[] full = new byte[16];
    MacNativeBridge.computeMac(algorithm, keyData, iv, message, messageLength, full);
    byte difference = 0;
    for (short i = 0; i < resultLength; i++) difference |= (byte)(full[i] ^ signature[(short)(signatureOffset + i)]);
    resetMessage(); return difference == 0;
  }

  public boolean verifyPreComputedHash(byte[] hash, short hashOffset, short hashLength,
      byte[] signature, short signatureOffset, short signatureLength) throws CryptoException {
    CryptoException.throwIt(CryptoException.ILLEGAL_USE); return false;
  }

  private void append(byte[] input, short offset, short length) {
    short next = (short)(messageLength + length);
    if (next < 0) throw new ArrayIndexOutOfBoundsException();
    if (next > message.length) {
      short size = (short)message.length;
      while (size < next) size = (short)(size * 2);
      byte[] expanded = new byte[size];
      Util.arrayCopyNonAtomic(message, (short)0, expanded, (short)0, messageLength);
      message = expanded;
    }
    Util.arrayCopyNonAtomic(input, offset, message, messageLength, length);
    messageLength = next;
  }
  private void resetMessage() {
    for (short i = 0; i < messageLength; i++) message[i] = 0;
    messageLength = 0;
  }
  private static void check(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || length < 0 || offset > buffer.length - length)
      throw new ArrayIndexOutOfBoundsException();
  }
}
