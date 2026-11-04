package samecaptransient;

import javacard.framework.JCSystem;

final class TransientState {
  static byte[] deselectBytes;
  static byte[] resetBytes;
  static byte deselectCount;

  static void initialize() {
    if (deselectBytes == null) {
      deselectBytes = JCSystem.makeTransientByteArray(
          (short)1, JCSystem.CLEAR_ON_DESELECT);
      resetBytes = JCSystem.makeTransientByteArray(
          (short)1, JCSystem.CLEAR_ON_RESET);
    }
  }

  private TransientState() {}
}
