import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Minimal, dependency-free PC/SC probe for the installed JCAlgTest applet. */
public final class JCAlgTestApduProbe {
  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static byte[] fromHex(String text) {
    String compact = text.replaceAll("\\s", "");
    if ((compact.length() & 1) != 0) {
      throw new IllegalArgumentException("hex data must contain whole bytes");
    }
    byte[] bytes = new byte[compact.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(compact.substring(2 * i, 2 * i + 2), 16);
    }
    return bytes;
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s sw=%04X%n", name,
        hex(command.getBytes()), hex(response.getBytes()), response.getSW());
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response;
  }

  public static void main(String[] args) throws Exception {
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
    }
    CardTerminal terminal = null;
    for (CardTerminal candidate : terminals) {
      if (candidate.getName().contains("Pico 2 CHOUPI CCID")) {
        terminal = candidate;
        break;
      }
    }
    if (terminal == null) {
      throw new IllegalStateException("PC/SC reader Pico 2 CHOUPI CCID not found");
    }

    Card card = terminal.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU select = transmit(channel, "SELECT_JCALGTEST",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00,
              new byte[] {0x4A, 0x43, 0x41, 0x6C, 0x67,
                          0x54, 0x65, 0x73, 0x74, 0x31}, 256));
      if (select.getSW() != 0x9000) {
        throw new IllegalStateException("SELECT failed");
      }

      ResponseAPDU version = transmit(channel, "GETVERSION",
          new CommandAPDU(0xB0, 0xE1, 0x00, 0x00, 256));
      String versionText = new String(version.getData(), "US-ASCII");
      if (!"1.8.2_jc305".equals(versionText)) {
        throw new IllegalStateException("unexpected applet version: " + versionText);
      }

      transmit(channel, "JCSYSTEM_INFO",
          new CommandAPDU(0xB0, 0x73, 0x01, 0x00,
              new byte[] {0x01}, 256));

      byte[] sha256Settings = fromHex(
          "0015 0004 FFFF FFFF FFFF 0006 0001 FFFF FFFF 0001 0001");
      transmit(channel, "SHA256_DOFINAL_PREPARE",
          new CommandAPDU(0xB0, 0x34, 0x00, 0x00, sha256Settings, 256));
      ResponseAPDU sha256DoFinal = transmit(channel, "SHA256_DOFINAL",
          new CommandAPDU(0xB0, 0x41, 0x00, 0x00, sha256Settings, 256));
      byte[] sha256Result = sha256DoFinal.getData();
      if (sha256Result.length < 1 || sha256Result[0] != (byte) 0xAA) {
        throw new IllegalStateException("SHA-256 doFinal did not report success");
      }

      ResponseAPDU unsupportedSignature = transmit(channel,
          "UNSUPPORTED_RSA_SHA1_SIGNATURE",
          new CommandAPDU(0xB0, 0x75, 0x12, 0x00,
              new byte[] {0x03, 0x0A, 0x00, 0x00, 0x00}, 256));
      byte[] unsupportedData = unsupportedSignature.getData();
      if (unsupportedData.length < 2 || unsupportedData[0] != 0x12 ||
          unsupportedData[1] != 0x03) {
        throw new IllegalStateException(
            "RSA-SHA1 should report class 0x12 / NO_SUCH_ALGORITHM (0x03)");
      }
      System.out.println("TEST:PASS jc_algtest_process_apdus version=" + versionText);
    } finally {
      card.disconnect(false);
    }
  }
}
