package sio.contract;

import javacard.framework.Shareable;
import javacard.framework.AID;

public interface Probe extends Shareable {
  short exercise(byte marker, short offset, short wide, AID callerAID,
                 byte[] nullable);
}
