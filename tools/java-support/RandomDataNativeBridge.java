package fr.gouv.ssi.nativeimpl;

/** Optional native entry point for Java Card random data. */
public final class RandomDataNativeBridge {
  private RandomDataNativeBridge() {}

  public static native void generateRandomData(byte[] buffer, short offset,
                                               short length);
}
