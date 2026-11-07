import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Host-side APDU driver for the small persistent-reclaimer test CAP. */
public final class PersistentReclaimerPhysicalProbe {
  private static final byte[] APPLET_AID = {
      (byte)0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x02, 0x01
  };

  private static ResponseAPDU send(CardChannel channel, String name,
                                   CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    StringBuilder hex = new StringBuilder();
    for (byte value : response.getBytes()) {
      hex.append(String.format("%02X", value & 0xff));
    }
    System.out.printf("APDU:%s response=%s sw=%04X%n", name,
        hex.toString(), response.getSW());
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response;
  }

  private static void expect(CardChannel channel, int instruction, int value,
                             String name) throws Exception {
    ResponseAPDU response = send(channel, name,
        new CommandAPDU(0x80, 0x00, instruction, 0, new byte[0], 256));
    byte[] data = response.getData();
    if (data.length != 1 || (data[0] & 0xff) != value) {
      throw new IllegalStateException(name + " expected=" +
          String.format("%02X", value) + " actual=" +
          java.util.Arrays.toString(data));
    }
    System.out.println("TEST:PASS " + name);
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException(
          "usage: PersistentReclaimerPhysicalProbe <exact reader name>");
    }
    String requestedReader = args[0];
    CardTerminal target = null;
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
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
      CardChannel channel = card.getBasicChannel();
      send(channel, "SELECT_PERSISTENT_RECLAIMER_PROBE",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      expect(channel, 1, 0x2a, "persistent_array_root_created");
      expect(channel, 2, 0x00, "persistent_array_root_released");
      expect(channel, 3, 0x00, "released_persistent_array_is_unreachable");
      expect(channel, 4, 0x55, "new_persistent_root_created");
      expect(channel, 5, 0x55, "persistent_root_survived_reclamation");
      System.out.println("TEST:SUMMARY pass=5 fail=0");
    } finally {
      card.disconnect(false);
    }
  }
}
