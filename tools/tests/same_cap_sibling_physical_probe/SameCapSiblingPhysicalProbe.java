import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Physical channel-0 check for applet siblings loaded from one CAP package. */
public final class SameCapSiblingPhysicalProbe {
  private static final byte[] WRITER_AID = hex("A00000015100000501");
  private static final byte[] READER_AID = hex("A00000015100000502");

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

  private static ResponseAPDU select(CardChannel channel, byte[] aid)
      throws Exception {
    return channel.transmit(new CommandAPDU(0x00, 0xA4, 0x04, 0x00, aid, 256));
  }

  private static void expect(ResponseAPDU response, String expected,
                             String label) {
    String actual = hex(response.getBytes());
    System.out.println("SAME_CAP_PHYSICAL:" + label + " response=" + actual);
    if (!actual.equals(expected)) {
      throw new AssertionError(label + " expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length < 1 || args.length > 2) {
      throw new IllegalArgumentException("reader name [reinstalled]");
    }
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = terminals.stream().filter(t -> t.getName().equals(args[0]))
        .findFirst().orElseThrow(() -> new IllegalStateException("reader not found: " + args[0]));
    Card card = target.connect("*");
    try {
      System.out.println("SAME_CAP_PHYSICAL:READER " + target.getName());
      System.out.println("SAME_CAP_PHYSICAL:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();

      if (args.length == 2) {
        if (!"reinstalled".equals(args[1])) {
          throw new IllegalArgumentException("unknown probe mode: " + args[1]);
        }
        ResponseAPDU selected = select(channel, READER_AID);
        if (selected.getSW() != 0x9000) {
          throw new AssertionError("reinstalled reader SELECT failed: " +
                                   hex(selected.getBytes()));
        }
        expect(channel.transmit(new CommandAPDU(hex("8020000000"))),
               "000012350157123456799000", "reinstalledReaderRetainsPackageState");
        System.out.println("SAME_CAP_PHYSICAL:PASS reader_delete_reinstall");
        return;
      }

      ResponseAPDU selected = select(channel, READER_AID);
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("reader SELECT failed: " + hex(selected.getBytes()));
      }
      expect(channel.transmit(new CommandAPDU(hex("8021000000"))), "019000",
             "siblingShareableArrayIdentity");
      // Match the VPCD fixture's lifecycle: explicitly SELECT the reader
      // again after the server's Shareable call before reading package state.
      selected = select(channel, READER_AID);
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("reader reselect after SIO failed: " +
                                 hex(selected.getBytes()));
      }
      expect(channel.transmit(new CommandAPDU(hex("8020000000"))),
             "000012340056123456789000", "initialStaticValues");

      selected = select(channel, WRITER_AID);
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("writer SELECT failed: " + hex(selected.getBytes()));
      }
      expect(channel.transmit(new CommandAPDU(hex("8010000000"))),
             "000012350157123456799000", "writerUpdatesStaticState");

      selected = select(channel, READER_AID);
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("reader reselect failed: " + hex(selected.getBytes()));
      }
      expect(channel.transmit(new CommandAPDU(hex("8020000000"))),
             "000012350157123456799000", "siblingObservesStaticUpdate");
      System.out.println("SAME_CAP_PHYSICAL:PASS sibling_sio_identity static_values static_update");
    } finally {
      card.disconnect(false);
    }
  }
}
