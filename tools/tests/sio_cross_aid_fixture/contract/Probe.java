package sio.contract;

import javacard.framework.Shareable;
import javacard.framework.AID;

public interface Probe extends Shareable {
  short exercise(byte marker, short offset, short wide, AID callerAID,
                 byte[] nullable);
  short sensitiveCopy();
  short sensitiveFailedCopy();
  short sensitiveAIDResults();
  short noSensitiveApi();
  short sensitiveDigestResult();
  short sensitiveDigestFailure();
  short sensitiveCipherResult();
  short sensitiveSignatureResult();
  short sensitiveSignatureMismatchResult();
  short sensitiveObjectResult();
  byte[] sensitiveGlobalArrayResult();
  int exerciseWide(int value);
  short throwSecurityException();
  short throwCryptoException();
  short throwArrayIndexOutOfBoundsException();
  short inspectInheritedInterfaces();
  short validateNonAtomicUtil();
  short inspectPrivateArray(byte[] value);
  short inspectPrivateArrayNative(byte[] value);
  short inspectGlobalReference(Object[] values);
  short retainForeignReference(Object[] values);
  Object getRetainedForeignArray();
  boolean retainedForeignArrayIsNull();
  byte[] echoArray(byte[] value);
  boolean sameReference(byte[] reference);
  boolean sameReferences(byte[] left, byte[] right);
  short inspectForeignCast(Object value);
  short inspectForeignInstanceof(Object value);
}
