package rmi.fixture;

import javacard.security.CryptoException;

public class CustomCryptoExceptionBase extends CryptoException {
  public CustomCryptoExceptionBase(short reason) {
    super(reason);
  }
}
