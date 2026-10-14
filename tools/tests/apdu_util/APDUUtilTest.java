import javacardx.apdu.util.APDUUtil;

public final class APDUUtilTest {
  private static int checks;

  private static void check(boolean condition, String name) {
    checks++;
    if (!condition) {
      throw new AssertionError(name);
    }
  }

  private static void channel(int cla, int expected) {
    check((APDUUtil.getCLAChannel((byte)cla) & 0xFF) == expected,
        "channel CLA=" + Integer.toHexString(cla));
  }

  public static void main(String[] args) {
    channel(0x00, 0);
    channel(0x03, 3);
    channel(0x40, 4);
    channel(0x4F, 19);
    channel(0x20, 0);
    channel(0xFF, 0);

    check(APDUUtil.isCommandChainingCLA((byte)0x10), "type 4 chaining");
    check(APDUUtil.isCommandChainingCLA((byte)0x50), "type 16 chaining");
    check(!APDUUtil.isCommandChainingCLA((byte)0x20),
        "reserved CLA cannot chain");

    check(APDUUtil.isSecureMessagingCLA((byte)0x04), "type 4 secure messaging");
    check(APDUUtil.isSecureMessagingCLA((byte)0x60), "type 16 secure messaging");
    check(!APDUUtil.isSecureMessagingCLA((byte)0x20),
        "reserved CLA has no secure messaging");

    check(APDUUtil.isISOInterindustryCLA((byte)0x00), "interindustry CLA");
    check(!APDUUtil.isISOInterindustryCLA((byte)0x80), "proprietary CLA");
    check(APDUUtil.isValidCLA((byte)0x00), "valid CLA");
    check(!APDUUtil.isValidCLA((byte)0x20), "reserved CLA");
    check(!APDUUtil.isValidCLA((byte)0xFF), "invalid FF CLA");

    System.out.println("TEST:SUMMARY pass=" + checks + " fail=0");
  }
}
