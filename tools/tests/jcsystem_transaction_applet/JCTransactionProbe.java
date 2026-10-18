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
import javacard.security.MessageDigest;
import javacard.security.InitializedMessageDigest;
import javacard.security.CryptoException;

public final class JCTransactionProbe extends Applet {
  private static Object staticReferenceRoot;
  private static Object[] persistentReferenceArray;
  private static SharedArrayHolder persistentReferenceHolder;
  private Object globalArrayInstanceReference;
  private byte value = 0x11;
  private byte lastApduExceptionReason = (byte)0x7f;
  private byte[] transientResetArray;
  private byte[] transientDeselectArray;
  private final byte[] shaInput = {0x61, 0x62, 0x63};
  private final byte[] shaDigest = new byte[32];

  private JCTransactionProbe() {
    register();
    staticReferenceRoot = this;
    byte[] shared = {0x01};
    persistentReferenceArray = new Object[] {shared, shared};
    persistentReferenceHolder = new SharedArrayHolder(shared);
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    new JCTransactionProbe();
  }

  public boolean select() {
    return true;
  }

  public void deselect() {}

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
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte command = buffer[ISO7816.OFFSET_P1];
    short length = 1;
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
          global[0] = this;
          buffer[0] = 0;
        } catch (java.lang.SecurityException e) {
          buffer[0] = 1;
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
        for (short i = 0; i < 64; i++) discarded = new byte[16];
        buffer[0] = discarded.length == 16 ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      case 52:
      case 53:
      case 54: {
        Object discarded = null;
        for (short i = 0; i < 64; i++) discarded = new Object();
        buffer[0] = discarded != null ? (byte)1 : (byte)0;
        length = 1;
        break;
      }
      default:
        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
    apdu.setOutgoingAndSend((short)0, length);
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
