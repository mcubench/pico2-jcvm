package algtest;

import javacard.framework.APDU;
import javacard.framework.APDUException;
import javacard.framework.AID;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
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

public final class JCTransactionProbe extends Applet {
  private static Object staticReferenceRoot;
  private static Object[] persistentReferenceArray;
  private static NestedArrayHolder persistentNestedReferenceHolder;
  private static SharedArrayHolder persistentReferenceHolder;
  private static byte[] gcRehydrateAliasA;
  private static byte[] gcRehydrateAliasB;
  private Object globalArrayInstanceReference;
  private byte value = 0x11;
  private byte selectCount;
  private byte deselectCount;
  private byte lastSelectWasReselect;
  private byte lastSelectProcessWasReselect;
  private byte selectingAppletProbeFlags;
  private byte rejectNextSelect;
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
  private byte[] transientResetArray;
  private byte[] transientDeselectArray;
  private final byte[] shaInput = {0x61, 0x62, 0x63};
  private final byte[] shaDigest = new byte[32];
  private final byte[] transactionCapacityArray;
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
    staticReferenceRoot = this;
    byte[] shared = {0x01};
    persistentReferenceArray = new Object[] {shared, shared};
    byte[] nestedLeaf = {0x6b};
    persistentNestedReferenceHolder = new NestedArrayHolder(
        new Object[] {nestedLeaf, nestedLeaf});
    persistentReferenceHolder = new SharedArrayHolder(shared);
    byte[] gcRehydratable = {0x5a};
    gcRehydrateAliasA = gcRehydratable;
    gcRehydrateAliasB = gcRehydratable;
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    if (bLength == 10) {
      new JCTransactionProbe(bArray, bOffset, bLength);
    } else if (bLength == 1 && bArray[bOffset] == (byte)0x7f) {
      new JCTransactionProbe(null, (short)0, (byte)0, false, false);
    } else {
      new JCTransactionProbe();
    }
  }

  public boolean select() {
    selectCount++;
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

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) {
      selectingAppletProbeFlags |= 2;
      lastSelectProcessWasReselect = reSelectingApplet() ? (byte)1 : (byte)0;
      return;
    }
    byte[] buffer = apdu.getBuffer();
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
          InitializedMessageDigest.OneShot.open(MessageDigest.ALG_SHA);
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
