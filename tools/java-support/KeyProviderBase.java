package javacard.security;

import javacard.framework.JCSystem;

abstract class KeyProviderBase implements SecretKey {
  private final byte type;
  private final short size;
  private final byte[] keyData;
  private final byte[] transientInitialized;
  private boolean persistentInitialized;

  KeyProviderBase(byte type, short size) {
    this.type = type;
    this.size = size;
    byte event = (type == KeyBuilder.TYPE_AES_TRANSIENT_DESELECT ||
                  type == KeyBuilder.TYPE_DES_TRANSIENT_DESELECT)
        ? JCSystem.CLEAR_ON_DESELECT : JCSystem.CLEAR_ON_RESET;
    boolean transientKey = type == KeyBuilder.TYPE_AES_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_AES_TRANSIENT_DESELECT ||
        type == KeyBuilder.TYPE_DES_TRANSIENT_RESET ||
        type == KeyBuilder.TYPE_DES_TRANSIENT_DESELECT;
    this.keyData = transientKey
        ? JCSystem.makeTransientByteArray((short) (size / 8), event)
        : new byte[(short) (size / 8)];
    this.transientInitialized = transientKey
        ? JCSystem.makeTransientByteArray((short) 1, event) : null;
  }

  public boolean isInitialized() {
    return transientInitialized != null
        ? transientInitialized[0] != 0 : persistentInitialized;
  }

  public void clearKey() {
    for (short index = 0; index < keyData.length; index++) keyData[index] = 0;
    setInitialized(false);
  }

  public byte getType() { return type; }
  public short getSize() { return size; }

  protected void setKeyData(byte[] source, short offset) {
    checkKeyRange(source, offset, (short) keyData.length);
    for (short index = 0; index < keyData.length; index++) {
      keyData[index] = source[(short) (offset + index)];
    }
    setInitialized(true);
  }

  protected byte getKeyData(byte[] destination, short offset) {
    if (!isInitialized()) CryptoException.throwIt(CryptoException.UNINITIALIZED_KEY);
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

  private void setInitialized(boolean value) {
    if (transientInitialized != null) transientInitialized[0] = value ? (byte)1 : (byte)0;
    else persistentInitialized = value;
  }
}
