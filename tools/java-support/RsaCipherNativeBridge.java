package fr.gouv.ssi.nativeimpl;

/** Raw RSA and the Java Card PKCS#1 v1.5 / OAEP encodings. */
public final class RsaCipherNativeBridge {
  private RsaCipherNativeBridge() {}

  public static native short transform(byte padding, boolean privateOperation,
      byte[] modulus, byte[] exponent, byte[] input, short inputLength,
      byte[] output);
}
