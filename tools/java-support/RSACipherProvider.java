package fr.gouv.ssi.nativeimpl;

import javacard.security.CryptoException;
import javacard.security.Key;
import javacardx.crypto.Cipher;

/** Native RSA cipher state object for the Java Card API. */
public final class RSACipherProvider extends Cipher {
  private final byte algorithm;
  private final byte padding;
  private byte[] modulus;
  private byte[] exponent;
  private byte[] buffered;
  private short bufferedLength;
  private short modulusLength;
  private byte mode;
  private boolean privateOperation;
  private boolean initialized;

  public RSACipherProvider(byte algorithm, byte padding) {
    this.algorithm = algorithm;
    this.padding = padding;
  }

  public native void init(Key key, byte requestedMode) throws CryptoException;

  public native void init(Key key, byte requestedMode, byte[] parameters,
                          short offset, short length) throws CryptoException;

  public native byte getAlgorithm();
  public native byte getCipherAlgorithm();
  public native byte getPaddingAlgorithm();

  public native short update(byte[] input, short inputOffset, short inputLength,
                             byte[] output, short outputOffset)
      throws CryptoException;

  public native short doFinal(byte[] input, short inputOffset,
                              short inputLength, byte[] output,
                              short outputOffset) throws CryptoException;
}
