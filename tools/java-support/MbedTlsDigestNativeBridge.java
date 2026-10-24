package fr.gouv.ssi.nativeimpl;

/** One-shot Mbed TLS digest operations used by the buffered Java Card provider. */
public final class MbedTlsDigestNativeBridge {
  private MbedTlsDigestNativeBridge() {}

  public static native boolean digest(byte algorithm, byte[] input,
                                      short inputLength, byte[] output,
                                      short outputOffset);
}
