import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Exact-reader physical regression for cross-AID Shareable dispatch. */
public final class SioCrossAidPhysicalProbe {
  private static final byte[] APPLET_AID = hex("A00000015100000401");

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

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("reader name required");
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = terminals.stream().filter(t -> t.getName().equals(args[0]))
        .findFirst().orElseThrow(() -> new IllegalStateException("reader not found: " + args[0]));
    System.out.println("SIO_PHYSICAL:READER " + target.getName());
    Card card = target.connect("*");
    try {
      System.out.println("SIO_PHYSICAL:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU selected = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      System.out.println("SIO_PHYSICAL:SELECT response=" + hex(selected.getBytes()));
      if (selected.getSW() != 0x9000) throw new AssertionError("SELECT failed");

      ResponseAPDU response = channel.transmit(
          new CommandAPDU(hex("8010000000")));
      String actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:crossAidShareable response=" + actual);
      if (!actual.equals("45679000")) {
        throw new AssertionError("expected 45679000, got " + actual);
      }
      System.out.println("SIO_PHYSICAL:PASS cross_aid_shareable");

      response = channel.transmit(new CommandAPDU(hex("8015000000")));
      actual = hex(response.getBytes());
      if (!actual.equals("6A109000")) {
        throw new AssertionError("proxy retain expected 6A109000, got " + actual);
      }
      card.disconnect(true);
      card = target.connect("*");
      System.out.println("SIO_PHYSICAL:RESET_ATR " + hex(card.getATR().getBytes()));
      channel = card.getBasicChannel();
      selected = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("client reselect after reset failed: " +
                                 hex(selected.getBytes()));
      }
      response = channel.transmit(new CommandAPDU(hex("8016000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:staleProxyAfterReset response=" + actual);
      if (!actual.equals("6A0A9000")) {
        throw new AssertionError("stale proxy should fail closed with 6A0A9000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8013000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:intArgumentReturn response=" + actual);
      if (!actual.equals("765432109000")) {
        throw new AssertionError("expected 765432109000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8014000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:exceptionUnwind response=" + actual);
      if (!actual.equals("6A079000")) {
        throw new AssertionError("expected 6A079000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8010000000")));
      actual = hex(response.getBytes());
      if (!actual.equals("45679000")) {
        throw new AssertionError("post-exception expected 45679000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8011000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:deniedLookup response=" + actual);
      if (!actual.equals("6A019000")) {
        throw new AssertionError("expected 6A019000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8012000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:localReference response=" + actual);
      if (!actual.equals("6A039000")) {
        throw new AssertionError("expected 6A039000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8017000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:proxyEquals response=" + actual);
      if (!actual.equals("6A099000")) {
        throw new AssertionError("expected 6A099000, got " + actual);
      }
      System.out.println("SIO_PHYSICAL:PASS int_argument_return exception_unwind " +
                         "context_restore denied_lookup local_reference_firewall " +
                         "proxy_method_firewall");
    } finally {
      card.disconnect(false);
    }
  }
}
