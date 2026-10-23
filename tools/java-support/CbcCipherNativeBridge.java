package fr.gouv.ssi.nativeimpl;

/** Optional native entry point for selected CBC cipher providers. */
public final class CbcCipherNativeBridge {
  private CbcCipherNativeBridge() {}

  public static native void cipherCbc(byte algorithm, byte mode, byte[] key,
                                      byte[] iv, byte[] input,
                                      short inputOffset, short inputLength,
                                      byte[] output, short outputOffset);
}
