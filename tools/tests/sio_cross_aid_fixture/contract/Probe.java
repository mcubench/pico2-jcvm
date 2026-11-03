package sio.contract;

import javacard.framework.Shareable;
import javacard.framework.AID;

public interface Probe extends Shareable {
  short exercise(byte marker, short offset, short wide, AID callerAID,
                 byte[] nullable);
  int exerciseWide(int value);
  short throwSecurityException();
  short inspectPrivateArray(byte[] value);
  short inspectPrivateArrayNative(byte[] value);
  short inspectGlobalReference(Object[] values);
  byte[] echoArray(byte[] value);
  boolean sameReference(byte[] reference);
  boolean sameReferences(byte[] left, byte[] right);
  short inspectForeignCast(Object value);
  short inspectForeignInstanceof(Object value);
}
