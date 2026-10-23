package javacard.security;

import fr.gouv.ssi.nativeimpl.Sha256NativeBridge;
import javacard.framework.JCSystem;

final class Sha256DigestProvider extends MessageDigest {
    private static final short MAX_INPUT_LENGTH = 2046;
    private final byte[] input = JCSystem.makeTransientByteArray(
        (short)(MAX_INPUT_LENGTH + 2), JCSystem.CLEAR_ON_RESET);

    Sha256DigestProvider() {}

    @Override
    public byte getAlgorithm() {
      return ALG_SHA_256;
    }

    @Override
    public byte getLength() {
      return LENGTH_SHA_256;
    }

    @Override
    public short doFinal(byte[] inBuff, short inOffset, short inLength,
                         byte[] outBuff, short outOffset)
        throws CryptoException {
      if (outBuff == null) {
        throw new NullPointerException();
      }
      if (outOffset < 0 || outOffset + LENGTH_SHA_256 > outBuff.length) {
        throw new ArrayIndexOutOfBoundsException();
      }
      byte[] state = input;
      if (!Sha256NativeBridge.update(state, inBuff, inOffset, inLength)) {
        CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      }
      Sha256NativeBridge.doFinal(state, outBuff, outOffset);
      return LENGTH_SHA_256;
    }

    @Override
    public void update(byte[] inBuff, short inOffset, short inLength)
        throws CryptoException {
      if (inBuff == null) {
        throw new NullPointerException();
      }
      if (inOffset < 0 || inLength < 0 ||
          inOffset + inLength > inBuff.length) {
        throw new ArrayIndexOutOfBoundsException();
      }
      if (!Sha256NativeBridge.update(input, inBuff, inOffset, inLength)) {
        CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      }
    }

    @Override
    public void reset() {
      Sha256NativeBridge.reset(input);
    }
  }
