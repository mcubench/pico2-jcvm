import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Run archived JCAlgTest RSA-1984/1536/2048 private and RSA signature paths. */
public final class JCAlgTestRsa1984PhysicalProbe {
  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) text.append(String.format("%02X", value & 0xff));
    return text.toString();
  }

  private static ResponseAPDU transmit(CardChannel channel, String label,
                                       CommandAPDU command,
                                       int expectedDataByte) throws Exception {
    System.out.printf("APDU:START label=%s command=%s%n", label,
                      hex(command.getBytes()));
    long started = System.nanoTime();
    ResponseAPDU response = channel.transmit(command);
    long elapsedMs = (System.nanoTime() - started) / 1_000_000L;
    System.out.printf("APDU:RESULT label=%s response=%s sw=%04X elapsed_ms=%d%n",
        label, hex(response.getBytes()), response.getSW(), elapsedMs);
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(label + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    if (expectedDataByte >= 0 &&
        (response.getData().length == 0 ||
         (response.getData()[0] & 0xff) != expectedDataByte)) {
      throw new IllegalStateException(label + " returned unexpected data=" +
                                      hex(response.getData()));
    }
    return response;
  }

  private static CommandAPDU command(int ins, byte[] data) {
    return new CommandAPDU(0xB0, ins, 0, 0, data, 256);
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("usage: JCAlgTestRsa1984PhysicalProbe <exact reader name>");
    }
    String requestedReader = args[0];
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal selected = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) selected = terminal;
    }
    if (selected == null) {
      throw new IllegalStateException("exact PC/SC reader not found: " + requestedReader);
    }

    byte[] config = new byte[] {
        0x00, 0x11, 0x00, 0x0a, (byte) 0xff, (byte) 0xff,
        0x00, 0x05, 0x07, (byte) 0xc0, 0x00, 0x07, 0x00,
        0x7c, (byte) 0xff, (byte) 0xff, 0x00, 0x01, 0x00,
        0x32, 0x00, 0x01
    };
    byte[] rsa1536Config = new byte[] {
        0x00, 0x11, 0x00, 0x0d, (byte) 0xff, (byte) 0xff,
        0x00, 0x0f, 0x00, (byte) 0x80, 0x00, 0x04, 0x01,
        0x00, (byte) 0xff, (byte) 0xff, 0x00, 0x02, 0x00,
        0x32, 0x00, 0x01
    };
    byte[] rsa2048CipherConfig = new byte[] {
        0x00, 0x11, 0x00, 0x0a, (byte) 0xff, (byte) 0xff,
        0x00, 0x05, 0x08, 0x00, 0x00, 0x07, 0x00, (byte) 0x80,
        (byte) 0xff, (byte) 0xff, 0x00, 0x01, 0x00, 0x32, 0x00,
        0x01
    };
    byte[] rsa2048SignaturePrepareConfig = new byte[] {
        0x00, 0x12, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x05, 0x08,
        0x00, 0x00, 0x0a, 0x01, 0x00, (byte) 0xff, (byte) 0xff,
        (byte) 0xff, (byte) 0xff, 0x00, 0x32, 0x00, 0x01
    };
    byte[] rsa2048SignatureMeasureConfig = new byte[] {
        0x00, 0x12, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x05, 0x08,
        0x00, 0x00, 0x0a, 0x01, 0x00, (byte) 0xff, (byte) 0xff,
        (byte) 0xff, (byte) 0xff, 0x00, 0x00, 0x00, 0x01
    };

    Card card = selected.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      transmit(channel, "SELECT_JCALGTEST",
          new CommandAPDU(0x00, 0xa4, 0x04, 0x00,
              new byte[] {0x4a, 0x43, 0x41, 0x6c, 0x67,
                          0x54, 0x65, 0x73, 0x74, 0x31}, 256), -1);
      transmit(channel, "GC_BEFORE_PREPARE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA1984_PREPARE_B031", command(0x31, config), 0xaa);
      transmit(channel, "GC_BEFORE_MEASURE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA1984_PRIVATE_PKCS1_X50_B043",
               command(0x43, config), 0xaa);
      transmit(channel, "GC_AFTER_MEASURE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA1536_PREPARE_B031", command(0x31, rsa1536Config), 0xaa);
      transmit(channel, "GC_BEFORE_RSA1536_MEASURE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA1536_PRIVATE_ISO14888_X50_B043",
               command(0x43, rsa1536Config), 0xaa);
      transmit(channel, "GC_AFTER_RSA1536_MEASURE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA2048_PRIVATE_PKCS1_PREPARE_B031",
               command(0x31, rsa2048CipherConfig), 0xaa);
      transmit(channel, "GC_BEFORE_RSA2048_CIPHER", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA2048_PRIVATE_PKCS1_X50_B043",
               command(0x43, rsa2048CipherConfig), 0xaa);
      transmit(channel, "GC_AFTER_RSA2048_CIPHER", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA2048_SHA_PKCS1_SIGNATURE_PREPARE_B032",
               command(0x32, rsa2048SignaturePrepareConfig), 0xaa);
      transmit(channel, "GC_BEFORE_RSA2048_SIGNATURE", command(0xe2, new byte[0]), -1);
      transmit(channel, "RSA2048_SHA_PKCS1_SIGNATURE_SIGN_B049",
               command(0x49, rsa2048SignatureMeasureConfig), 0xaa);
      transmit(channel, "GC_AFTER_RSA2048_SIGNATURE", command(0xe2, new byte[0]), -1);
      System.out.println("TEST:PASS RSA1984_PRIVATE_PKCS1_X50");
      System.out.println("TEST:PASS RSA1536_PRIVATE_ISO14888_X50");
      System.out.println("TEST:PASS RSA2048_PRIVATE_PKCS1_X50");
      System.out.println("TEST:PASS RSA2048_SHA_PKCS1_SIGNATURE_SIGN");
    } finally {
      card.disconnect(false);
    }
  }
}
