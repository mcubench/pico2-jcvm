package javacardx.crypto;

import javacard.security.DESKey;
import javacard.security.Key;
import javacard.security.KeyBuilder;

final class DESCipherProvider extends CbcCipherProvider {
  DESCipherProvider() { super(ALG_DES_CBC_NOPAD, (byte) 8); }
  byte providerKeyType() { return KeyBuilder.TYPE_DES; }
  boolean providerSupportsKeySize(short size) {
    return size == 8 || size == 16 || size == 24;
  }
  void providerCopyKey(Key key, byte[] destination) {
    ((DESKey) key).getKey(destination, (short) 0);
  }
  byte providerCipherAlgorithm() { return CIPHER_DES_CBC; }
}
