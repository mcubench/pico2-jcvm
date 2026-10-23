package fr.gouv.ssi.nativeimpl;

/** Native operations for the buffered SHA-256 implementation. */
public final class Sha256NativeBridge {
  private Sha256NativeBridge() {}

  /** Adds input to the bounded transient SHA-256 state buffer. */
  public static native boolean update(byte[] state, byte[] input,
                                      short inputOffset, short inputLength);

  /** Finalizes SHA-256 into the output buffer and clears transient state. */
  public static native void doFinal(byte[] state, byte[] output,
                                    short outputOffset);

  /** Clears buffered SHA-256 input and its encoded length. */
  public static native void reset(byte[] state);
}
