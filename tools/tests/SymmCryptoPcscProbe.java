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
          fromHex("A00000015100020101"), 256));
      System.out.printf("APDU:SELECT response=%s%n", hex(select.getBytes()));
      if (select.getSW() != 0x9000) throw new IllegalStateException("probe applet SELECT failed");

      try (BufferedReader input = new BufferedReader(new FileReader(args[1]))) {
        String line;
        while ((line = input.readLine()) != null) {
          String[] fields = line.split("\\t", -1);
          if (fields.length != 3) throw new IllegalArgumentException("invalid vector line: " + line);
          byte[] command = fromHex(fields[1]);
          byte[] expected = fromHex(fields[2]);
          long started = System.nanoTime();
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
      if (failed != 0) throw new IllegalStateException("physical crypto vectors failed: " + failed);
    } finally {
      card.disconnect(false);
    }
  }
}
