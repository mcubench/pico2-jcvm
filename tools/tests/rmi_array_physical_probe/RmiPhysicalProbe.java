import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Exact-reader physical regression for Java Card RMI int[] marshalling. */
public final class RmiPhysicalProbe {
  private static final byte[] APPLET_AID = hex("A00000015100000101");

  private static byte[] hex(String value) {
    byte[] bytes = new byte[value.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
    }
    return bytes;
  }

  private static String hex(byte[] bytes) {
    StringBuilder out = new StringBuilder();
    for (byte b : bytes) out.append(String.format("%02X", b & 0xff));
    return out.toString();
  }

  private static void expect(CardChannel channel, String name, String command,
                             String expected) throws Exception {
    CommandAPDU apdu = new CommandAPDU(hex(command));
    ResponseAPDU response = channel.transmit(apdu);
    String actual = hex(response.getBytes());
    System.out.println("RMI_PHYSICAL:" + name + " response=" + actual);
    if (!actual.equals(expected)) {
      throw new AssertionError(name + " expected=" + expected + " actual=" + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("reader name required");
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = terminals.stream().filter(t -> t.getName().equals(args[0]))
        .findFirst().orElseThrow(() -> new IllegalStateException("reader not found: " + args[0]));
    System.out.println("RMI_PHYSICAL:READER " + target.getName());
    Card card = target.connect("*");
    try {
      System.out.println("RMI_PHYSICAL:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU selected = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      System.out.println("RMI_PHYSICAL:SELECT response=" + hex(selected.getBytes()));
      if (selected.getSW() != 0x9000) throw new AssertionError("SELECT failed");

      expect(channel, "sumInts", "803802020D0001BB6C0200000005FFFFFFFE00", "8100039000");
      expect(channel, "echoInts", "803802020D0001A33602123456789ABCDEF000",
          "8102123456789ABCDEF09000");
      expect(channel, "echoIntsEmpty", "80380202050001A3360000", "81009000");
      expect(channel, "echoIntsNull", "80380202050001A336FF00", "81FFFF9000");
      System.out.println("RMI_PHYSICAL:PASS int_array_rmi");
    } finally {
      card.disconnect(false);
    }
  }
}
