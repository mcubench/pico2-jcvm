package sio.contract;

/** Library static for review findings #1 and #19. */
public final class Registry {
  /** Written and read from different applet contexts (#1, #19). */
  public static Object slot;

  public static void put(Object value) {
    slot = value;
  }

  public static Object get() {
    return slot;
  }

  public static boolean isEmpty() {
    return slot == null;
  }
}
