package javacardx.crypto;

import fr.gouv.ssi.nativeimpl.NativeImplementation;
import javacard.security.CryptoException;
import javacard.security.Key;

abstract class CbcCipherProvider extends Cipher {
    private final byte algorithm;
    private final byte blockSize;
    private byte[] keyData;
    private final byte[] iv;
    private final byte[] pending;
    private short pendingLength;
    private byte mode;
    private boolean initialized;

    CbcCipherProvider(byte algorithm, byte blockSize) {
      this.algorithm = algorithm;
      this.blockSize = blockSize;
      this.keyData = new byte[32];
      this.iv = new byte[16];
      this.pending = new byte[blockSize];
      this.pendingLength = 0;
      this.mode = 0;
      this.initialized = false;
    }

    abstract byte providerKeyType();

    abstract boolean providerSupportsKeySize(short size);

    abstract void providerCopyKey(Key key, byte[] destination);

    abstract byte providerCipherAlgorithm();

    public void init(Key key, byte requestedMode) throws CryptoException {
      if (key == null) {
        throw new NullPointerException();
      }
      if (requestedMode != MODE_ENCRYPT && requestedMode != MODE_DECRYPT) {
        CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
      }
      if (key.getType() != providerKeyType()) {
        CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
      }
      short keyLength = (short) (key.getSize() / 8);
      if (!providerSupportsKeySize(keyLength)) {
        CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
      }
      keyData = new byte[keyLength];
      providerCopyKey(key, keyData);
      for (short index = 0; index < iv.length; index++) {
        iv[index] = 0;
      }
      pendingLength = 0;
      mode = requestedMode;
      initialized = true;
    }

    public void init(Key key, byte requestedMode, byte[] parameters,
                     short offset, short length) throws CryptoException {
      if (parameters == null) {
        throw new NullPointerException();
      }
      if (offset < 0 || length != blockSize ||
          offset > parameters.length - length) {
        CryptoException.throwIt(CryptoException.ILLEGAL_VALUE);
      }
      init(key, requestedMode);
      for (short index = 0; index < blockSize; index++) {
        iv[index] = parameters[(short) (offset + index)];
      }
    }

    public byte getAlgorithm() { return algorithm; }

    public byte getCipherAlgorithm() { return providerCipherAlgorithm(); }

    public byte getPaddingAlgorithm() { return PAD_NOPAD; }

    public short doFinal(byte[] input, short inputOffset, short inputLength,
                         byte[] output, short outputOffset)
        throws CryptoException {
      return process(input, inputOffset, inputLength, output, outputOffset,
                     true);
    }

    public short update(byte[] input, short inputOffset, short inputLength,
                        byte[] output, short outputOffset)
        throws CryptoException {
      return process(input, inputOffset, inputLength, output, outputOffset,
                     false);
    }

    private short process(byte[] input, short inputOffset, short inputLength,
                          byte[] output, short outputOffset, boolean finalizing)
        throws CryptoException {
      if (!initialized) {
        CryptoException.throwIt(CryptoException.INVALID_INIT);
      }
      checkBuffer(input, inputOffset, inputLength);
      checkBuffer(output, outputOffset, (short) 0);
      int combinedLength = pendingLength + inputLength;
      int outputLength = finalizing
          ? combinedLength
          : combinedLength - (combinedLength % blockSize);
      if (finalizing && outputLength % blockSize != 0) {
        CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      }
      if (finalizing && combinedLength == 0) {
        CryptoException.throwIt(CryptoException.ILLEGAL_USE);
      }
      if (outputLength > Short.MAX_VALUE) {
        throw new ArrayIndexOutOfBoundsException();
      }
      checkBuffer(output, outputOffset, (short) outputLength);

      byte[] cipherInput = input;
      short cipherInputOffset = inputOffset;
      if (pendingLength != 0 || outputLength != inputLength) {
        cipherInput = new byte[combinedLength];
        for (short index = 0; index < pendingLength; index++) {
          cipherInput[index] = pending[index];
        }
        for (short index = 0; index < inputLength; index++) {
          cipherInput[(short) (pendingLength + index)] =
              input[(short) (inputOffset + index)];
        }
        cipherInputOffset = 0;
      }

      if (outputLength != 0) {
        short lastBlockOffset = (short) (outputLength - blockSize);
        boolean saveInputIv = mode == MODE_DECRYPT && cipherInput == output;
        if (saveInputIv) {
          for (short index = 0; index < blockSize; index++) {
            pending[index] = cipherInput[(short) (cipherInputOffset +
                                                   lastBlockOffset + index)];
          }
        }
        NativeImplementation.cipherCbc(
            algorithm, mode, keyData, iv, cipherInput, cipherInputOffset,
            (short) outputLength,
            output, outputOffset);
        for (short index = 0; index < blockSize; index++) {
          iv[index] = mode == MODE_ENCRYPT
              ? output[(short) (outputOffset + lastBlockOffset + index)]
              : saveInputIv
                  ? pending[index]
                  : cipherInput[(short) (cipherInputOffset + lastBlockOffset + index)];
        }
      }

      pendingLength = (short) (combinedLength - outputLength);
      for (short index = 0; index < pendingLength; index++) {
        pending[index] = cipherInput[(short) (cipherInputOffset + outputLength + index)];
      }
      if (finalizing) {
        pendingLength = 0;
        for (short index = 0; index < iv.length; index++) {
          iv[index] = 0;
        }
      }
      return (short) outputLength;
    }

    private static void checkBuffer(byte[] buffer, short offset, short length) {
      if (buffer == null) {
        throw new NullPointerException();
      }
      if (offset < 0 || length < 0 || offset > buffer.length - length) {
        throw new ArrayIndexOutOfBoundsException();
      }
    }
  }
