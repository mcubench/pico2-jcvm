package javacard.security;

import fr.gouv.ssi.nativeimpl.MbedTlsDigestNativeBridge;
import javacard.framework.JCSystem;
import javacard.framework.Util;

/** Buffered Java Card digest using the selected Mbed TLS SHA implementation. */
final class MbedTlsDigestProvider extends MessageDigest {
  private static final short MAX_INPUT_LENGTH = 2046;
  private final byte algorithm;
  private final byte digestLength;
  private final byte[] input = JCSystem.makeTransientByteArray(
      MAX_INPUT_LENGTH, JCSystem.CLEAR_ON_RESET);
  private short inputLength;

  MbedTlsDigestProvider(byte algorithm) {
    this.algorithm = algorithm;
    switch (algorithm) {
      case ALG_SHA: digestLength = LENGTH_SHA; break;
      case ALG_SHA_224: digestLength = LENGTH_SHA_224; break;
      case ALG_SHA_384: digestLength = LENGTH_SHA_384; break;
      case ALG_SHA_512: digestLength = LENGTH_SHA_512; break;
      default: CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM); digestLength = 0;
    }
  }

  public byte getAlgorithm() { return algorithm; }
  public byte getLength() { return digestLength; }

  public void update(byte[] inBuff, short inOffset, short inLength)
      throws CryptoException {
    append(inBuff, inOffset, inLength);
  }

  public short doFinal(byte[] inBuff, short inOffset, short inLength,
                       byte[] outBuff, short outOffset) throws CryptoException {
    if (outBuff == null) throw new NullPointerException();
    if (outOffset < 0 || outOffset + digestLength > outBuff.length)
      throw new ArrayIndexOutOfBoundsException();
    append(inBuff, inOffset, inLength);
    if (!MbedTlsDigestNativeBridge.digest(algorithm, input, inputLength,
                                          outBuff, outOffset)) {
      inputLength = 0;
      CryptoException.throwIt(CryptoException.ILLEGAL_USE);
    }
    inputLength = 0;
    return digestLength;
  }

  public void reset() { inputLength = 0; }

  private void append(byte[] source, short offset, short length) {
    if (source == null) throw new NullPointerException();
    if (offset < 0 || length < 0 || offset + length > source.length)
      throw new ArrayIndexOutOfBoundsException();
    if (inputLength + length > MAX_INPUT_LENGTH)
      CryptoException.throwIt(CryptoException.ILLEGAL_USE);
    Util.arrayCopyNonAtomic(source, offset, input, inputLength, length);
    inputLength += length;
  }
}
