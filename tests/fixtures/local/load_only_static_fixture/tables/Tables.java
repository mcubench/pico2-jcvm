package lostatic.tables;

/**
 * Review finding #17. The converter refuses array initializers in library
 * packages, so the build adds one for TABLE ({0x11, 0x22, 0x33}) to the CAP's
 * StaticField component, giving a LOAD-only package with array_init entries.
 */
public final class Tables {
  public static byte[] TABLE;
}
