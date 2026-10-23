package javacard.security;

final class DESKeyProvider extends KeyProviderBase implements DESKey {
  DESKeyProvider(byte type, short size) { super(type, size); }
  public void setKey(byte[] keyData, short offset) { setKeyData(keyData, offset); }
  public byte getKey(byte[] keyData, short offset) { return getKeyData(keyData, offset); }
}
