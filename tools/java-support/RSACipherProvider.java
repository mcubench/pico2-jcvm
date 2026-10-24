package javacardx.crypto;

import fr.gouv.ssi.nativeimpl.RsaCipherNativeBridge;
import javacard.security.CryptoException;
import javacard.security.Key;
import javacard.security.KeyBuilder;
import javacard.security.RSAPrivateKey;
import javacard.security.RSAPublicKey;

/** RSA Cipher facade. Each doFinal performs one modulus-sized RSA operation. */
final class RSACipherProvider extends Cipher {
  private final byte algorithm;
  private final byte padding;
  private byte[] modulus;
  private byte[] exponent;
  private byte[] buffered = new byte[0];
  private short bufferedLength;
  private short modulusLength;
  private byte mode;
  private boolean privateOperation;
  private boolean initialized;

  RSACipherProvider(byte algorithm, byte padding) {
    this.algorithm = algorithm;
    this.padding = padding;
  }

  public void init(Key key, byte requestedMode) throws CryptoException {
    if (key == null) throw new NullPointerException();
    if (requestedMode != MODE_ENCRYPT && requestedMode != MODE_DECRYPT)
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    if (!key.isInitialized()) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    if (key instanceof RSAPublicKey && requestedMode == MODE_ENCRYPT) {
      RSAPublicKey rsa = (RSAPublicKey)key;
      privateOperation = false;
      modulusLength = (short)((rsa.getSize() + 7) / 8);
      modulus = new byte[modulusLength];
      exponent = new byte[modulusLength];
      short n = rsa.getModulus(modulus, (short)0);
      short e = rsa.getExponent(exponent, (short)0);
      rightAlign(modulus, n);
      rightAlign(exponent, e);
    } else if (key instanceof RSAPrivateKey &&
               requestedMode == MODE_DECRYPT &&
               key.getType() != KeyBuilder.TYPE_RSA_CRT_PRIVATE &&
               key.getType() != KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_RESET &&
               key.getType() != KeyBuilder.TYPE_RSA_CRT_PRIVATE_TRANSIENT_DESELECT) {
      RSAPrivateKey rsa = (RSAPrivateKey)key;
      privateOperation = true;
      modulusLength = (short)((rsa.getSize() + 7) / 8);
      modulus = new byte[modulusLength];
      exponent = new byte[modulusLength];
      short n = rsa.getModulus(modulus, (short)0);
      short d = rsa.getExponent(exponent, (short)0);
      rightAlign(modulus, n);
      rightAlign(exponent, d);
    } else {
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    }
    mode = requestedMode;
    bufferedLength = 0;
    initialized = true;
  }

  public void init(Key key, byte requestedMode, byte[] parameters,
                   short offset, short length) throws CryptoException {
    CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
  }

  public byte getAlgorithm() { return algorithm; }
  public byte getCipherAlgorithm() { return CIPHER_RSA; }
  public byte getPaddingAlgorithm() { return padding; }

  public short update(byte[] input, short inputOffset, short inputLength,
                      byte[] output, short outputOffset) throws CryptoException {
    if (!initialized) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, inputOffset, inputLength);
    check(output, outputOffset, (short)0);
    append(input, inputOffset, inputLength);
    return 0;
  }

  public short doFinal(byte[] input, short inputOffset, short inputLength,
                       byte[] output, short outputOffset) throws CryptoException {
    if (!initialized) CryptoException.throwIt(CryptoException.INVALID_INIT);
    check(input, inputOffset, inputLength);
    check(output, outputOffset, (short)0);
    append(input, inputOffset, inputLength);
    byte[] result = new byte[modulusLength];
    short resultLength = RsaCipherNativeBridge.transform(padding,
        privateOperation, modulus, exponent, buffered, bufferedLength, result);
    check(output, outputOffset, resultLength);
    for (short i = 0; i < resultLength; i++)
      output[(short)(outputOffset + i)] = result[i];
    clearBuffer();
    return resultLength;
  }

  private void rightAlign(byte[] value, short length) {
    short start = (short)(value.length - length);
    if (start <= 0) return;
    for (short i = (short)(length - 1); i >= 0; i--)
      value[(short)(start + i)] = value[i];
    for (short i = 0; i < start; i++) value[i] = 0;
  }

  private void append(byte[] source, short offset, short length) {
    short wanted = (short)(bufferedLength + length);
    if (wanted < 0 || wanted > modulusLength)
      CryptoException.throwIt(CryptoException.ILLEGAL_USE);
    if (wanted > buffered.length) {
      byte[] next = new byte[wanted];
      for (short i = 0; i < bufferedLength; i++) next[i] = buffered[i];
      buffered = next;
    }
    for (short i = 0; i < length; i++)
      buffered[(short)(bufferedLength + i)] = source[(short)(offset + i)];
    bufferedLength = wanted;
  }

  private void clearBuffer() {
    for (short i = 0; i < bufferedLength; i++) buffered[i] = 0;
    bufferedLength = 0;
  }

  private static void check(byte[] array, short offset, short length) {
    if (array == null) throw new NullPointerException();
    if (offset < 0 || length < 0 || offset > array.length - length)
      throw new ArrayIndexOutOfBoundsException();
  }
}
