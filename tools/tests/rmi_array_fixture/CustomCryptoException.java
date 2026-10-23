package rmi.fixture;

import javacard.security.CryptoException;

public final class CustomCryptoException extends CryptoException {
  public CustomCryptoException(short reason) {
    super(reason);
  }
}
