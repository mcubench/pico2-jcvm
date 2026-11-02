package samecap;

final class SharedState {
  static short zero;
  static short counter = 0x1234;
  static byte marker = 0x56;
  static int integer = 0x12345678;
  static byte[] bytes = new byte[1];

  private SharedState() {}
}
