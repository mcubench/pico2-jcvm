import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Focused physical probe for JCSystem short and jc304 memory queries. */
public final class JCSystemLiveMemoryProbe {
  private static long readUnsignedShort(byte[] bytes, int offset) {
    return ((long) (bytes[offset] & 0xff) << 8) |
        (long) (bytes[offset + 1] & 0xff);
  }

  private static long readUnsignedInt(byte[] bytes, int offset) {
    return (readUnsignedShort(bytes, offset) << 16) |
        readUnsignedShort(bytes, offset + 2);
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s response=%s sw=%04X%n", name,
        toHex(response.getBytes()), response.getSW());
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response;
  }

  private static String toHex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 0 && args.length != 2 ||
        args.length == 2 && !"--reader".equals(args[0])) {
      throw new IllegalArgumentException(
          "usage: JCSystemLiveMemoryProbe [--reader <exact reader name>]");
    }
    String requestedReader = args.length == 2 ? args[1] : null;
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    CardTerminal target = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (requestedReader == null
          ? terminal.getName().contains("Pico 2 CHOUPI CCID")
          : terminal.getName().equals(requestedReader)) {
        target = terminal;
      }
    }
    if (target == null) throw new IllegalStateException("requested Pico 2 reader not found");

    Card card = target.connect("*");
    try {
      CardChannel channel = card.getBasicChannel();
      transmit(channel, "SELECT_JCALGTEST", new CommandAPDU(0x00, 0xA4, 0x04,
          0x00, new byte[] {0x4A, 0x43, 0x41, 0x6C, 0x67,
              0x54, 0x65, 0x73, 0x74, 0x31}, 256));
      ResponseAPDU version = transmit(channel, "GETVERSION",
          new CommandAPDU(0xB0, 0xE1, 0x00, 0x00, 256));
      String versionText = new String(version.getData(), "US-ASCII");
      if (!"1.8.2_jc305".equals(versionText)) {
        throw new IllegalStateException("unexpected applet version: " +
            versionText);
      }

      ResponseAPDU info = transmit(channel, "JCSYSTEM_LIVE_MEMORY",
          new CommandAPDU(0xB0, 0x73, 0x00, 0x00,
              new byte[] {0x00}, 256));
      byte[] data = info.getData();
      if (data.length != 29) {
        throw new IllegalStateException("unexpected JCSystem info length: " +
            data.length);
      }
      long persistentShort = readUnsignedShort(data, 3);
      long deselectShort = readUnsignedShort(data, 7);
      long persistentExtended = readUnsignedInt(data, 17);
      long deselectExtended = readUnsignedInt(data, 21);
      long resetExtended = readUnsignedInt(data, 25);
      if (persistentShort != Math.min(0x7fffL, persistentExtended) ||
          deselectShort != Math.min(0x7fffL, deselectExtended) ||
          resetExtended != deselectExtended || persistentExtended == 0) {
        throw new IllegalStateException(
            "JCSystem short and extended memory reports disagree");
      }
      System.out.printf(
          "TEST:PASS jcsystem_live_memory persistent=%d reset=%d deselect=%d%n",
          persistentExtended, resetExtended, deselectExtended);
    } finally {
      card.disconnect(false);
    }
  }
}
