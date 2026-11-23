package javacard.security;

// Preserve the legacy ROM class and method tokens for stored instances.
// The ROM generator binds these declarations to native_random.cpp.
final class SecureRandomData extends RandomData {
  public void generateData(byte[] buffer, short offset, short length) {}
  public short nextBytes(byte[] buffer, short offset, short length) { return 0; }
  public void setSeed(byte[] buffer, short offset, short length) {}
  public byte getAlgorithm() { return 0; }
}
