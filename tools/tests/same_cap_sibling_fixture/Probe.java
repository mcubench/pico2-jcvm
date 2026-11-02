package samecap;

import javacard.framework.Shareable;

public interface Probe extends Shareable {
  boolean isShared(byte[] candidate);
}
