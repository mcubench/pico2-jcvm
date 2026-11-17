import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Allocate rooted persistent arrays to prepare a physical high-store state. */
public final class FilesystemCapacityPhysicalProbe {
  private static final byte[] APPLET_AID = new byte[] {
      (byte)0xa0, 0, 0, 0, 0x62, 0x03, 0x05, 0x01
  };

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) text.append(String.format("%02X", value & 0xff));
    return text.toString();
  }

  private static long word(byte[] data, int offset) {
    return ((long)(data[offset] & 0xff) << 24) |
           ((long)(data[offset + 1] & 0xff) << 16) |
           ((long)(data[offset + 2] & 0xff) << 8) |
           (long)(data[offset + 3] & 0xff);
  }

  private static long[] decodeMemory(ResponseAPDU response, String label)
      throws Exception {
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(label + " SW=" +
                                      String.format("%04X", response.getSW()));
    }
    byte[] data = response.getData();
    if (data.length != 13) {
      throw new IllegalStateException(label + " data=" + hex(data));
    }
    int count = data[0] & 0xff;
    long persistent = word(data, 3);
    long transientBytes = word(data, 9);
    System.out.printf("MEMORY:RESULT label=%s chunks=%d persistent_free=%d " +
        "transient_free=%d response=%s%n", label, count, persistent,
        transientBytes, hex(response.getBytes()));
    return new long[] {count, persistent, transientBytes};
  }

  private static ResponseAPDU transmit(CardChannel channel, String label,
                                       CommandAPDU command) throws Exception {
    System.out.printf("APDU:START label=%s command=%s%n", label,
                      hex(command.getBytes()));
    long started = System.nanoTime();
    ResponseAPDU response = channel.transmit(command);
    long elapsedMs = (System.nanoTime() - started) / 1_000_000L;
    System.out.printf("APDU:RESULT label=%s response=%s sw=%04X elapsed_ms=%d%n",
        label, hex(response.getBytes()), response.getSW(), elapsedMs);
    return response;
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException(
          "usage: FilesystemCapacityPhysicalProbe <exact reader name>");
    }
    String requestedReader = args[0];
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal selected = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) selected = terminal;
    }
    if (selected == null) {
      throw new IllegalStateException("exact PC/SC reader not found: " +
                                      requestedReader);
    }

    Card card = selected.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU select = transmit(channel, "SELECT_CAPACITY_APPLET",
          new CommandAPDU(0x00, 0xa4, 0x04, 0x00, APPLET_AID, 256));
      if (select.getSW() != 0x9000) {
        throw new IllegalStateException("capacity applet SELECT failed");
      }
      long[] memory = null;
      for (int index = 1; index <= 3; index++) {
        ResponseAPDU fill = transmit(channel, "ALLOCATE_30000_CHUNK_" + index,
            new CommandAPDU(0x80, 0x70, 1, 0, 256));
        memory = decodeMemory(fill, "allocate_" + index);
        if (memory[0] != index) {
          throw new IllegalStateException("persistent chunk count did not advance");
        }
      }
      ResponseAPDU inspect = transmit(channel, "VERIFY_ROOTED_CHUNKS",
          new CommandAPDU(0x80, 0x70, 2, 0, 256));
      memory = decodeMemory(inspect, "verify");
      if (memory[0] != 3 || memory[1] == 0 || memory[1] >= 16384 ||
          memory[2] < 8192) {
        throw new IllegalStateException("store or transient memory is outside target bounds");
      }
      System.out.println("TEST:PASS FILESYSTEM_CAPACITY_STATE_READY");
    } finally {
      card.disconnect(false);
    }
  }
}
