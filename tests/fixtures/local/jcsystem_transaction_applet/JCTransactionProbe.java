package algtest;

import javacard.framework.APDU;
import javacard.framework.APDUException;
import javacard.framework.AID;
import javacard.framework.Applet;
import javacard.framework.CardRuntimeException;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.OwnerPIN;
import javacard.framework.OwnerPINBuilder;
import javacard.framework.OwnerPINx;
import javacard.framework.OwnerPINxWithPredecrement;
import javacard.framework.PINException;
import javacard.framework.SensitiveArrays;
import javacard.framework.SystemException;
import javacard.framework.TransactionException;
import javacard.framework.UserException;
import javacard.framework.Util;
import javacard.security.MessageDigest;
import javacard.security.InitializedMessageDigest;
import javacard.security.CryptoException;
import javacard.security.AESKey;
import javacard.security.ECPrivateKey;
import javacard.security.HMACKey;
import javacard.security.KeyBuilder;
import javacard.security.RandomData;
import javacard.security.Signature;
import javacardx.crypto.AEADCipher;
import javacardx.crypto.Cipher;
import javacardx.framework.util.ArrayLogic;
import javacardx.framework.util.UtilException;

public final class JCTransactionProbe extends Applet {
  private static Object staticReferenceRoot;
  private static Object[] persistentReferenceArray;
  private static NestedArrayHolder persistentNestedReferenceHolder;
  private static SharedArrayHolder persistentReferenceHolder;
  private static byte[] gcRehydrateAliasA;
  private static byte[] gcRehydrateAliasB;
  private Object globalArrayInstanceReference;
  private OwnerPIN persistentPin;
  private OwnerPINxWithPredecrement persistentPinX;
  // RAM cost probe (cases 107-110): objects kept reachable from a static.
  private static Object[] memoryProbe;
  // OneShot probe (cases 112-115): stores that must be refused.
  private static Object staticOneShot;
  private Object oneShotField;
  private Object[] oneShotArray;
  private AESKey oneShotKey;
  private Cipher oneShotReference;
  // 1: open OneShots in select() and again in the selecting process();
  // 2: both succeeded; 3: process() got NO_RESOURCE.
  private byte oneShotSelectProbe;
  private InitializedMessageDigest initializedDigest;
  private MessageDigest streamingDigest;
  private byte[] streamingInput;
  // SHA-256 intermediate state after 64 bytes of 0x61 (cases 119/120).
  private static final byte[] SHA256_STATE_AFTER_64A = {
      (byte)0xDF, (byte)0x5B, (byte)0xB8, (byte)0x1C, (byte)0xE8, (byte)0x1E, (byte)0x06, (byte)0x26, (byte)0xFB, (byte)0x45, (byte)0xA8, (byte)0x94, (byte)0x4F, (byte)0xD4, (byte)0x0F, (byte)0x31, (byte)0xB2, (byte)0x5E, (byte)0x68, (byte)0x16, (byte)0xD6, (byte)0xD4, (byte)0x99, (byte)0xC1, (byte)0xAB, (byte)0x90, (byte)0x49, (byte)0x29, (byte)0x00, (byte)0x63, (byte)0x5E, (byte)0x66};
  private short[] memoryProbeWords;
  private byte value = 0x11;
  private byte selectCount;
  private byte deselectCount;
  private byte lastSelectWasReselect;
  private byte lastSelectProcessWasReselect;
  private byte selectingAppletProbeFlags;
  private byte selectDeselectTransientResult;
  private byte rejectNextSelect;
  private HMACKey replacementProbeKey;
  // Open review findings (docs/review_claude_20261127.md), cases 147-158.
  private static byte[] installArgumentStore;
  private static byte installArgumentStoreRejected;
  private static byte[] staticApduBuffer;
  private Object apduBufferField;
  private Object apduObjectField;
  private Object[] apduStoreArray;
  private byte installTransactionBeforeRegister;
  private byte installTransactionAfterRegister;
  private Cipher deselectKeyCipher;
  private Cipher reinitializedCipher;
  private byte[] cipherReference;
  private byte beginTransactionOnNextSelect;
  private byte beginTransactionOnNextDeselect;
  private byte currentApduHiddenOutsideProcess;
  private byte currentApduBufferHiddenOutsideProcess;
  private byte lastApduExceptionReason = (byte)0x7f;
  private byte explicitRegistrationFailureReason;
  private byte installSawAIDNullBeforeRegister;
  private byte installSawAppletActive;
  private byte selectSawAppletActive;
  private byte deselectSawAppletActive;
  private static byte[] deletionOrphan;
  private byte[] transientResetArray;
  private byte[] transientDeselectArray;
  private final byte[] shaInput = {0x61, 0x62, 0x63};
  private final byte[] shaDigest = new byte[32];
  private final byte[] transactionCapacityArray;
  private short[] arrayLogicShortArray;
  private int[] arrayLogicIntArray;
  private final byte[] transactionSmallArray = {0x11, 0x12};
  private final byte[] transactionOtherArray = {0x21, 0x22};

  private JCTransactionProbe() {
    this(null, (short)0, (byte)0, false, true);
  }

  private JCTransactionProbe(byte[] requestedAID, short offset, byte length) {
    this(requestedAID, offset, length, true, true);
  }

  private JCTransactionProbe(byte[] requestedAID, short offset, byte length,
                             boolean explicitAID, boolean allocateCapacityArray) {
    transactionCapacityArray = allocateCapacityArray ? new byte[32767] : new byte[1];
    installSawAIDNullBeforeRegister =
        JCSystem.getAID() == null ? (byte)1 : (byte)0;
    installTransactionBeforeRegister = installTransactionProbe();
    if (explicitAID) {
      try {
        register(requestedAID, offset, length);
      } catch (SystemException expected) {
        explicitRegistrationFailureReason = (byte)expected.getReason();
        register();
      }
    } else {
      register();
    }
    installSawAppletActive =
        JCSystem.isAppletActive(JCSystem.getAID()) ? (byte)1 : (byte)0;
    installTransactionAfterRegister = installTransactionProbe();
    staticReferenceRoot = this;
    byte[] shared = {0x01};
    persistentReferenceArray = new Object[257];
    persistentReferenceArray[0] = shared;
    persistentReferenceArray[1] = shared;
    byte[] nestedLeaf = {0x6b};
    persistentNestedReferenceHolder = new NestedArrayHolder(
        new Object[] {nestedLeaf, nestedLeaf});
    persistentReferenceHolder = new SharedArrayHolder(shared);
    byte[] gcRehydratable = {0x5a};
    gcRehydrateAliasA = gcRehydratable;
    gcRehydrateAliasB = gcRehydratable;
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    // Finding #2: the install argument is a JCRE global array.
    try {
      installArgumentStore = bArray;
    } catch (SecurityException expected) {
      installArgumentStoreRejected = 1;
    }
    installArgumentStore = null;
    if (bLength == 10) {
      new JCTransactionProbe(bArray, bOffset, bLength);
    } else if ((bLength == 1 && bArray[bOffset] == (byte)0x7f) ||
               (bLength == 3 && bArray[bOffset] == (byte)0xc9 &&
                bArray[(short)(bOffset + 1)] == (byte)1 &&
                bArray[(short)(bOffset + 2)] == (byte)0x7f)) {
      new JCTransactionProbe(null, (short)0, (byte)0, false, false);
    } else {
      new JCTransactionProbe();
    }
  }

  public boolean select() {
    selectCount++;
    if (oneShotSelectProbe == 1) {
      // Kept open on purpose: returning from select() must release them.
      MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
      MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
    }
    selectDeselectTransientResult = 0;
    try {
      byte[] transientDuringSelect = JCSystem.makeTransientByteArray(
          (short)1, JCSystem.CLEAR_ON_DESELECT);
      selectDeselectTransientResult =
          transientDuringSelect != null &&
                  JCSystem.isTransient(transientDuringSelect) ==
                      JCSystem.CLEAR_ON_DESELECT
              ? (byte)1 : (byte)2;
    } catch (SystemException expected) {
      selectDeselectTransientResult =
          expected.getReason() == SystemException.ILLEGAL_TRANSIENT
              ? (byte)3 : (byte)4;
    }
    selectSawAppletActive =
        JCSystem.isAppletActive(JCSystem.getAID()) ? (byte)1 : (byte)0;
    if (selectingApplet()) selectingAppletProbeFlags |= 1;
    lastSelectWasReselect = reSelectingApplet() ? (byte)1 : (byte)0;
    try {
      APDU.getCurrentAPDU();
      currentApduHiddenOutsideProcess = 0;
    } catch (SecurityException expected) {
      currentApduHiddenOutsideProcess = 1;
    }
    try {
      APDU.getCurrentAPDUBuffer();
      currentApduBufferHiddenOutsideProcess = 0;
    } catch (SecurityException expected) {
      currentApduBufferHiddenOutsideProcess = 1;
    }
    if (rejectNextSelect == 2) {
      rejectNextSelect = 0;
      ISOException.throwIt(ISO7816.SW_WRONG_DATA);
    }
    boolean rejectSelection = rejectNextSelect != 0;
    if (rejectSelection) rejectNextSelect = 0;
    if (beginTransactionOnNextSelect != 0) {
      beginTransactionOnNextSelect = 0;
      JCSystem.beginTransaction();
      value = 0x44;
    }
    if (rejectSelection) return false;
    return true;
  }

  public void deselect() {
    if (beginTransactionOnNextDeselect != 0) {
      beginTransactionOnNextDeselect = 0;
      JCSystem.beginTransaction();
      value = 0x55;
    }
    deselectCount++;
    deselectSawAppletActive =
        JCSystem.isAppletActive(JCSystem.getAID()) ? (byte)1 : (byte)0;
    selectingAppletProbeFlags |= selectingApplet() ? (byte)4 : (byte)8;
  }

  private short appendHeapFree(byte[] output, short offset) {
    JCSystem.getAvailableMemory(memoryProbeWords, (short)0,
        JCSystem.MEMORY_TYPE_TRANSIENT_RESET);
    Util.setShort(output, offset, memoryProbeWords[0]);
    Util.setShort(output, (short)(offset + 2), memoryProbeWords[1]);
    return (short)(offset + 4);
  }

  private static short appendMemory(byte[] output, short offset, byte type) {
    short scalar = JCSystem.getAvailableMemory(type);
    short[] extended = new short[2];
    JCSystem.getAvailableMemory(extended, (short)0, type);
    output[offset++] = (byte)(scalar >> 8);
    output[offset++] = (byte)scalar;
    output[offset++] = (byte)(extended[0] >> 8);
    output[offset++] = (byte)extended[0];
    output[offset++] = (byte)(extended[1] >> 8);
    output[offset++] = (byte)extended[1];
    return offset;
  }

  private static short countFrames(short remaining) {
    if (remaining == 0) return 1;
    return (short)(countFrames((short)(remaining - 1)) + 1);
  }

  private static byte denseIntSwitch(int key) {
    switch (key) {
      case 65536: return 1;
      case 65537: return 2;
      case 65538: return 3;
      default: return 4;
    }
  }

  // The 3.0.5 converter crashes (InstrContainer.reset) on this int local inside process().
  private static short denseIntSwitchProbe(byte[] buffer) {
    int base = 65536 + buffer[ISO7816.OFFSET_P2];
    try {
      for (short index = 0; index < 4; index++) {
        buffer[index] = denseIntSwitch(base + index);
      }
    } catch (RuntimeException unexpected) {
      ISOException.throwIt(ISO7816.SW_WRONG_DATA);
    }
    return 4;
  }

  // Returns 0 when ECDSA accepts a fully initialized transient P-256 private key.
  private static byte transientEcdsaInitReason() {
    byte[] p = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff};
    byte[] a = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xfc};
    byte[] b = {(byte)0x5a,(byte)0xc6,0x35,(byte)0xd8,(byte)0xaa,0x3a,(byte)0x93,(byte)0xe7,(byte)0xb3,(byte)0xeb,(byte)0xbd,0x55,0x76,(byte)0x98,(byte)0x86,(byte)0xbc,0x65,0x1d,0x06,(byte)0xb0,(byte)0xcc,0x53,(byte)0xb0,(byte)0xf6,0x3b,(byte)0xce,0x3c,0x3e,0x27,(byte)0xd2,0x60,0x4b};
    byte[] g = {4,0x6b,0x17,(byte)0xd1,(byte)0xf2,(byte)0xe1,0x2c,0x42,0x47,(byte)0xf8,(byte)0xbc,(byte)0xe6,(byte)0xe5,0x63,(byte)0xa4,0x40,(byte)0xf2,0x77,0x03,0x7d,(byte)0x81,0x2d,(byte)0xeb,0x33,(byte)0xa0,(byte)0xf4,(byte)0xa1,0x39,0x45,(byte)0xd8,(byte)0x98,(byte)0xc2,(byte)0x96,0x4f,(byte)0xe3,0x42,(byte)0xe2,(byte)0xfe,0x1a,0x7f,(byte)0x9b,(byte)0x8e,(byte)0xe7,(byte)0xeb,0x4a,0x7c,0x0f,(byte)0x9e,0x16,0x2b,(byte)0xce,0x33,0x57,0x6b,0x31,0x5e,(byte)0xce,(byte)0xcb,(byte)0xb6,0x40,0x68,0x37,(byte)0xbf,0x51,(byte)0xf5};
    byte[] n = {(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,0,0,0,0,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xff,(byte)0xbc,(byte)0xe6,(byte)0xfa,(byte)0xad,(byte)0xa7,0x17,(byte)0x9e,(byte)0x84,(byte)0xf3,(byte)0xb9,(byte)0xca,(byte)0xc2,(byte)0xfc,0x63,0x25,0x51};
    ECPrivateKey key = (ECPrivateKey)KeyBuilder.buildKey(
        KeyBuilder.TYPE_EC_FP_PRIVATE_TRANSIENT_RESET,
        KeyBuilder.LENGTH_EC_FP_256, false);
    key.setFieldFP(p, (short)0, (short)p.length);
    key.setA(a, (short)0, (short)a.length);
    key.setB(b, (short)0, (short)b.length);
    key.setG(g, (short)0, (short)g.length);
    key.setR(n, (short)0, (short)n.length);
    key.setK((short)1);
    key.setS(g, (short)1, (short)1);
    if (!key.isInitialized()) return (byte)0x7f;
    Signature signature = Signature.getInstance(Signature.ALG_ECDSA_SHA_256, false);
    try {
      signature.init(key, Signature.MODE_SIGN);
    } catch (CryptoException expected) {
      return (byte)expected.getReason();
    }
    return 0;
  }

  // Finding #8: 1 when begin/commit run without exception and, after
  // register(), open a real applet transaction; 0x10|reason on failure.
  private static byte installTransactionProbe() {
    try {
      JCSystem.beginTransaction();
      byte depth = JCSystem.getTransactionDepth();
      JCSystem.commitTransaction();
      return depth == 1 || JCSystem.getAID() == null ? (byte)1 : (byte)0x20;
    } catch (TransactionException failure) {
      return (byte)(0x10 | failure.getReason());
    }
  }

  // Finding #21: an applet-defined ISOException subclass.
  static final class ProbeISOException extends ISOException {
    ProbeISOException(short sw) {
      super(sw);
    }
  }

  // Finding #27: the build replaces the checkcast with nops, so athrow gets
  // an object that is not Throwable.
  private static void throwNonThrowable(Object value) {
    throw (RuntimeException)value;
  }

  private static AESKey aesKey(byte type, byte fill) {
    AESKey key = (AESKey)KeyBuilder.buildKey(type, KeyBuilder.LENGTH_AES_128, false);
    byte[] bytes = new byte[16];
    Util.arrayFillNonAtomic(bytes, (short)0, (short)16, fill);
    key.setKey(bytes, (short)0);
    return key;
  }

  private static boolean sameBytes(byte[] left, byte[] right, short length) {
    return Util.arrayCompare(left, (short)0, right, (short)0, length) == 0;
  }

  // Finding #13: GCM with no IV equals an explicit zero IV, and doFinal
  // resets to the initialized state. Returns 1 when both hold.
  private static byte gcmResetProbe() {
    AESKey key = aesKey(KeyBuilder.TYPE_AES, (byte)0x2B);
    AEADCipher gcm = (AEADCipher)Cipher.getInstance(AEADCipher.ALG_AES_GCM, false);
    byte[] zero = new byte[16];
    byte[] first = new byte[32];
    byte[] second = new byte[32];
    byte[] third = new byte[32];
    gcm.init(key, Cipher.MODE_ENCRYPT);
    gcm.doFinal(zero, (short)0, (short)16, first, (short)0);
    gcm.retrieveTag(first, (short)16, (short)16);
    gcm.doFinal(zero, (short)0, (short)16, second, (short)0);
    gcm.retrieveTag(second, (short)16, (short)16);
    gcm.init(key, Cipher.MODE_ENCRYPT, new byte[12], (short)0, (short)12);
    gcm.doFinal(zero, (short)0, (short)16, third, (short)0);
    gcm.retrieveTag(third, (short)16, (short)16);
    return sameBytes(first, second, (short)32) && sameBytes(first, third, (short)32)
        ? (byte)1 : (byte)0;
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) {
      if (oneShotSelectProbe == 1) {
        try {
          MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
          MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
          oneShotSelectProbe = 2;
        } catch (SystemException e) {
          oneShotSelectProbe = 3;
        }
      }
      selectingAppletProbeFlags |= 2;
      lastSelectProcessWasReselect = reSelectingApplet() ? (byte)1 : (byte)0;
      return;
    }
    byte[] buffer = apdu.getBuffer();
    if (buffer[ISO7816.OFFSET_CLA] == 0) {
      switch (buffer[ISO7816.OFFSET_INS]) {
        case (byte)0xa4:
          ISOException.throwIt(ISO7816.SW_FILE_NOT_FOUND);
          return;
        case (byte)0xcb:
          // Finding #16: 00 CB is an ordinary command for the selected applet.
          buffer[0] = 0x43;
          apdu.setOutgoingAndSend((short)0, (short)1);
          return;
        case (byte)0xc0:
          buffer[0] = 0x42;
          apdu.setOutgoingAndSend((short)0, (short)1);
          return;
        default:
          break;
      }
    }
    byte command = buffer[ISO7816.OFFSET_P1];
    short length = 1;
    boolean manualResponse = false;
    switch (command) {
      case 1:
        JCSystem.beginTransaction();
        buffer[0] = JCSystem.getTransactionDepth();
        JCSystem.commitTransaction();
        buffer[1] = JCSystem.getTransactionDepth();
        length = 2;
        break;
      case 2:
        try { JCSystem.commitTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        break;
      case 3:
        try { JCSystem.abortTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        break;
      case 4:
        JCSystem.beginTransaction();
        try { JCSystem.beginTransaction(); buffer[0] = 0; }
        catch (TransactionException e) { buffer[0] = (byte)e.getReason(); }
        JCSystem.abortTransaction();
        buffer[1] = JCSystem.getTransactionDepth();
        length = 2;
        break;
      case 5:
        value = 0x11;
        JCSystem.beginTransaction();
        value = 0x22;
        JCSystem.abortTransaction();
        buffer[0] = value;
        break;
      case 6:
        JCSystem.beginTransaction();
        value = 0x33;
        JCSystem.commitTransaction();
        buffer[0] = value;
        break;
      case 7:
        buffer[0] = value;
        break;
      case 8:
        MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false)
            .doFinal(shaInput, (short)0, (short)3, shaDigest, (short)0);
        for (short i = 0; i < (short)shaDigest.length; i++) {
          buffer[i] = shaDigest[i];
        }
        length = (short)shaDigest.length;
        break;
      case 9: {
        MessageDigest digest =
            MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false);
        digest.update(shaInput, (short)0, (short)3);
        digest.doFinal(shaInput, (short)0, (short)0,
                       shaDigest, (short)0);
        for (short i = 0; i < (short)shaDigest.length; i++) {
          buffer[i] = shaDigest[i];
        }
        length = (short)shaDigest.length;
        break;
      }
      case 10: {
        short offset = 0;
        offset = appendMemory(buffer, offset, JCSystem.MEMORY_TYPE_PERSISTENT);
        offset = appendMemory(buffer, offset, JCSystem.MEMORY_TYPE_TRANSIENT_RESET);
        offset = appendMemory(buffer, offset, JCSystem.MEMORY_TYPE_TRANSIENT_DESELECT);
        short maxCommit = JCSystem.getMaxCommitCapacity();
        short unusedCommit = JCSystem.getUnusedCommitCapacity();
        buffer[offset++] = (byte)(maxCommit >> 8);
        buffer[offset++] = (byte)maxCommit;
        buffer[offset++] = (byte)(unusedCommit >> 8);
        buffer[offset++] = (byte)unusedCommit;
        JCSystem.beginTransaction();
        value++;
        short activeUnusedCommit = JCSystem.getUnusedCommitCapacity();
        JCSystem.abortTransaction();
        buffer[offset++] = (byte)(activeUnusedCommit >> 8);
        buffer[offset++] = (byte)activeUnusedCommit;
        length = offset;
        break;
      }
      case 11:
        try {
          // MD5 is not a MessageDigest algorithm in any profile.
          InitializedMessageDigest.OneShot.open(MessageDigest.ALG_MD5);
          buffer[0] = 0;
        } catch (CryptoException e) {
          buffer[0] = (byte)e.getReason();
        }
        break;
      case 12: {
        CryptoException previous = null;
        byte failure = 0;
        byte thrown = 0;
        byte returned = 0;
        for (short i = 0; i < (short)50; i++) {
          short reason = (short)((i % 5) + 1);
          try {
            CryptoException.throwIt(reason);
            returned++;
          } catch (CryptoException e) {
            thrown++;
            if (e.getReason() != reason) {
              failure = 1;
              buffer[1] = (byte)e.getReason();
              buffer[2] = (byte)reason;
              buffer[3] = (byte)i;
              break;
            }
            if (previous != null && previous != e) {
              failure = 2;
              buffer[1] = (byte)i;
              buffer[2] = thrown;
              buffer[3] = returned;
              break;
            }
            previous = e;
          }
        }
        buffer[0] = failure == 0 ? (byte)50 : failure;
        length = (short)(failure == 0 ? 1 : 4);
        break;
      }
      case 13: {
        byte[] correct = {(byte)'1', (byte)'2', (byte)'3', (byte)'4'};
        byte[] wrong = {(byte)'1', (byte)'2', (byte)'3', (byte)'5'};
        OwnerPINx adjustable = (OwnerPINx)OwnerPINBuilder.buildOwnerPIN(
            (byte)3, (byte)4, OwnerPINBuilder.OWNER_PIN_X);
        buffer[0] = adjustable.getTryLimit();
        buffer[1] = adjustable.getTriesRemaining();
        adjustable.update(correct, (short)0, (byte)correct.length);
        adjustable.setTryLimit((byte)2);
        buffer[2] = adjustable.getTriesRemaining();
        buffer[3] = (byte)(adjustable.check(wrong, (short)0,
                                             (byte)wrong.length) ? 1 : 0);
        buffer[4] = adjustable.getTriesRemaining();
        adjustable.setTriesRemaining((byte)0);
        buffer[5] = (byte)(adjustable.check(correct, (short)0,
                                             (byte)correct.length) ? 1 : 0);
        buffer[6] = adjustable.getTriesRemaining();

        OwnerPINxWithPredecrement predecrement =
            (OwnerPINxWithPredecrement)OwnerPINBuilder.buildOwnerPIN(
                (byte)3, (byte)4,
                OwnerPINBuilder.OWNER_PIN_X_WITH_PREDECREMENT);
        predecrement.update(correct, (short)0, (byte)correct.length);
        try {
          predecrement.check(correct, (short)0, (byte)correct.length);
          buffer[7] = 0;
        } catch (PINException e) {
          buffer[7] = (byte)e.getReason();
        }
        buffer[8] = predecrement.decrementTriesRemaining();
        buffer[9] = (byte)(predecrement.check(wrong, (short)0,
                                               (byte)wrong.length) ? 1 : 0);
        buffer[10] = predecrement.getTriesRemaining();
        predecrement.decrementTriesRemaining();
        buffer[11] = (byte)(predecrement.check(correct, (short)0,
                                                (byte)correct.length) ? 1 : 0);
        buffer[12] = predecrement.getTriesRemaining();
        buffer[13] = (byte)(predecrement.isValidated() ? 1 : 0);
        predecrement.reset();
        buffer[14] = predecrement.getTriesRemaining();
        buffer[15] = (byte)(predecrement.isValidated() ? 1 : 0);
        predecrement.decrementTriesRemaining();
        try {
          predecrement.check(null, (short)0, (byte)4);
          ISOException.throwIt((short)0x6F01);
        } catch (NullPointerException expected) {
          if (predecrement.getTriesRemaining() != (byte)2) {
            ISOException.throwIt((short)0x6F02);
          }
        }
        try {
          predecrement.check(correct, (short)0, (byte)4);
          ISOException.throwIt((short)0x6F03);
        } catch (PINException expected) {
          if (expected.getReason() != PINException.ILLEGAL_STATE) {
            ISOException.throwIt((short)0x6F04);
          }
        }
        length = 16;
        break;
      }
      case 14: {
        Object first = staticReferenceRoot;
        Object second = staticReferenceRoot;
        buffer[0] = (byte)(first == second ? 1 : 0);
        length = 1;
        break;
      }
      case 15: {
        Object first = persistentReferenceArray[0];
        Object second = persistentReferenceArray[1];
        buffer[0] = (byte)(first == second ? 1 : 0);
        length = 1;
        break;
      }
      case 16: {
        byte[] first = persistentReferenceHolder.first;
        byte[] second = persistentReferenceHolder.second;
        second[0] = 0x2A;
        buffer[0] = (byte)(first == second && first[0] == 0x2A ? 1 : 0);
        length = 1;
        break;
      }
      case 17: {
        Object candidate = persistentReferenceArray[0];
        byte[] cast = (byte[])candidate;
        buffer[0] = (byte)(cast.length == 1 ? 1 : 0);
        length = 1;
        break;
      }
      case 18: {
        Object candidate = persistentReferenceArray;
        ClassCastException previous = null;
        byte failure = 0;
        byte thrown = 0;
        for (short i = 0; i < (short)50; i++) {
          try {
            byte[] invalid = (byte[])candidate;
            failure = 1;
            break;
          } catch (ClassCastException e) {
            thrown++;
            if (previous != null && previous != e) {
              failure = 2;
              break;
            }
            previous = e;
          }
        }
        buffer[0] = (byte)(failure == 0 && thrown == (byte)50 ? 1 : 0);
        length = 1;
        break;
      }
      case 19: {
        byte[] nullArray = null;
        try {
          short ignored = (short)nullArray.length;
          buffer[0] = 0;
        } catch (NullPointerException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 20: {
        byte[] values = new byte[1];
        try {
          buffer[0] = values[1];
        } catch (ArrayIndexOutOfBoundsException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 21: {
        try {
          short ignored = (short)(1 / 0);
          buffer[0] = 0;
        } catch (ArithmeticException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 22: {
        try {
          byte[] invalid = new byte[-1];
          buffer[0] = 0;
        } catch (NegativeArraySizeException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 23: {
        Object[] values = new Exception[1];
        try {
          values[0] = this;
          buffer[0] = 0;
        } catch (ArrayStoreException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 24: {
        try {
          JCSystem.getAvailableMemory((byte)3);
          buffer[0] = 0;
        } catch (SystemException e) {
          buffer[0] = e.getReason() == SystemException.ILLEGAL_VALUE
              ? (byte)1 : (byte)2;
        }
        length = 1;
        break;
      }
      case 25: {
        Object value = new Object();
        buffer[0] = value == null ? (byte)0 : (byte)1;
        length = 1;
        break;
      }
      case 26: {
        lastApduExceptionReason = (byte)0x7f;
        try {
          apdu.setIncomingAndReceive();
          apdu.setIncomingAndReceive();
        } catch (APDUException e) {
          lastApduExceptionReason = (byte)e.getReason();
        }
        return;
      }
      case 27: {
        buffer[0] = lastApduExceptionReason;
        length = 1;
        break;
      }
      case 28: {
        byte[] resetBytes = JCSystem.makeTransientByteArray(
            (short)2, JCSystem.CLEAR_ON_RESET);
        boolean[] resetBooleans = JCSystem.makeTransientBooleanArray(
            (short)1, JCSystem.CLEAR_ON_RESET);
        short[] deselectShorts = JCSystem.makeTransientShortArray(
            (short)1, JCSystem.CLEAR_ON_DESELECT);
        Object[] deselectObjects = JCSystem.makeTransientObjectArray(
            (short)1, JCSystem.CLEAR_ON_DESELECT);
        resetBytes[0] = 0x5a;
        resetBooleans[0] = true;
        deselectShorts[0] = 0x1234;
        deselectObjects[0] = this;
        buffer[0] = resetBytes.length == 2 && resetBytes[0] == 0x5a &&
                    resetBooleans[0] &&
                    JCSystem.isTransient(resetBooleans) == JCSystem.CLEAR_ON_RESET &&
                    deselectShorts[0] == 0x1234 &&
                    JCSystem.isTransient(deselectShorts) == JCSystem.CLEAR_ON_DESELECT &&
                    JCSystem.isTransient(resetBytes) == JCSystem.CLEAR_ON_RESET &&
                    deselectObjects[0] == this &&
                    JCSystem.isTransient(deselectObjects) == JCSystem.CLEAR_ON_DESELECT
                        ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 29: {
        try {
          JCSystem.makeTransientByteArray((short)1, (byte)3);
          buffer[0] = 0;
        } catch (SystemException e) {
          buffer[0] = e.getReason() == SystemException.ILLEGAL_VALUE
              ? (byte)1 : (byte)2;
        }
        length = 1;
        break;
      }
      case 30: {
        transientResetArray = JCSystem.makeTransientByteArray(
            (short)1, JCSystem.CLEAR_ON_RESET);
        transientDeselectArray = JCSystem.makeTransientByteArray(
            (short)1, JCSystem.CLEAR_ON_DESELECT);
        transientResetArray[0] = 0x55;
        transientDeselectArray[0] = 0x66;
        buffer[0] = 1;
        length = 1;
        break;
      }
      case 79: {
        JCSystem.beginTransaction();
        JCSystem.abortTransaction();
        byte result = 0;
        if (transientResetArray != null && transientResetArray[0] == 0x55 &&
            JCSystem.isTransient(transientResetArray) ==
                JCSystem.CLEAR_ON_RESET) {
          result |= 1;
        }
        if (transientDeselectArray != null &&
            transientDeselectArray[0] == 0x66 &&
            JCSystem.isTransient(transientDeselectArray) ==
                JCSystem.CLEAR_ON_DESELECT) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 31: {
        byte result = 0;
        if (transientDeselectArray != null && transientDeselectArray[0] == 0) {
          result |= 1;
        }
        if (transientResetArray != null && transientResetArray[0] == 0x55) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 32: {
        transientResetArray[0] = 0x2a;
        transientDeselectArray[0] = 0x3b;
        buffer[0] = 1;
        length = 1;
        break;
      }
      case 33: {
        byte result = 0;
        if (transientDeselectArray != null && transientDeselectArray[0] == 0) {
          result |= 1;
        }
        if (transientResetArray != null && transientResetArray[0] == 0) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 34: {
        deletionOrphan = new byte[8];
        deletionOrphan[0] = 0x5a;
        deletionOrphan = null;
        try {
          JCSystem.requestObjectDeletion();
          buffer[0] = 0;
        } catch (SystemException e) {
          buffer[0] = e.getReason() == SystemException.ILLEGAL_USE
              ? (byte)1 : (byte)0;
        }
        length = 1;
        break;
      }
      case 35: {
        byte[] otherAidBytes = {0x01, 0x02, 0x03, 0x04, 0x05};
        AID otherAid = new AID(otherAidBytes, (short)0, (byte)otherAidBytes.length);
        buffer[0] = JCSystem.isAppletActive(JCSystem.getAID()) &&
                !JCSystem.isAppletActive(otherAid) ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 36: {
        byte[] global = (byte[])JCSystem.makeGlobalArray(
            JCSystem.ARRAY_TYPE_BYTE, (short)2);
        global[0] = 0x5a;
        buffer[0] = global[0] == 0x5a &&
                JCSystem.isTransient(global) == JCSystem.MEMORY_TYPE_TRANSIENT_RESET
            ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 37: {
        byte result = 0;
        try {
          JCSystem.makeGlobalArray((byte)0, (short)1);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_VALUE) result |= 1;
        }
        try {
          JCSystem.makeGlobalArray(JCSystem.ARRAY_TYPE_BYTE, (short)-1);
        } catch (NegativeArraySizeException e) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 38: {
        byte[] global = (byte[])JCSystem.makeGlobalArray(
            JCSystem.ARRAY_TYPE_BYTE, (short)1);
        try {
          globalArrayInstanceReference = global;
          buffer[0] = 0;
        } catch (java.lang.SecurityException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 39: {
        byte[] global = (byte[])JCSystem.makeGlobalArray(
            JCSystem.ARRAY_TYPE_BYTE, (short)1);
        try {
          staticReferenceRoot = global;
          buffer[0] = 0;
        } catch (java.lang.SecurityException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 40: {
        byte[] global = (byte[])JCSystem.makeGlobalArray(
            JCSystem.ARRAY_TYPE_BYTE, (short)1);
        Object[] local = new Object[1];
        try {
          local[0] = global;
          buffer[0] = 0;
        } catch (java.lang.SecurityException e) {
          buffer[0] = 1;
        }
        length = 1;
        break;
      }
      case 41: {
        Object[] global = (Object[])JCSystem.makeGlobalArray(
            JCSystem.ARRAY_TYPE_OBJECT, (short)1);
        global[0] = null;
        if (global[0] != null) {
          buffer[0] = 0;
          length = 1;
          break;
        }
        try {
          buffer[0] = 2;
          global[0] = this;
          buffer[0] = 3;
          buffer[0] = global[0] == this ? (byte)0 : (byte)4;
        } catch (java.lang.SecurityException e) {
          buffer[0] = buffer[0] == 2 ? (byte)1 : (byte)5;
        }
        length = 1;
        break;
      }
      case 42: {
        buffer[0] = SensitiveArrays.isIntegritySensitive(new byte[1])
            ? (byte)0 : (byte)1;
        length = 1;
        break;
      }
      case 43: {
        byte result = 0;
        try {
          SensitiveArrays.assertIntegrity(new byte[1]);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_VALUE) result |= 1;
        }
        try {
          SensitiveArrays.assertIntegrity(null);
        } catch (NullPointerException e) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 44: {
        byte result = 0;
        try {
          SensitiveArrays.makeIntegritySensitiveArray(
              JCSystem.ARRAY_TYPE_BYTE, JCSystem.MEMORY_TYPE_TRANSIENT_RESET,
              (short)1);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_USE) result |= 1;
        }
        try {
          SensitiveArrays.makeIntegritySensitiveArray((byte)0,
              JCSystem.MEMORY_TYPE_PERSISTENT, (short)1);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_VALUE) result |= 2;
        }
        try {
          SensitiveArrays.makeIntegritySensitiveArray(JCSystem.ARRAY_TYPE_BYTE,
              (byte)3, (short)1);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_VALUE) result |= 4;
        }
        try {
          SensitiveArrays.makeIntegritySensitiveArray(JCSystem.ARRAY_TYPE_BYTE,
              JCSystem.MEMORY_TYPE_PERSISTENT, (short)-1);
        } catch (NegativeArraySizeException e) {
          result |= 8;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 45: {
        byte result = 0;
        try {
          SensitiveArrays.clearArray(new byte[1]);
        } catch (SystemException e) {
          if (e.getReason() == SystemException.ILLEGAL_VALUE) result |= 1;
        }
        try {
          SensitiveArrays.clearArray(null);
        } catch (NullPointerException e) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 46: {
        buffer[0] = SensitiveArrays.isIntegritySensitiveArraysSupported()
            ? (byte)0 : (byte)1;
        length = 1;
        break;
      }
      case 47: {
        byte result = SensitiveArrays.isIntegritySensitive(new Object())
            ? (byte)0 : (byte)1;
        try {
          SensitiveArrays.isIntegritySensitive(null);
        } catch (NullPointerException e) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 48: {
        byte result = 0;
        if (JCSystem.getAppletShareableInterfaceObject(null, (byte)0) == null) {
          result |= 1;
        }
        byte[] unknownAidBytes = {0x01, 0x02, 0x03, 0x04, 0x05};
        AID unknownAid = new AID(unknownAidBytes, (short)0,
                                 (byte)unknownAidBytes.length);
        if (JCSystem.getAppletShareableInterfaceObject(unknownAid, (byte)0) == null &&
            JCSystem.getAppletShareableInterfaceObject(JCSystem.getAID(), (byte)0) == null) {
          result |= 2;
        }
        buffer[0] = result;
        length = 1;
        break;
      }
      case 49:
      case 50:
      case 51: {
        byte[] discarded = null;
        for (short i = 0; i < 256; i++) discarded = new byte[16];
        buffer[0] = discarded.length == 16 ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 93: {
        length = 0;
        byte[] retained = null;
        for (short i = 0; i < 256; i++) {
          byte[] global = (byte[])JCSystem.makeGlobalArray(
              JCSystem.ARRAY_TYPE_BYTE, (short)1);
          global[0] = (byte)i;
          if (i == 0) {
            retained = global;
            retained[0] = 0x5a;
          } else if (retained[0] != 0x5a) {
            buffer[0] = 0;
            length = 1;
            break;
          }
        }
        if (length == 0) {
          byte[] distinct = (byte[])JCSystem.makeGlobalArray(
              JCSystem.ARRAY_TYPE_BYTE, (short)1);
          distinct[0] = 0x33;
          buffer[0] = retained != distinct && retained[0] == 0x5a
              ? (byte)1 : (byte)0;
          length = 1;
        }
        break;
      }
      case 52:
      case 53:
      case 54: {
        Object discarded = null;
        for (short i = 0; i < 256; i++) discarded = new Object();
        buffer[0] = discarded != null ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 55: {
        buffer[0] = selectCount;
        buffer[1] = deselectCount;
        buffer[2] = lastSelectWasReselect;
        buffer[3] = lastSelectProcessWasReselect;
        length = 4;
        break;
      }
      case 58: {
        buffer[0] = value;
        length = 1;
        break;
      }
      case 59: {
        buffer[0] = APDU.getCurrentAPDU() == apdu ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 60: {
        buffer[0] = currentApduHiddenOutsideProcess;
        length = 1;
        break;
      }
      case 61: {
        buffer[0] = APDU.getCurrentAPDUBuffer() == apdu.getBuffer()
            ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 62: {
        buffer[0] = currentApduBufferHiddenOutsideProcess;
        length = 1;
        break;
      }
      case 63: {
        apdu.setOutgoingNoChaining();
        try {
          APDU.waitExtension();
          buffer[0] = 0;
        } catch (APDUException expected) {
          buffer[0] = (byte)expected.getReason();
        }
        apdu.setOutgoingLength((short)1);
        apdu.sendBytes((short)0, (short)1);
        manualResponse = true;
        break;
      }
      case 64: {
        apdu.setOutgoing();
        try {
          APDU.waitExtension();
          buffer[0] = 1;
        } catch (APDUException unexpected) {
          buffer[0] = (byte)unexpected.getReason();
        }
        apdu.setOutgoingLength((short)1);
        apdu.sendBytes((short)0, (short)1);
        manualResponse = true;
        break;
      }
      case 65: {
        AID currentAID = JCSystem.getAID();
        byte[] currentAIDBytes = new byte[16];
        byte currentAIDLength = currentAID.getBytes(currentAIDBytes, (byte)0);
        AID foundAID = JCSystem.lookupAID(currentAIDBytes, (short)0,
                                           currentAIDLength);
        byte[] unknownAIDBytes = {0x01, 0x02, 0x03, 0x04, 0x05};
        AID unknownAID = JCSystem.lookupAID(unknownAIDBytes, (short)0,
                                            (byte)unknownAIDBytes.length);
        buffer[0] = foundAID == currentAID && unknownAID == null
            ? (byte)1 : (byte)0;
        break;
      }
      case 66: {
        byte result = 0;
        try {
          JCSystem.lookupAID(null, (short)0, (byte)5);
        } catch (NullPointerException expected) {
          result |= 1;
        }
        try {
          JCSystem.lookupAID(buffer, (short)-1, (byte)5);
        } catch (ArrayIndexOutOfBoundsException expected) {
          result |= 2;
        }
        try {
          JCSystem.lookupAID(buffer, (short)0, (byte)-1);
        } catch (ArrayIndexOutOfBoundsException expected) {
          result |= 4;
        }
        try {
          JCSystem.lookupAID(buffer, (short)(buffer.length - 2), (byte)5);
        } catch (ArrayIndexOutOfBoundsException expected) {
          result |= 8;
        }
        buffer[0] = result;
        break;
      }
      case 75: {
        AID currentAID = JCSystem.getAID();
        byte[] currentAIDBytes = new byte[16];
        byte currentAIDLength = currentAID.getBytes(currentAIDBytes, (short)0);
        boolean equalBytes = currentAID.equals(
            currentAIDBytes, (short)0, currentAIDLength);
        boolean equalNull = currentAID.equals(
            (byte[])null, (short)0, currentAIDLength);
        boolean partialNull = currentAID.partialEquals(
            null, (short)0, (byte)5);
        boolean partialPrefix = currentAID.partialEquals(
            currentAIDBytes, (short)0, (byte)5);
        buffer[0] = equalBytes && !equalNull && !partialNull && partialPrefix
            ? (byte)1 : (byte)0;
        break;
      }
      case 76:
        buffer[0] = installSawAIDNullBeforeRegister == 1 &&
                installSawAppletActive == 1 &&
                JCSystem.isAppletActive(JCSystem.getAID())
            ? (byte)1 : (byte)0;
        break;
      case 77:
        buffer[0] = installSawAppletActive == 1 &&
                selectSawAppletActive == 1 && deselectSawAppletActive == 1 &&
                JCSystem.isAppletActive(JCSystem.getAID())
            ? (byte)0x0f : (byte)0;
        break;
      case 78: {
        byte result = 0;
        try {
          UserException.throwIt((short)0x1234);
          result = 1;
        } catch (Exception expected) {
          if (expected instanceof UserException) {
            UserException userException = (UserException)expected;
            result = userException.getReason() == (short)0x1234
                ? (byte)1 : (byte)2;
          } else {
            result = 3;
          }
        }
        buffer[0] = result;
        break;
      }
      case 80: {
        byte[] alias = gcRehydrateAliasA;
        buffer[0] = alias != null && alias.length == 1 && alias[0] == 0x5a
            ? (byte)1 : (byte)0;
        gcRehydrateAliasA = null;
        length = 1;
        break;
      }
      case 81: {
        byte[] restored = gcRehydrateAliasB;
        buffer[0] = restored != null && restored.length == 1 &&
                    restored[0] == 0x5a ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 82: {
        Object[] children = persistentNestedReferenceHolder.children;
        byte[] firstLeaf = (byte[])children[0];
        byte[] secondLeaf = (byte[])children[1];
        boolean graphValid = children.length == 2 && firstLeaf == secondLeaf &&
            firstLeaf.length == 1 && firstLeaf[0] == 0x6b;
        firstLeaf[0] = 0x6c;
        boolean aliasMutationVisible = secondLeaf[0] == 0x6c;
        firstLeaf[0] = 0x6b;
        buffer[0] = graphValid && aliasMutationVisible ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 83: {
        byte reason = 0;
        JCSystem.beginTransaction();
        try {
          transactionCapacityArray[0] = 0x5a;
        } catch (TransactionException expected) {
          reason = (byte)expected.getReason();
        }
        JCSystem.abortTransaction();
        buffer[0] = reason;
        buffer[1] = transactionCapacityArray[0];
        length = 2;
        break;
      }
      case 85: {
        JCSystem.beginTransaction();
        value = 0x22;
        buffer[0] = 1;
        break;
      }
      case 86: {
        JCSystem.beginTransaction();
        value = 0x33;
        TransactionException.throwIt(TransactionException.IN_PROGRESS);
        break;
      }
      case 87: {
        beginTransactionOnNextDeselect = 1;
        buffer[0] = 1;
        length = 1;
        break;
      }
      case 88: {
        beginTransactionOnNextSelect = 1;
        rejectNextSelect = 1;
        buffer[0] = 1;
        length = 1;
        break;
      }
      case 89: {
        byte[] source = {0x5a};
        JCSystem.beginTransaction();
        short copied = Util.arrayCopyNonAtomic(
            source, (short)0, transactionCapacityArray, (short)0,
            (short)1);
        short unused = JCSystem.getUnusedCommitCapacity();
        JCSystem.abortTransaction();
        buffer[0] = copied == 1 && unused == JCSystem.getMaxCommitCapacity()
            ? (byte)1 : (byte)0;
        buffer[1] = transactionCapacityArray[0];
        length = 2;
        break;
      }
      case 94: {
        buffer[0] = selectDeselectTransientResult;
        length = 1;
        break;
      }
      case 95: {
        byte[] retained = new byte[1];
        retained[0] = 0x5a;
        JCSystem.beginTransaction();
        for (short i = 0; i < 130; i++) {
          byte[] discarded = new byte[1];
          discarded[0] = (byte)i;
          persistentReferenceArray[0] = discarded;
        }
        JCSystem.abortTransaction();
        boolean bodyRecovered = retained.length == 1 &&
            retained[0] == 0x5a;
        persistentReferenceArray[0] = retained;
        buffer[0] = bodyRecovered ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 96: {
        byte[] retained = (byte[])persistentReferenceArray[0];
        byte[] next = new byte[1];
        next[0] = 0x33;
        persistentReferenceArray[1] = next;
        buffer[0] = retained != next && retained.length == 1 &&
            retained[0] == 0x5a && next[0] == 0x33
                ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 97: {
        byte[] low = {0x11};
        byte[] high = {0x22};
        persistentReferenceArray[0] = low;
        persistentReferenceArray[256] = high;
        buffer[0] = persistentReferenceArray[0] != persistentReferenceArray[256] &&
            ((byte[])persistentReferenceArray[0])[0] == 0x11 &&
            ((byte[])persistentReferenceArray[256])[0] == 0x22
                ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 98: {
        byte[] low = (byte[])persistentReferenceArray[0];
        byte[] high = (byte[])persistentReferenceArray[256];
        buffer[0] = low != high && low[0] == 0x11 && high[0] == 0x22
                ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 99: {
        byte checksum = 0;
        for (short i = 0; i < 128; i++) {
          checksum ^= transactionCapacityArray[1];
        }
        for (short i = 0; i < 64; i++) {
          transactionCapacityArray[1]++;
        }
        buffer[0] = checksum;
        buffer[1] = transactionCapacityArray[1];
        length = 2;
        break;
      }
      case 101: {
        short checksum = 0;
        for (short i = 0; i < 1024; i++) {
          checksum ^= transactionCapacityArray.length;
        }
        buffer[0] = (byte)(checksum >> 8);
        buffer[1] = (byte)checksum;
        length = 2;
        break;
      }
      case 90: {
        transactionSmallArray[0] = 0x11;
        transactionSmallArray[1] = 0x12;
        JCSystem.beginTransaction();
        transactionSmallArray[0] = 0x32;
        byte[] source = {0x43};
        Util.arrayCopyNonAtomic(source, (short)0, transactionSmallArray,
                                (short)0, (short)1);
        JCSystem.abortTransaction();
        buffer[0] = transactionSmallArray[0];
        buffer[1] = transactionSmallArray[1];
        length = 2;
        break;
      }
      case 91: {
        transactionSmallArray[0] = 0x11;
        transactionOtherArray[0] = 0x21;
        JCSystem.beginTransaction();
        transactionSmallArray[0] = 0x32;
        byte[] source = {0x54};
        Util.arrayCopyNonAtomic(source, (short)0, transactionOtherArray,
                                (short)0, (short)1);
        JCSystem.abortTransaction();
        buffer[0] = transactionSmallArray[0];
        buffer[1] = transactionOtherArray[0];
        length = 2;
        break;
      }
      case 92: {
        transactionSmallArray[0] = 0x11;
        transactionSmallArray[1] = 0x12;
        JCSystem.beginTransaction();
        byte[] source = {0x63};
        Util.arrayCopyNonAtomic(source, (short)0, transactionSmallArray,
                                (short)0, (short)1);
        transactionSmallArray[1] = 0x74;
        JCSystem.abortTransaction();
        buffer[0] = transactionSmallArray[0];
        buffer[1] = transactionSmallArray[1];
        length = 2;
        break;
      }
      case 84: {
        short arrayLength = (short)transactionCapacityArray.length;
        buffer[0] = (byte)(arrayLength >> 8);
        buffer[1] = (byte)arrayLength;
        length = 2;
        break;
      }
      case 67: {
        byte reason = 0;
        try {
          register();
        } catch (SystemException expected) {
          reason = (byte)expected.getReason();
        }
        buffer[0] = reason;
        break;
      }
      case 68: {
        byte reason = 0;
        byte[] requestedAID = {
            0x4a, 0x43, 0x41, 0x6c, 0x67,
            0x54, 0x65, 0x73, 0x74, 0x31};
        try {
          register(requestedAID, (short)0, (byte)requestedAID.length);
        } catch (SystemException expected) {
          reason = (byte)expected.getReason();
        }
        buffer[0] = reason;
        break;
      }
      case 69: {
        byte reason = 0;
        byte[] invalidAID = {0x01, 0x02, 0x03, 0x04};
        try {
          register(invalidAID, (short)0, (byte)invalidAID.length);
        } catch (SystemException expected) {
          reason = (byte)expected.getReason();
        }
        buffer[0] = reason;
        break;
      }
      case 70: {
        buffer[0] = explicitRegistrationFailureReason;
        break;
      }
      case 71: {
        buffer[0] = selectingAppletProbeFlags;
        break;
      }
      case 72: {
        selectingAppletProbeFlags = 0;
        buffer[0] = 0;
        break;
      }
      case 102: {
        // OwnerPIN state stays out of transactions (FLASH-03).
        byte[] correct = {(byte)'1', (byte)'2', (byte)'3', (byte)'4'};
        byte[] wrong = {(byte)'1', (byte)'2', (byte)'3', (byte)'5'};
        if (persistentPin == null) {
          persistentPin = new OwnerPIN((byte)3, (byte)4);
        }
        persistentPin.update(correct, (short)0, (byte)4);
        JCSystem.beginTransaction();
        persistentPin.check(wrong, (short)0, (byte)4);
        JCSystem.abortTransaction();
        buffer[0] = persistentPin.getTriesRemaining();
        JCSystem.beginTransaction();
        persistentPin.check(correct, (short)0, (byte)4);
        JCSystem.abortTransaction();
        buffer[1] = persistentPin.getTriesRemaining();
        buffer[2] = (byte)(persistentPin.isValidated() ? 1 : 0);
        persistentPin.check(wrong, (short)0, (byte)4);
        JCSystem.beginTransaction();
        persistentPin.resetAndUnblock();
        JCSystem.abortTransaction();
        buffer[3] = persistentPin.getTriesRemaining();
        buffer[4] = (byte)(persistentPin.check(correct, (short)0, (byte)4)
                               ? 1 : 0);
        buffer[5] = (byte)(persistentPin.isValidated() ? 1 : 0);
        // Leave the PIN validated for case 103.
        persistentPin.check(wrong, (short)0, (byte)4);
        persistentPin.check(correct, (short)0, (byte)4);
        buffer[6] = persistentPin.getTriesRemaining();
        length = 7;
        break;
      }
      case 104: {
        // Persistent OwnerPINx: presentation (decrementTriesRemaining)
        // stays out of transactions, administration (setTriesRemaining)
        // participates in them.
        byte[] correct = {(byte)'1', (byte)'2', (byte)'3', (byte)'4'};
        byte[] wrong = {(byte)'1', (byte)'2', (byte)'3', (byte)'5'};
        if (persistentPinX == null) {
          persistentPinX = (OwnerPINxWithPredecrement)
              OwnerPINBuilder.buildOwnerPIN((byte)3, (byte)4,
                  OwnerPINBuilder.OWNER_PIN_X_WITH_PREDECREMENT);
        }
        persistentPinX.update(correct, (short)0, (byte)4);
        JCSystem.beginTransaction();
        buffer[0] = persistentPinX.decrementTriesRemaining();
        JCSystem.abortTransaction();
        buffer[1] = persistentPinX.getTriesRemaining();
        buffer[2] = (byte)(persistentPinX.check(wrong, (short)0, (byte)4)
                               ? 1 : 0);
        buffer[3] = persistentPinX.getTriesRemaining();
        JCSystem.beginTransaction();
        persistentPinX.setTriesRemaining((byte)1);
        JCSystem.abortTransaction();
        buffer[4] = persistentPinX.getTriesRemaining();
        persistentPinX.setTriesRemaining((byte)3);
        buffer[5] = persistentPinX.getTriesRemaining();
        length = 6;
        break;
      }
      case 105: {
        // Leave one failed try recorded for a cold-restart check (case 103
        // then reports 00 02).
        byte[] correct = {(byte)'1', (byte)'2', (byte)'3', (byte)'4'};
        byte[] wrong = {(byte)'1', (byte)'2', (byte)'3', (byte)'5'};
        if (persistentPin == null) {
          persistentPin = new OwnerPIN((byte)3, (byte)4);
        }
        persistentPin.update(correct, (short)0, (byte)4);
        persistentPin.check(wrong, (short)0, (byte)4);
        buffer[0] = (byte)(persistentPin.isValidated() ? 1 : 0);
        buffer[1] = persistentPin.getTriesRemaining();
        length = 2;
        break;
      }
      case 106:
      case 103: {
        // After a card reset: not validated, try counter kept.
        if (persistentPin == null) {
          buffer[0] = (byte)0x7f;
          buffer[1] = (byte)0x7f;
        } else {
          buffer[0] = (byte)(persistentPin.isValidated() ? 1 : 0);
          buffer[1] = persistentPin.getTriesRemaining();
        }
        length = 2;
        break;
      }
      case 112: {
        // Cipher.OneShot gives the same result as Cipher.getInstance().
        if (oneShotKey == null) {
          oneShotKey = (AESKey) KeyBuilder.buildKey(KeyBuilder.TYPE_AES,
              KeyBuilder.LENGTH_AES_128, false);
          Util.arrayFillNonAtomic(buffer, (short) 48, (short) 16, (byte) 0x5A);
          oneShotKey.setKey(buffer, (short) 48);
          oneShotReference = Cipher.getInstance(
              Cipher.ALG_AES_BLOCK_128_CBC_NOPAD, false);
        }
        Util.arrayFillNonAtomic(buffer, (short) 48, (short) 16, (byte) 0x11);
        Cipher.OneShot oneShot = null;
        try {
          oneShot = Cipher.OneShot.open(Cipher.CIPHER_AES_CBC, Cipher.PAD_NOPAD);
          oneShot.init(oneShotKey, Cipher.MODE_ENCRYPT);
          oneShot.doFinal(buffer, (short) 48, (short) 16, buffer, (short) 16);
          buffer[0] = oneShot.getAlgorithm();
          buffer[1] = oneShot.getCipherAlgorithm();
          buffer[2] = oneShot.getPaddingAlgorithm();
        } finally {
          if (oneShot != null) {
            oneShot.close();
            oneShot = null;
          }
        }
        oneShotReference.init(oneShotKey, Cipher.MODE_ENCRYPT);
        oneShotReference.doFinal(buffer, (short) 48, (short) 16, buffer,
                                 (short) 32);
        buffer[3] = Util.arrayCompare(buffer, (short) 16, buffer, (short) 32,
                                      (short) 16) == 0 ? (byte) 1 : (byte) 0;
        length = 4;
        break;
      }
      case 113: {
        // MessageDigest.OneShot lifecycle: released instances, update(),
        // unsupported algorithms.
        MessageDigest.OneShot digest =
            MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
        digest.close();
        digest.close();
        byte released = 0;
        try {
          digest.getAlgorithm();
        } catch (CryptoException e) {
          released = (byte) e.getReason();
        }
        digest = MessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
        byte update = 0;
        try {
          digest.update(buffer, (short) 0, (short) 1);
        } catch (CryptoException e) {
          update = (byte) e.getReason();
        }
        buffer[64] = 0x61;
        buffer[65] = 0x62;
        buffer[66] = 0x63;
        short digestLength =
            digest.doFinal(buffer, (short) 64, (short) 3, buffer, (short) 8);
        digest.close();
        digest = null;
        byte unsupported = 0;
        try {
          MessageDigest.OneShot.open((byte) 99);
        } catch (CryptoException e) {
          unsupported = (byte) e.getReason();
        }
        buffer[0] = released;
        buffer[1] = update;
        buffer[2] = (byte) digestLength;
        buffer[3] = buffer[8];
        buffer[4] = unsupported;
        length = 5;
        break;
      }
      case 114: {
        // A OneShot reference cannot be stored in a field, array or static.
        if (oneShotArray == null) oneShotArray = new Object[1];
        RandomData.OneShot random =
            RandomData.OneShot.open(RandomData.ALG_SECURE_RANDOM);
        byte field = 0;
        byte array = 0;
        byte statics = 0;
        try {
          oneShotField = random;
        } catch (SecurityException e) {
          field = 1;
        }
        try {
          oneShotArray[0] = random;
        } catch (SecurityException e) {
          array = 1;
        }
        try {
          staticOneShot = random;
        } catch (SecurityException e) {
          statics = 1;
        }
        short next = random.nextBytes(buffer, (short) 8, (short) 4);
        random.close();
        random = null;
        buffer[0] = field;
        buffer[1] = array;
        buffer[2] = statics;
        buffer[3] = (byte) next;
        length = 4;
        break;
      }
      case 115: {
        // Two Signature.OneShot instances may be open; a third is refused.
        // Neither is closed: returning from process() releases them, so the
        // next APDU can open two again.
        Signature.OneShot first = Signature.OneShot.open(
            MessageDigest.ALG_SHA_256, Signature.SIG_CIPHER_HMAC,
            Cipher.PAD_NULL);
        Signature.OneShot second = Signature.OneShot.open(
            MessageDigest.ALG_SHA_256, Signature.SIG_CIPHER_HMAC,
            Cipher.PAD_NULL);
        buffer[0] = (first != null && second != null && first != second)
            ? (byte) 1 : (byte) 0;
        buffer[1] = 0;
        try {
          Signature.OneShot.open(MessageDigest.ALG_SHA_256,
                                 Signature.SIG_CIPHER_HMAC, Cipher.PAD_NULL);
        } catch (SystemException e) {
          buffer[1] = (byte) e.getReason();
        }
        length = 2;
        break;
      }
      case 119: {
        // InitializedMessageDigest continues SHA-256 from an intermediate state:
        // state(64 x 'a') + "abc" must equal SHA-256(64 x 'a' || "abc").
        if (initializedDigest == null) {
          initializedDigest = MessageDigest.getInitializedMessageDigestInstance(
              MessageDigest.ALG_SHA_256, false);
        }
        Util.arrayCopyNonAtomic(SHA256_STATE_AFTER_64A, (short) 0, buffer,
                                (short) 64, (short) 32);
        buffer[100] = 0x40;
        initializedDigest.setInitialDigest(buffer, (short) 64, (short) 32,
                                           buffer, (short) 100, (short) 1);
        buffer[104] = 0x61;
        buffer[105] = 0x62;
        buffer[106] = 0x63;
        length = initializedDigest.doFinal(buffer, (short) 104, (short) 3,
                                           buffer, (short) 0);
        break;
      }
      case 120: {
        // InitializedMessageDigest.OneShot: update() is ILLEGAL_USE, a digested
        // length that is not a block multiple is ILLEGAL_VALUE.
        InitializedMessageDigest.OneShot digest =
            InitializedMessageDigest.OneShot.open(MessageDigest.ALG_SHA_256);
        byte update = 0;
        byte value = 0;
        try {
          try {
            digest.update(buffer, (short) 0, (short) 1);
          } catch (CryptoException e) {
            update = (byte) e.getReason();
          }
          Util.arrayCopyNonAtomic(SHA256_STATE_AFTER_64A, (short) 0, buffer,
                                  (short) 64, (short) 32);
          buffer[100] = 0x3F;
          try {
            digest.setInitialDigest(buffer, (short) 64, (short) 32, buffer,
                                    (short) 100, (short) 1);
          } catch (CryptoException e) {
            value = (byte) e.getReason();
          }
          buffer[100] = 0x40;
          digest.setInitialDigest(buffer, (short) 64, (short) 32, buffer,
                                  (short) 100, (short) 1);
          buffer[104] = 0x61;
          buffer[105] = 0x62;
          buffer[106] = 0x63;
          digest.doFinal(buffer, (short) 104, (short) 3, buffer, (short) 8);
        } finally {
          digest.close();
          digest = null;
        }
        buffer[0] = update;
        buffer[1] = value;
        Util.arrayCopyNonAtomic(buffer, (short) 8, buffer, (short) 2, (short) 4);
        length = 6;
        break;
      }
      case 121: {
        if (streamingInput == null) {
          streamingInput = new byte[5122];
          streamingInput[1] = 0x61;
          streamingInput[5120] = 0x62;
        }
        if (streamingDigest == null) {
          streamingDigest = MessageDigest.getInstance(
              MessageDigest.ALG_SHA_256, false);
        }
        streamingDigest.reset();
        streamingDigest.update(streamingInput, (short) 1, (short) 5120);
        buffer[0] = 1;
        break;
      }
      case 122: {
        for (byte i = 0; i < 6; i++) {
          streamingDigest.update(streamingInput, (short) 1, (short) 5120);
        }
        JCSystem.beginTransaction();
        streamingDigest.update(streamingInput, (short) 1, (short) 63);
        JCSystem.abortTransaction();
        length = streamingDigest.doFinal(buffer, (short) 0, (short) 0,
                                          buffer, (short) 0);
        break;
      }
      case 123: {
        length = streamingDigest.doFinal(buffer, (short) 0, (short) 0,
                                          buffer, (short) 0);
        break;
      }
      case 124: {
        byte[] algorithms = {MessageDigest.ALG_SHA, MessageDigest.ALG_SHA_224,
                             MessageDigest.ALG_SHA_256, MessageDigest.ALG_SHA_384,
                             MessageDigest.ALG_SHA_512};
        length = 0;
        for (byte i = 0; i < (byte) algorithms.length; i++) {
          MessageDigest digest = MessageDigest.getInstance(algorithms[i], false);
          length += digest.doFinal(streamingInput, (short) 1, (short) 5120,
                                   buffer, length);
        }
        break;
      }
      case (byte)132: {
        arrayLogicIntArray = new int[2];
        int[] destination = arrayLogicIntArray;
        int[] pattern = {0x12345678, 0x01234567, 0xfedcba98};
        short[] packed = {(short)0x89ab, (short)0xcdef};
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)0);
        JCSystem.beginTransaction();
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)1);
        short capacity = JCSystem.getUnusedCommitCapacity();
        short filled = ArrayLogic.arrayFillGenericNonAtomic(
            destination, (short)0, (short)1, pattern, (short)2);
        short copied = ArrayLogic.arrayCopyRepackNonAtomic(
            packed, (short)0, (short)2, destination, (short)1);
        boolean unchangedCapacity = JCSystem.getUnusedCommitCapacity() == capacity;
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)0);
        JCSystem.abortTransaction();
        buffer[0] = (byte)(filled == 1 && copied == 2 && unchangedCapacity &&
                          destination[0] == 0xfedcba98 &&
                          destination[1] == 0x89abcdef ? 1 : 0);
        break;
      }
      case (byte)131: {
        buffer[0] = transactionCapacityArray[0];
        buffer[1] = transactionCapacityArray[1];
        length = 2;
        break;
      }
      case (byte)130: {
        byte[] pattern = JCSystem.makeTransientByteArray((short)2, JCSystem.CLEAR_ON_RESET);
        pattern[0] = (byte)(transactionCapacityArray[0] ^ 0xff);
        pattern[1] = (byte)(transactionCapacityArray[1] ^ 0xff);
        JCSystem.beginTransaction();
        short capacity = JCSystem.getUnusedCommitCapacity();
        short filled = ArrayLogic.arrayFillGenericNonAtomic(
            transactionCapacityArray, (short)0, (short)1, pattern, (short)0);
        short copied = ArrayLogic.arrayCopyRepackNonAtomic(
            pattern, (short)1, (short)1, transactionCapacityArray, (short)1);
        boolean unchangedCapacity = JCSystem.getUnusedCommitCapacity() == capacity;
        JCSystem.abortTransaction();
        buffer[0] = (byte)(filled == 1 && copied == 2 && unchangedCapacity &&
                          transactionCapacityArray[0] == pattern[0] &&
                          transactionCapacityArray[1] == pattern[1] ? 1 : 0);
        break;
      }
      case (byte)129: {
        byte before = transactionCapacityArray[0];
        byte[] pattern = JCSystem.makeTransientByteArray((short)1, JCSystem.CLEAR_ON_RESET);
        pattern[0] = (byte)(before ^ 0xff);
        boolean rejectedFill = false;
        try {
          ArrayLogic.arrayFillGeneric(transactionCapacityArray, (short)0,
                                      (short)1, pattern, (short)0);
        } catch (TransactionException expected) {
          rejectedFill = expected.getReason() == TransactionException.BUFFER_FULL;
        }
        boolean standaloneUnchanged = transactionCapacityArray[0] == before;
        JCSystem.beginTransaction();
        short capacity = JCSystem.getUnusedCommitCapacity();
        boolean rejectedCopy = false;
        try {
          ArrayLogic.arrayCopyRepack(pattern, (short)0, (short)1,
                                      transactionCapacityArray, (short)0);
        } catch (TransactionException expected) {
          rejectedCopy = expected.getReason() == TransactionException.BUFFER_FULL;
        }
        boolean transactionUnchanged = transactionCapacityArray[0] == before &&
            JCSystem.getUnusedCommitCapacity() == capacity &&
            JCSystem.getTransactionDepth() == 1;
        JCSystem.abortTransaction();
        buffer[0] = (byte)(rejectedFill && standaloneUnchanged && rejectedCopy &&
                          transactionUnchanged && transactionCapacityArray[0] == before
                          ? 1 : 0);
        break;
      }
      case (byte)128: {
        arrayLogicShortArray = new short[2];
        short[] destination = arrayLogicShortArray;
        short[] pattern = JCSystem.makeTransientShortArray((short)2, JCSystem.CLEAR_ON_RESET);
        byte[] packed = JCSystem.makeTransientByteArray((short)2, JCSystem.CLEAR_ON_RESET);
        pattern[0] = (short)0x1234;
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)0);
        JCSystem.beginTransaction();
        pattern[0] = (short)0x5678;
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)0);
        short capacity = JCSystem.getUnusedCommitCapacity();
        pattern[0] = (short)0xfedc;
        packed[0] = (byte)0x89;
        packed[1] = (byte)0xab;
        short filled = ArrayLogic.arrayFillGenericNonAtomic(
            destination, (short)0, (short)1, pattern, (short)0);
        short copied = ArrayLogic.arrayCopyRepackNonAtomic(
            packed, (short)0, (short)2, destination, (short)1);
        boolean unchangedCapacity = JCSystem.getUnusedCommitCapacity() == capacity;
        ArrayLogic.arrayFillGeneric(destination, (short)0, (short)2, pattern, (short)1);
        JCSystem.abortTransaction();
        buffer[0] = (byte)(filled == 1 && copied == 2 && unchangedCapacity &&
                          destination[0] == (short)0xfedc &&
                          destination[1] == (short)0x89ab ? 1 : 0);
        break;
      }
      case 127: {
        byte[] pattern = JCSystem.makeTransientByteArray((short)2, JCSystem.CLEAR_ON_RESET);
        pattern[0] = 1;
        ArrayLogic.arrayFillGeneric(transactionSmallArray, (short)0, (short)2,
                                    pattern, (short)0);
        JCSystem.beginTransaction();
        pattern[0] = 2;
        ArrayLogic.arrayFillGeneric(transactionSmallArray, (short)0, (short)2,
                                    pattern, (short)0);
        short capacity = JCSystem.getUnusedCommitCapacity();
        pattern[0] = 3;
        pattern[1] = 4;
        short filled = ArrayLogic.arrayFillGenericNonAtomic(
            transactionSmallArray, (short)0, (short)1, pattern, (short)0);
        short copied = ArrayLogic.arrayCopyRepackNonAtomic(
            pattern, (short)1, (short)1, transactionSmallArray, (short)1);
        boolean unchangedCapacity = JCSystem.getUnusedCommitCapacity() == capacity;
        ArrayLogic.arrayFillGeneric(transactionSmallArray, (short)0, (short)2,
                                    pattern, (short)1);
        JCSystem.abortTransaction();
        boolean survived = ArrayLogic.arrayCompareGeneric(
            transactionSmallArray, (short)0, pattern, (short)0, (short)2) == 0;
        short found = ArrayLogic.arrayFindGeneric(
            transactionSmallArray, (short)0, pattern, (short)1);
        buffer[0] = (byte)(filled == 1 && copied == 2 && unchangedCapacity &&
                          survived && found == 1 ? 1 : 0);
        break;
      }
      case 126: {
        byte[] bytes = JCSystem.makeTransientByteArray((short)2, JCSystem.CLEAR_ON_RESET);
        short[] words = JCSystem.makeTransientShortArray((short)1, JCSystem.CLEAR_ON_RESET);
        try {
          ArrayLogic.arrayCopyRepack(this, (short)0, (short)1, words, (short)0);
          ISOException.throwIt(ISO7816.SW_UNKNOWN);
        } catch (UtilException expected) {
          if (expected.getReason() != UtilException.ILLEGAL_VALUE) {
            ISOException.throwIt(ISO7816.SW_WRONG_DATA);
          }
        }
        try {
          ArrayLogic.arrayFillGeneric(words, (short)0, (short)1, bytes, (short)0);
          ISOException.throwIt(ISO7816.SW_UNKNOWN);
        } catch (CardRuntimeException expected) {
          if (!(expected instanceof UtilException) ||
              expected.getReason() != UtilException.TYPE_MISMATCHED) {
            ISOException.throwIt(ISO7816.SW_WRONG_DATA);
          }
        }
        bytes[0] = (byte)0x12;
        bytes[1] = (byte)0x34;
        short end = ArrayLogic.arrayCopyRepack(bytes, (short)0, (short)2,
                                              words, (short)0);
        buffer[0] = (byte)(end == 1 && words[0] == (short)0x1234 ? 1 : 0);
        break;
      }
      case 125: {
        buffer[0] = (byte)countFrames(buffer[ISO7816.OFFSET_P2]);
        break;
      }
      case 117: {
        oneShotSelectProbe = 1;
        buffer[0] = 1;
        break;
      }
      case 118: {
        buffer[0] = oneShotSelectProbe;
        oneShotSelectProbe = 0;
        break;
      }
      case 107: {
        // Set up the RAM cost probe: container and result words first, so
        // the following measurements see only the probe objects.
        memoryProbe = new Object[64];
        if (memoryProbeWords == null) {
          memoryProbeWords =
              JCSystem.makeTransientShortArray((short)2, JCSystem.CLEAR_ON_RESET);
        }
        length = appendHeapFree(buffer, (short)0);
        break;
      }
      case 108: {
        // P2=0: 64 byte[16]; P2=1: 64 holders each with a byte[16];
        // P2=2: 64 holders without arrays.
        byte mode = buffer[ISO7816.OFFSET_P2];
        for (short index = 0; index < (short)64; index++) {
          if (mode == 0) {
            memoryProbe[index] = new byte[16];
          } else {
            memoryProbe[index] =
                new SharedArrayHolder(mode == 1 ? new byte[16] : null);
          }
        }
        length = appendHeapFree(buffer, (short)0);
        break;
      }
      case 109: {
        length = appendHeapFree(buffer, (short)0);
        break;
      }
      case 111: {
        // Allocate 1 KB transient arrays until the card refuses. Expect
        // SystemException.NO_RESOURCE with the RAM reserve still free.
        memoryProbe = new Object[160];
        short count = 0;
        short reason = 0;
        try {
          for (; count < (short)160; count++) {
            memoryProbe[count] = JCSystem.makeTransientByteArray(
                (short)1024, JCSystem.CLEAR_ON_RESET);
          }
        } catch (SystemException e) {
          reason = e.getReason();
        }
        Util.setShort(buffer, (short)0, count);
        Util.setShort(buffer, (short)2, reason);
        length = appendHeapFree(buffer, (short)4);
        break;
      }
      case 110: {
        memoryProbe = null;
        JCSystem.requestObjectDeletion();
        length = appendHeapFree(buffer, (short)0);
        break;
      }
      case (byte)133: {
        byte[] local = new byte[1];
        local[0] = 0x11;
        JCSystem.beginTransaction();
        local[0] = 0x22;
        JCSystem.abortTransaction();
        buffer[0] = local[0];
        break;
      }
      case (byte)134: {
        JCSystem.beginTransaction();
        byte[] local = new byte[1];
        JCSystem.abortTransaction();
        buffer[0] = local == null ? (byte)1 : (byte)0;
        break;
      }
      case (byte)135: {
        byte rejected = 0;
        try {
          Util.arrayCompare(null, (short)0, buffer, (short)0, (short)0);
        } catch (NullPointerException expected) {
          rejected |= 1;
        }
        try {
          Util.arrayCompare(buffer, (short)0, buffer, (short)0, (short)-1);
        } catch (ArrayIndexOutOfBoundsException expected) {
          rejected |= 2;
        }
        buffer[0] = rejected;
        break;
      }
      case (byte)136:
        buffer[0] = 0;
        try {
          if (JCSystem.isTransient(null) == JCSystem.NOT_A_TRANSIENT_OBJECT) {
            buffer[0] = 1;
          }
        } catch (NullPointerException unexpected) {
          buffer[0] = 2;
        }
        break;
      case (byte)137:
        buffer[0] = 0;
        try {
          apdu.receiveBytes((short)ISO7816.OFFSET_CDATA);
        } catch (APDUException expected) {
          buffer[0] = (byte)expected.getReason();
        }
        break;
      case (byte)138:
      case (byte)139: {
        // Return the result as a status because setOutgoing has already run.
        short rejected = 0;
        if (command == (byte)138) {
          apdu.setOutgoing();
        } else {
          apdu.setOutgoingNoChaining();
        }
        try {
          apdu.setOutgoingLength((short)(command == (byte)138 ? 257 : 253));
        } catch (APDUException expected) {
          rejected = expected.getReason();
        }
        ISOException.throwIt(rejected == APDUException.BAD_LENGTH ?
            (short)0x6281 : (short)(0x6f10 | rejected));
        return;
      }
      case (byte)140:
        buffer[0] = 0x42;
        apdu.setOutgoingAndSend((short)0, (short)1);
        ISOException.throwIt((short)0x6283);
        return;
      case (byte)141:
        length = denseIntSwitchProbe(buffer);
        break;
      case (byte)142:
        rejectNextSelect = 2;
        buffer[0] = 1;
        break;
      case (byte)144: {
        final byte before = transactionCapacityArray[0];
        short reason = 0;
        try {
          Util.setShort(transactionCapacityArray, (short)0, (short)0x1234);
        } catch (TransactionException expected) {
          reason = expected.getReason();
        }
        byte depth = JCSystem.getTransactionDepth();
        if (depth != 0) JCSystem.abortTransaction();
        buffer[0] = (byte)reason;
        buffer[1] = depth;
        // 0 when the failed atomic update left the first byte unchanged.
        buffer[2] = (byte)(transactionCapacityArray[0] == before ? 0 : 1);
        length = 3;
        break;
      }
      case (byte)145:
        buffer[0] = transientEcdsaInitReason();
        break;
      case (byte)146: {
        replacementProbeKey = (HMACKey)KeyBuilder.buildKey(
            KeyBuilder.TYPE_HMAC, KeyBuilder.LENGTH_HMAC_SHA_256_BLOCK_64, false);
        final byte[] material = {1, 2, 3, 4, 5, 6};
        replacementProbeKey.setKey(material, (short)0, (short)6);
        JCSystem.beginTransaction();
        replacementProbeKey.setKey(material, (short)0, (short)4);
        short capacity = JCSystem.getUnusedCommitCapacity();
        replacementProbeKey.setKey(material, (short)1, (short)5);
        buffer[0] = (byte)(capacity == JCSystem.getUnusedCommitCapacity() ? 1 : 0);
        buffer[1] = replacementProbeKey.getKey(buffer, (short)2);
        JCSystem.abortTransaction();
        buffer[7] = replacementProbeKey.getKey(buffer, (short)8);
        replacementProbeKey.clearKey();
        replacementProbeKey = null;
        length = 14;
        break;
      }
      case (byte)147: {
        // Finding #2: the APDU buffer and APDU object are JCRE temporary
        // objects; storing them in a field, array or static is refused.
        byte rejected = 0;
        if (apduStoreArray == null) apduStoreArray = new Object[1];
        try { apduBufferField = buffer; } catch (SecurityException e) { rejected |= 1; }
        try { apduStoreArray[0] = buffer; } catch (SecurityException e) { rejected |= 2; }
        try { staticApduBuffer = buffer; } catch (SecurityException e) { rejected |= 4; }
        try { apduObjectField = apdu; } catch (SecurityException e) { rejected |= 8; }
        apduBufferField = null;
        apduStoreArray[0] = null;
        staticApduBuffer = null;
        apduObjectField = null;
        buffer[0] = rejected;
        break;
      }
      case (byte)148:
        buffer[0] = installArgumentStoreRejected;
        break;
      case (byte)149:
        buffer[0] = installTransactionBeforeRegister;
        buffer[1] = installTransactionAfterRegister;
        length = 2;
        break;
      case (byte)150: {
        // Finding #12, part 1: a CLEAR_ON_DESELECT key must not keep working
        // through the Cipher after deselect (case 151 after reselection).
        // The first init creates a CLEAR_ON_RESET workspace that the
        // same-size deselect key would reuse.
        deselectKeyCipher = Cipher.getInstance(Cipher.ALG_AES_BLOCK_128_ECB_NOPAD, false);
        deselectKeyCipher.init(aesKey(KeyBuilder.TYPE_AES_TRANSIENT_RESET, (byte)0x10),
            Cipher.MODE_ENCRYPT);
        deselectKeyCipher.init(aesKey(KeyBuilder.TYPE_AES_TRANSIENT_DESELECT, (byte)0x11),
            Cipher.MODE_ENCRYPT);
        cipherReference = new byte[16];
        deselectKeyCipher.doFinal(new byte[16], (short)0, (short)16, cipherReference, (short)0);
        buffer[0] = 1;
        break;
      }
      case (byte)151: {
        byte result = 1;
        try {
          byte[] output = new byte[16];
          deselectKeyCipher.doFinal(new byte[16], (short)0, (short)16, output, (short)0);
          if (sameBytes(output, cipherReference, (short)16)) result = 2;
        } catch (CryptoException expected) {
          result = 1;
        }
        buffer[0] = result;
        break;
      }
      case (byte)152: {
        // Finding #12, part 2: re-initializing with a CLEAR_ON_RESET key of
        // the same size must keep working after deselect (case 153).
        reinitializedCipher = Cipher.getInstance(Cipher.ALG_AES_BLOCK_128_ECB_NOPAD, false);
        reinitializedCipher.init(aesKey(KeyBuilder.TYPE_AES_TRANSIENT_DESELECT, (byte)0x22),
            Cipher.MODE_ENCRYPT);
        reinitializedCipher.init(aesKey(KeyBuilder.TYPE_AES_TRANSIENT_RESET, (byte)0x33),
            Cipher.MODE_ENCRYPT);
        cipherReference = new byte[16];
        reinitializedCipher.doFinal(new byte[16], (short)0, (short)16, cipherReference, (short)0);
        buffer[0] = 1;
        break;
      }
      case (byte)153: {
        byte result = 2;
        try {
          byte[] output = new byte[16];
          reinitializedCipher.doFinal(new byte[16], (short)0, (short)16, output, (short)0);
          if (sameBytes(output, cipherReference, (short)16)) result = 1;
        } catch (CryptoException unexpected) {
          result = 3;
        }
        buffer[0] = result;
        break;
      }
      case (byte)154:
        buffer[0] = gcmResetProbe();
        break;
      case (byte)156:
        throw new ProbeISOException((short)0x6A88);
      case (byte)157: {
        // Finding #25: uncaught VM-raised exceptions end with 6F00 and leave
        // the card responsive. P2 selects the exception.
        byte zero = (byte)(buffer[ISO7816.OFFSET_P2] - buffer[ISO7816.OFFSET_P2]);
        switch (buffer[ISO7816.OFFSET_P2]) {
          case 0: { byte[] missing = null; buffer[0] = missing[zero]; break; }
          case 1: buffer[0] = (byte)(1 / zero); break;
          case 2: buffer[0] = buffer[(short)(zero + 300)]; break;
          case 3: { Object value = buffer; buffer[0] = ((AID)value).getPartialBytes(
              (short)0, buffer, (short)0, (byte)1); break; }
          default: buffer[0] = (byte)new byte[(short)(zero - 1)].length; break;
        }
        break;
      }
      case (byte)158: {
        byte result = 3;
        try {
          throwNonThrowable(this);
        } catch (SecurityException expected) {
          result = 0;
        } catch (RuntimeException wrong) {
          result = 1;
        }
        buffer[0] = result;
        break;
      }
      case 56: {
        rejectNextSelect = 1;
        buffer[0] = 1;
        length = 1;
        break;
      }
      default:
        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
    if (!manualResponse) apdu.setOutgoingAndSend((short)0, length);
  }
}

final class SharedArrayHolder {
  byte[] first;
  byte[] second;

  SharedArrayHolder(byte[] shared) {
    first = shared;
    second = shared;
  }
}

final class NestedArrayHolder {
  Object[] children;

  NestedArrayHolder(Object[] children) {
    this.children = children;
  }
}
