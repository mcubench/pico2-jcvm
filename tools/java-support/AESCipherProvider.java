package javacardx.crypto;

import javacard.security.AESKey;
import javacard.security.Key;
import javacard.security.KeyBuilder;

final class AESCipherProvider extends CbcCipherProvider {
  AESCipherProvider() { super(ALG_AES_BLOCK_128_CBC_NOPAD, (byte) 16); }
  byte providerKeyType() { return KeyBuilder.TYPE_AES; }
  boolean providerSupportsKeySize(short size) {
    return size == 16 || size == 24 || size == 32;
  }
  void providerCopyKey(Key key, byte[] destination) {
    ((AESKey) key).getKey(destination, (short) 0);
  }
  byte providerCipherAlgorithm() { return CIPHER_AES_CBC; }
}
