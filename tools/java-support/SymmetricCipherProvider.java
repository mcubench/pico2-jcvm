package javacardx.crypto;

import fr.gouv.ssi.nativeimpl.CbcCipherNativeBridge;
import javacard.security.AESKey;
import javacard.security.CryptoException;
import javacard.security.DESKey;
import javacard.security.Key;
import javacard.security.KeyBuilder;

/** Shared block cipher implementation for the selected AES/DES algorithms. */
final class SymmetricCipherProvider extends Cipher {
  private final byte algorithm;
  private final byte cipher;
  private final byte padding;
  private final boolean aes;
  private final boolean cbc;
  private final byte blockSize;
  private final byte[] iv;
  private final byte[] initialIv;
  private byte[] keyData;
  private byte[] buffered;
  private short bufferedLength;
  private byte mode;
  private boolean initialized;

  SymmetricCipherProvider(byte algorithm, byte cipher, byte padding,
                          boolean aes, boolean cbc) {
    this.algorithm = algorithm;
    this.cipher = cipher;
    this.padding = padding;
    this.aes = aes;
    this.cbc = cbc;
    this.blockSize = (byte) (aes ? 16 : 8);
    this.iv = new byte[this.blockSize];
    this.initialIv = new byte[this.blockSize];
    this.keyData = new byte[(short) (aes ? 32 : 24)];
    this.buffered = new byte[32];
  }

  public void init(Key key, byte requestedMode) throws CryptoException {
    if (key == null) throw new NullPointerException();
    if (requestedMode != MODE_ENCRYPT && requestedMode != MODE_DECRYPT)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    if (!key.isInitialized()) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    byte type = key.getType();
    if (aes ? !(key instanceof AESKey) : !(key instanceof DESKey))
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    if (aes ? (type != KeyBuilder.TYPE_AES && type != KeyBuilder.TYPE_AES_TRANSIENT_RESET &&
               type != KeyBuilder.TYPE_AES_TRANSIENT_DESELECT)
            : (type != KeyBuilder.TYPE_DES && type != KeyBuilder.TYPE_DES_TRANSIENT_RESET &&
               type != KeyBuilder.TYPE_DES_TRANSIENT_DESELECT))
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    short size = (short) (key.getSize() / 8);
    if (aes ? (size != 16 && size != 24 && size != 32)
            : (size != 8 && size != 16 && size != 24))
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    keyData = new byte[size];
    if (aes) ((AESKey) key).getKey(keyData, (short) 0);
    else ((DESKey) key).getKey(keyData, (short) 0);
    clearBuffer();
    for (short i = 0; i < iv.length; i++) { iv[i] = 0; initialIv[i] = 0; }
    mode = requestedMode;
    initialized = true;
  }

  public void init(Key key, byte requestedMode, byte[] parameters,
                   short offset, short length) throws CryptoException {
    if (!cbc || parameters == null || offset < 0 || length != blockSize ||
        offset > parameters.length - length)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    init(key, requestedMode);
    for (short i = 0; i < blockSize; i++) {
      iv[i] = parameters[(short)(offset + i)];
      initialIv[i] = iv[i];
    }
  }

  public byte getAlgorithm() { return algorithm; }
  public byte getCipherAlgorithm() { return cipher; }
  public byte getPaddingAlgorithm() { return padding; }

  public short update(byte[] input, short inputOffset, short inputLength,
                      byte[] output, short outputOffset) throws CryptoException {
    if (!initialized) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, inputOffset, inputLength);
    check(output, outputOffset, (short) 0);
    append(input, inputOffset, inputLength);
    short processLength = (short)(bufferedLength - (bufferedLength % blockSize));
    if (!encrypting() && padding != PAD_NOPAD && processLength != 0)
      processLength = (short)(processLength - blockSize);
    check(output, outputOffset, processLength);
    if (processLength > 0) transform(processLength, output, outputOffset);
    discard(processLength);
    return processLength;
  }

  public short doFinal(byte[] input, short inputOffset, short inputLength,
                       byte[] output, short outputOffset) throws CryptoException {
    if (!initialized) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, inputOffset, inputLength);
    check(output, outputOffset, (short) 0);
    append(input, inputOffset, inputLength);
    boolean encrypt = mode == MODE_ENCRYPT;
    short dataLength = bufferedLength;
    if (encrypt && padding != PAD_NOPAD) {
      short remainder = (short)(dataLength % blockSize);
      short pad = (short)(remainder == 0 && padding == PAD_ISO9797_M1
          ? 0 : blockSize - remainder);
      short paddedLength = (short) (dataLength + pad);
      ensureCapacity(paddedLength);
      if (padding == PAD_ISO9797_M2) buffered[dataLength] = (byte) 0x80;
      else if (padding == PAD_PKCS5) {
        for (short i = dataLength; i < paddedLength; i++) buffered[i] = (byte) pad;
      }
      dataLength = paddedLength;
    }
    if (dataLength == 0 || dataLength % blockSize != 0)
      CryptoException.throwIt(CryptoException.ILLEGAL_USE);
    // A padded decrypt can produce less than one full cipher block. Only
    // require the caller's buffer to hold the plaintext after unpadding.
    if (encrypt || padding == PAD_NOPAD) check(output, outputOffset, dataLength);
    byte[] result = new byte[dataLength];
    if (dataLength != 0) {
      CbcCipherNativeBridge.cipherCbc(algorithm, mode, keyData, iv, buffered,
          (short) 0, dataLength, result, (short) 0);
    }
    if (!encrypt && padding != PAD_NOPAD) dataLength = unpad(result, dataLength);
    check(output, outputOffset, dataLength);
    for (short i = 0; i < dataLength; i++) output[(short)(outputOffset + i)] = result[i];
    clearBuffer();
    for (short i = 0; i < iv.length; i++) iv[i] = initialIv[i];
    return dataLength;
  }

  private short unpad(byte[] data, short length) {
    if (padding == PAD_ISO9797_M1) {
      short i = (short)(length - 1);
      while (i >= 0 && data[i] == 0) i--;
      return (short)(i + 1);
    }
    if (padding == PAD_PKCS5) {
      short last = (short)(length - 1);
      short count = (short)(data[last] & 0xff);
      if (count < 1 || count > blockSize) CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      for (short i = (short)(length - count); i < length; i++)
        if ((data[i] & 0xff) != count) CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      return (short)(length - count);
    }
    if (padding == PAD_ISO9797_M2) {
      short i = (short)(length - 1);
      while (i >= 0 && data[i] == 0) i--;
      if (i < 0 || data[i] != (byte)0x80) CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      return i;
    }
    return length;
  }

  private void append(byte[] data, short offset, short length) {
    short newLength = (short)(bufferedLength + length);
    if (newLength < 0) throw new ArrayIndexOutOfBoundsException();
    ensureCapacity(newLength);
    for (short i = 0; i < length; i++) buffered[(short)(bufferedLength + i)] = data[(short)(offset + i)];
    bufferedLength = newLength;
  }

  private void ensureCapacity(short wanted) {
    if (wanted <= buffered.length) return;
    short size = (short)buffered.length;
    while (size < wanted) size = (short)(size * 2);
    byte[] next = new byte[size];
    for (short i = 0; i < bufferedLength; i++) next[i] = buffered[i];
    buffered = next;
  }

  private void clearBuffer() {
    for (short i = 0; i < bufferedLength; i++) buffered[i] = 0;
    bufferedLength = 0;
  }

  private boolean encrypting() { return mode == MODE_ENCRYPT; }

  private void transform(short length, byte[] output, short outputOffset) {
    CbcCipherNativeBridge.cipherCbc(algorithm, mode, keyData, iv, buffered,
        (short)0, length, output, outputOffset);
  }

  private void discard(short length) {
    short remaining = (short)(bufferedLength - length);
    for (short i = 0; i < remaining; i++) buffered[i] = buffered[(short)(length + i)];
    for (short i = remaining; i < bufferedLength; i++) buffered[i] = 0;
    bufferedLength = remaining;
  }

  private static void check(byte[] data, short offset, short length) {
    if (data == null) throw new NullPointerException();
    if (offset < 0 || length < 0 || offset > data.length - length)
      throw new ArrayIndexOutOfBoundsException();
  }
}
