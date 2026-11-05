import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** PC/SC lifecycle client for the disposable Cipher lifecycle CAP. */
public final class CipherLifecyclePcscProbe {
  private static final byte[] APPLET_AID = {
      (byte)0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x02, 0x03, 0x01
  };
  private static final byte[] CARD_MANAGER_AID = {
      (byte)0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00
  };

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) text.append(String.format("%02X", value & 0xff));
    return text.toString();
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command,
                                       String expected) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s%n", name,
        hex(command.getBytes()), hex(response.getBytes()));
    if (!hex(response.getBytes()).equals(expected)) {
      throw new IllegalStateException(name + ": expected=" + expected +
          " actual=" + hex(response.getBytes()));
    }
    System.out.println("TEST:PASS " + name);
    return response;
  }

  private static CardChannel selectApplet(CardChannel channel) throws Exception {
    transmit(channel, "applet SELECT",
        new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256), "9000");
    return channel;
  }

  private static void expectCommand(CardChannel channel, int instruction,
                                    String name, String expected)
      throws Exception {
    transmit(channel, name,
        new CommandAPDU(0x80, instruction, 0, 0, 256), expected);
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException(
          "usage: CipherLifecyclePcscProbe <exact reader name>");
    }
    String requestedReader = args[0];
    CardTerminal target = null;
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) target = terminal;
    }
    if (target == null) throw new IllegalStateException("reader not found: " + requestedReader);

    Card card = target.connect("*");
    try {
      CardChannel channel = selectApplet(card.getBasicChannel());
      expectCommand(channel, 10, "build transient key into Key local", "019000");
      expectCommand(channel, 9, "build transient key without retaining it", "019000");
      expectCommand(channel, 1, "build transient AES key", "019000");
      expectCommand(channel, 2, "set transient AES key", "019000");
      expectCommand(channel, 3, "create AES Cipher", "019000");
      expectCommand(channel, 4, "initialize retained Cipher", "019000");
      expectCommand(channel, 5, "Cipher works before deselect", "019000");
      expectCommand(channel, 6, "build transient RSA private key", "019000");
      expectCommand(channel, 7, "initialize transient RSA private key", "019000");
      expectCommand(channel, 8, "create RSA Cipher", "019000");
      expectCommand(channel, 11, "initialize retained RSA Cipher", "019000");

      transmit(channel, "card manager SELECT",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, CARD_MANAGER_AID, 256),
          "6F0A8408A0000001510000009000");
      channel = selectApplet(channel);
      expectCommand(channel, 2, "restore AES transient key after deselect", "019000");
      expectCommand(channel, 5, "Cipher rejects key cleared on deselect", "429000");
      expectCommand(channel, 13, "restore RSA transient key after deselect", "019000");
      expectCommand(channel, 12, "RSA Cipher copy cleared on deselect", "429000");

      expectCommand(channel, 1, "rebuild transient AES key", "019000");
      expectCommand(channel, 2, "reset-test set AES key", "019000");
      expectCommand(channel, 3, "reset-test create AES Cipher", "019000");
      expectCommand(channel, 4, "reset-test initialize Cipher", "019000");
      expectCommand(channel, 6, "reset-test build transient RSA key", "019000");
      expectCommand(channel, 7, "reset-test initialize transient RSA key", "019000");
      expectCommand(channel, 8, "reset-test create RSA Cipher", "019000");
      expectCommand(channel, 11, "reset-test initialize RSA Cipher", "019000");
      card.disconnect(true);
      card = target.connect("*");
      channel = selectApplet(card.getBasicChannel());
      expectCommand(channel, 2, "restore AES transient key after reset", "019000");
      expectCommand(channel, 5, "Cipher is uninitialized after reader reset", "449000");
      expectCommand(channel, 13, "restore RSA transient key after reset", "019000");
      expectCommand(channel, 12, "RSA Cipher is uninitialized after reset", "449000");
      System.out.println("TEST:SUMMARY pass=31 fail=0");
    } finally {
      card.disconnect(false);
    }
  }
}
