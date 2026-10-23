package javacard.security;

abstract class KeyProviderBase implements SecretKey {
  private final byte type;
  private final short size;
  private final byte[] keyData;
  private boolean initialized;

  KeyProviderBase(byte type, short size) {
    this.type = type;
    this.size = size;
    this.keyData = new byte[(short) (size / 8)];
  }

  public boolean isInitialized() { return initialized; }

  public void clearKey() {
    for (short index = 0; index < keyData.length; index++) keyData[index] = 0;
    initialized = false;
  }

  public byte getType() { return type; }
  public short getSize() { return size; }

  protected void setKeyData(byte[] source, short offset) {
    checkKeyRange(source, offset, (short) keyData.length);
    for (short index = 0; index < keyData.length; index++) {
      keyData[index] = source[(short) (offset + index)];
    }
    initialized = true;
  }

  protected byte getKeyData(byte[] destination, short offset) {
    if (!initialized) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
    checkKeyRange(destination, offset, (short) keyData.length);
    for (short index = 0; index < keyData.length; index++) {
      destination[(short) (offset + index)] = keyData[index];
    }
    return (byte) keyData.length;
  }

  private static void checkKeyRange(byte[] buffer, short offset, short length) {
    if (buffer == null) throw new NullPointerException();
    if (offset < 0 || offset > buffer.length - length) {
      throw new ArrayIndexOutOfBoundsException();
    }
  }
}
