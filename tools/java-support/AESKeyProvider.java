package javacard.security;

final class AESKeyProvider extends KeyProviderBase implements AESKey {
  AESKeyProvider(byte type, short size) { super(type, size); }
  public void setKey(byte[] keyData, short offset) { setKeyData(keyData, offset); }
  public byte getKey(byte[] keyData, short offset) { return getKeyData(keyData, offset); }
}
