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

      expect(channel, "allocationPressure", "80380202040001178900",
          "81FF809000");
      for (int pass = 2; pass <= 4; pass++) {
        expect(channel, "allocationPressure" + pass,
            "80380202040001178900", "81FF809000");
      }
      expect(channel, "sumInts", "803802020D0001BB6C0200000005FFFFFFFE00", "8100039000");
      expect(channel, "echoInts", "803802020D0001A33602123456789ABCDEF000",
          "8102123456789ABCDEF09000");
      expect(channel, "truncatedIntArray",
          "80380202090001A336050000000100", "9900039000");
      expect(channel, "oversizedDeclaredIntArray",
          "80380202090001A336FE0000000100", "9900039000");
      expect(channel, "echoIntsEmpty", "80380202050001A3360000", "81009000");
      expect(channel, "echoIntsNull", "80380202050001A336FF00", "81FFFF9000");
      expect(channel, "systemExceptionReason",
          "80380202040001168900", "822512349000");
      expect(channel, "cardExceptionReason", "803802020400010C1000",
          "822112349000");
      expect(channel, "cardRuntimeExceptionReason",
          "803802020400014E2400", "822212349000");
      expect(channel, "apduExceptionReason", "8038020204000199E700",
          "822012349000");
      expect(channel, "pinExceptionReason", "80380202040001CB2800",
          "822412349000");
      expect(channel, "transactionExceptionReason",
          "80380202040001F2F300", "822612349000");
      expect(channel, "userExceptionReason", "8038020204000122D000",
          "822712349000");
      expect(channel, "cryptoExceptionReason", "80380202040001EC5C00",
          "823012349000");
      expect(channel, "customCryptoExceptionReason",
          "803802020400018A9E00", "833012349000");
      System.out.println("RMI_PHYSICAL:PASS int_array_rmi");
    } finally {
      card.disconnect(false);
    }
  }
}
