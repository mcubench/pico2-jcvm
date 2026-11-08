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
    if (args.length < 1 || args.length > 3) {
      throw new IllegalArgumentException(
          "usage: PersistentReclaimerPhysicalProbe <exact reader name> [RSA cycles 1..16] [RSA bits 512|1024|2048|4096]");
    }
    String requestedReader = args[0];
    int rsaCycles = args.length >= 2 ? Integer.parseInt(args[1]) : 8;
    int rsaBits = args.length >= 3 ? Integer.parseInt(args[2]) : 4096;
    if (rsaCycles < 1 || rsaCycles > 16) {
      throw new IllegalArgumentException("RSA cycles must be in 1..16");
    }
    int sizeCode;
    switch (rsaBits) {
      case 512: sizeCode = 0; break;
      case 1024: sizeCode = 1; break;
      case 2048: sizeCode = 2; break;
      case 4096: sizeCode = 3; break;
      default:
        throw new IllegalArgumentException(
            "RSA bits must be 512, 1024, 2048, or 4096");
    }
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
      for (int cycle = 1; cycle <= rsaCycles; cycle++) {
        ResponseAPDU built = send(channel, "rsa_key_replacement_" + cycle,
            new CommandAPDU(0x80, 0x00, 7, sizeCode, new byte[0], 256));
        if (built.getData().length != 1 ||
            (built.getData()[0] & 0xff) != 0x37) {
          throw new IllegalStateException("RSA " + rsaBits +
              " key was not initialized on cycle " + cycle);
        }
        System.out.println("TEST:PASS rsa_key_replacement cycle=" + cycle +
            " bits=" + rsaBits);
      }
      ResponseAPDU cleared = send(channel, "clear_final_rsa_reference",
          new CommandAPDU(0x80, 0x00, 14, 0, new byte[0], 256));
      if (cleared.getData().length != 1 ||
          (cleared.getData()[0] & 0xff) != 0) {
        throw new IllegalStateException("RSA key reference did not clear");
      }
      System.out.println("TEST:PASS rsa_key_reference_cleared");
      System.out.println("TEST:METRIC rsa_builds=" + rsaCycles +
          " rsa_bits=" + rsaBits);
      System.out.println("TEST:SUMMARY pass=" + (6 + rsaCycles) + " fail=0");
    } finally {
      card.disconnect(false);
    }
  }
}
