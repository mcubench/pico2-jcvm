package javacard.security;

import fr.gouv.ssi.nativeimpl.NativeImplementation;

final class SecureRandomData extends RandomData {
  @Override
  public void generateData(byte[] buffer, short offset, short length)
      throws CryptoException {
    checkBufferRange(buffer, offset, length);
    if (length == 0) {
      CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
    }
    NativeImplementation.generateRandomData(buffer, offset, length);
  }

  @Override
  public short nextBytes(byte[] buffer, short offset, short length)
      throws CryptoException {
    generateData(buffer, offset, length);
    return (short) (offset + length);
  }

  @Override
  public void setSeed(byte[] buffer, short offset, short length) {
    checkBufferRange(buffer, offset, length);
  }

  @Override
  public byte getAlgorithm() {
    return ALG_SECURE_RANDOM;
  }

  private static void checkBufferRange(byte[] buffer, short offset,
                                       short length) {
    if (buffer == null) {
      throw new NullPointerException();
    }
    if (offset < 0 || length < 0 || offset > buffer.length - length) {
      throw new ArrayIndexOutOfBoundsException();
    }
  }
}
