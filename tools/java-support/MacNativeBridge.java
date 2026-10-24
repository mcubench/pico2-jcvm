package fr.gouv.ssi.nativeimpl;

/** Native implementation of the selected symmetric MAC algorithms. */
public final class MacNativeBridge {
  private MacNativeBridge() {}
  public static native void computeMac(byte algorithm, byte[] key, byte[] iv,
      byte[] message, short messageLength, byte[] output);
}
