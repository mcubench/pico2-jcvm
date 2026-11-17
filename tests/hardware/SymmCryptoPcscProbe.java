import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.List;

/** Run precomputed symmetric crypto known-answer vectors through a PC/SC card. */
public final class SymmCryptoPcscProbe {
  private static final byte[] APPLET_AID = fromHex("A00000015100020101");
  private static final byte[] CARD_MANAGER_AID = fromHex("A000000151000000");
  private static byte[] fromHex(String value) {
    byte[] result = new byte[value.length() / 2];
    for (int i = 0; i < result.length; i++) {
      result[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
    }
    return result;
  }

  private static String hex(byte[] value) {
    StringBuilder result = new StringBuilder(value.length * 2);
    for (byte item : value) result.append(String.format("%02X", item & 0xff));
    return result.toString();
  }

  private static void expect(CardChannel channel, CommandAPDU command,
                             String expected, String label) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    String actual = hex(response.getBytes());
    System.out.printf("TEST:%s %s response=%s%n",
        actual.equals(expected) ? "PASS" : "FAIL", label, actual);
    if (!actual.equals(expected))
      throw new IllegalStateException(label + " expected " + expected + ", got " + actual);
  }

  private static CardChannel selectApplet(CardChannel channel) throws Exception {
    ResponseAPDU response = channel.transmit(
        new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
    if (response.getSW() != 0x9000)
      throw new IllegalStateException("probe applet SELECT failed: " + hex(response.getBytes()));
    return channel;
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("usage: SymmCryptoPcscProbe <reader-name> <vectors.tsv>");
    }
    String requestedReader = args[0];
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) target = terminal;
    }
    if (target == null) throw new IllegalStateException("reader not found: " + requestedReader);

    Card card = target.connect("*");
    int passed = 0;
    int failed = 0;
    try {
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU select = channel.transmit(new CommandAPDU(0x00, 0xA4, 0x04, 0x00,
          APPLET_AID, 256));
      System.out.printf("APDU:SELECT response=%s%n", hex(select.getBytes()));
      if (select.getSW() != 0x9000) throw new IllegalStateException("probe applet SELECT failed");

      expect(channel, new CommandAPDU(0x80, 0x21, 0, 0, 0),
          "019000", "JCSystem transient byte array allocation");
      passed++;
      expect(channel, new CommandAPDU(0x80, 0x22, 0, 0, 0),
          "019000", "KeyBuilder transient AES allocation");
      passed++;
      for (int stage = 1; stage <= 4; stage++) {
        expect(channel, new CommandAPDU(0x80, 0x1C + stage, 0, 0, 0),
            "019000", "AES transient setup stage " + stage);
        passed++;
      }
      ResponseAPDU manager = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, CARD_MANAGER_AID, 256));
      if (manager.getSW() != 0x9000)
        throw new IllegalStateException("card manager SELECT failed: " + hex(manager.getBytes()));
      channel = selectApplet(channel);
      expect(channel, new CommandAPDU(0x80, 0x1C, 0, 0, 0),
          "019000", "AES staged Cipher rejects deselected transient key");
      passed++;

      for (int selector = 0; selector < 2; selector++) {
        String label = selector == 0 ? "AES" : "RSA";
        expect(channel, new CommandAPDU(0x80, 0x1B, selector, 0, 0),
            "019000", label + " transient Cipher init for deselect");
        passed++;
        ResponseAPDU managerSelect = channel.transmit(new CommandAPDU(
            0x00, 0xA4, 0x04, 0x00, CARD_MANAGER_AID, 256));
        if (managerSelect.getSW() != 0x9000)
          throw new IllegalStateException("card manager SELECT failed: " + hex(managerSelect.getBytes()));
        channel = selectApplet(channel);
        expect(channel, new CommandAPDU(0x80, 0x1C, selector, 0, 0),
            "019000", label + " Cipher rejects deselected transient key");
        passed++;

        expect(channel, new CommandAPDU(0x80, 0x1B, selector, 0, 0),
            "019000", label + " transient Cipher init for reader reset");
        passed++;
        card.disconnect(true);
        card = target.connect("*");
        channel = card.getBasicChannel();
        channel = selectApplet(channel);
        expect(channel, new CommandAPDU(0x80, 0x1C, selector, 0, 0),
            "019000", label + " Cipher rejects reset-cleared transient key");
        passed++;
      }

      try (BufferedReader input = new BufferedReader(new FileReader(args[1]))) {
        String line;
        while ((line = input.readLine()) != null) {
          String[] fields = line.split("\\t", -1);
          if (fields.length != 3) throw new IllegalArgumentException("invalid vector line: " + line);
          byte[] command = fromHex(fields[1]);
          byte[] expected = fromHex(fields[2]);
          long started = System.nanoTime();
          System.out.printf("APDU:START label=%s command=%s%n",
              fields[0], fields[1]);
          System.out.flush();
          ResponseAPDU response = channel.transmit(new CommandAPDU(command));
          long elapsedMillis = (System.nanoTime() - started) / 1_000_000L;
          byte[] actual = response.getBytes();
          if (java.util.Arrays.equals(actual, expected)) {
            passed++;
            System.out.printf("TEST:PASS %s response=%s elapsed_ms=%d%n",
                fields[0], hex(actual), elapsedMillis);
          } else {
            failed++;
            System.out.printf("TEST:FAIL %s expected=%s actual=%s elapsed_ms=%d%n",
                fields[0], hex(expected), hex(actual), elapsedMillis);
          }
        }
      }
      System.out.printf("TEST:SUMMARY pass=%d fail=%d%n", passed, failed);
      System.out.println("TEST:SUMMARY transient_cipher_lifecycle=pass deselect=2 reset=2");
      if (failed != 0) throw new IllegalStateException("physical crypto vectors failed: " + failed);
    } finally {
      card.disconnect(false);
    }
  }
}
