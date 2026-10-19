import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Send bounded, applet-independent GlobalPlatform GET DATA probes. */
public final class GPGetDataApduProbe {
  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s sw=%04X%n", name,
        hex(command.getBytes()), hex(response.getBytes()), response.getSW());
    return response;
  }

  public static void main(String[] args) throws Exception {
    String requestedReader = args.length == 0 ? "Pico 2 CHOUPI CCID" : args[0];
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    CardTerminal terminal = null;
    for (CardTerminal candidate : terminals) {
      System.out.println("PCSC:READER " + candidate.getName());
      if (args.length == 0
          ? candidate.getName().startsWith(requestedReader)
          : candidate.getName().equals(requestedReader)) {
        terminal = candidate;
      }
    }
    if (terminal == null) {
      throw new IllegalStateException("requested PC/SC reader not found: " +
          requestedReader);
    }

    Card card = terminal.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU cardData = transmit(channel, "GET_DATA_CARD_RECOGNITION",
          new CommandAPDU(0x80, 0xCA, 0x00, 0x66, 256));
      String expected = "6618731606072A864886FC6B01640B06092A864886FC6B040310";
      if (cardData.getSW() != 0x9000 || !expected.equals(hex(cardData.getData()))) {
        throw new IllegalStateException("unexpected Card Recognition Data response");
      }

      ResponseAPDU cplc = transmit(channel, "GET_DATA_CPLC_UNAVAILABLE",
          new CommandAPDU(0x80, 0xCA, 0x9F, 0x7F, 256));
      if (cplc.getSW() != 0x6a88 || cplc.getData().length != 0) {
        throw new IllegalStateException("unavailable CPLC must return 6A88");
      }
      System.out.println("TEST:PASS gp_get_data_card_recognition_and_cplc");
    } finally {
      card.disconnect(false);
    }
  }
}
