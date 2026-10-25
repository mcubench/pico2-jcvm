import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Physical PC/SC probe for transaction rollback and commit-capacity usage. */
public final class JCSystemTransactionPhysicalProbe {
  private static final byte[] APPLET_AID = {
      0x4A, 0x43, 0x41, 0x6C, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31
  };

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static long u16(byte[] bytes, int offset) {
    return ((long)(bytes[offset] & 0xff) << 8) | (bytes[offset + 1] & 0xffL);
  }

  private static byte[] exchange(CardChannel channel, String name,
                                 CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s sw=%04X%n", name,
        hex(command.getBytes()), hex(response.getBytes()), response.getSW());
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response.getData();
  }

  private static byte[] probe(CardChannel channel, int p1) throws Exception {
    return exchange(channel, "JCTransactionProbe_" + p1,
        new CommandAPDU(0x80, 0x00, p1, 0x00, new byte[0], 256));
  }

  private static void expectByte(CardChannel channel, int p1, int expected,
                                 String description) throws Exception {
    byte[] data = probe(channel, p1);
    if (data.length != 1 || (data[0] & 0xff) != expected) {
      throw new IllegalStateException(description + ": expected=" +
          String.format("%02X", expected) + " got=" + hex(data));
    }
    System.out.println("TEST:PASS " + description);
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !"--reader".equals(args[0])) {
      throw new IllegalArgumentException(
          "usage: JCSystemTransactionPhysicalProbe --reader <exact reader name>");
    }
    String requestedReader = args[1];
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    CardTerminal target = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) target = terminal;
    }
    if (target == null) {
      throw new IllegalStateException("exact PC/SC reader not found: " +
          requestedReader);
    }

    Card card = target.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      exchange(channel, "SELECT_TRANSACTION_PROBE",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));

      expectByte(channel, 5, 0x11,
          "abort restores persistent applet field");
      expectByte(channel, 7, 0x11,
          "aborted persistent field remains restored");
      expectByte(channel, 6, 0x33,
          "commit preserves persistent applet field");
      expectByte(channel, 7, 0x33,
          "committed persistent field survives next APDU");

      byte[] capacity = probe(channel, 10);
      if (capacity.length != 24) {
        throw new IllegalStateException("unexpected case 10 response length=" +
            capacity.length + " data=" + hex(capacity));
      }
      long max = u16(capacity, 18);
      long unused = u16(capacity, 20);
      long activeUnused = u16(capacity, 22);
      System.out.printf("JCSYSTEM:COMMIT_CAPACITY max=%d unused=%d " +
          "after_persistent_write=%d%n", max, unused, activeUnused);
      if (max == 0 || unused > max || activeUnused >= unused) {
        throw new IllegalStateException(
            "persistent field update did not reduce the active commit budget");
      }
      System.out.println("TEST:PASS physical_commit_capacity_consumption");
      System.out.println("TEST:SUMMARY pass=5 fail=0");
    } finally {
      card.disconnect(false);
    }
  }
}
