import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Physical same-CAP transient clear-event probe. */
public final class SameCapTransientPhysicalProbe {
  private static final byte[] WRITER_AID = hex("A00000015100000601");
  private static final byte[] READER_AID = hex("A00000015100000602");

  private static byte[] hex(String value) {
    byte[] bytes = new byte[value.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
    }
    return bytes;
  }

  private static String hex(byte[] bytes) {
    StringBuilder value = new StringBuilder();
    for (byte b : bytes) value.append(String.format("%02X", b & 0xff));
    return value.toString();
  }

  private static ResponseAPDU transmit(CardChannel channel, String command)
      throws Exception {
    return channel.transmit(new CommandAPDU(hex(command)));
  }

  private static void select(CardChannel channel, byte[] aid) throws Exception {
    ResponseAPDU response = channel.transmit(
        new CommandAPDU(0x00, 0xA4, 0x04, 0x00, aid, 256));
    if (response.getSW() != 0x9000) {
      throw new AssertionError("SELECT failed: " + hex(response.getBytes()));
    }
  }

  private static void expect(CardChannel channel, String command,
                             String expected, String label) throws Exception {
    String actual = hex(transmit(channel, command).getBytes());
    System.out.println("SAME_CAP_TRANSIENT:" + label + " response=" + actual);
    if (!expected.equals(actual)) {
      throw new AssertionError(label + " expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("reader name required");
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = terminals.stream().filter(t -> t.getName().equals(args[0]))
        .findFirst().orElseThrow(() -> new IllegalStateException("reader not found: " + args[0]));
    Card card = target.connect("*");
    try {
      System.out.println("SAME_CAP_TRANSIENT:READER " + target.getName());
      System.out.println("SAME_CAP_TRANSIENT:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();

      select(channel, WRITER_AID);
      expect(channel, "8010000000", "5A6B9000", "initializeBothArrays");
      select(channel, READER_AID);
      expect(channel, "8020000000", "006B9000", "afterSiblingDeselect");

      card.disconnect(true);
      card = target.connect("*");
      channel = card.getBasicChannel();
      select(channel, WRITER_AID);
      expect(channel, "8011000000", "00009000", "afterCardReset");
      System.out.println("SAME_CAP_TRANSIENT:PASS deselect_and_reset");
    } finally {
      card.disconnect(false);
    }
  }
}
