import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;

/**
 * Sends a TSV of (label, command hex, expected response hex) to one applet on
 * an exact PC/SC reader and stops at the first mismatch.
 * Usage: ApduVectorRunner <reader name> <applet AID hex> <vectors.tsv>
 */
public final class ApduVectorRunner {
  private static byte[] fromHex(String hex) {
    byte[] out = new byte[hex.length() / 2];
    for (int i = 0; i < out.length; i++)
      out[i] = (byte) Integer.parseInt(hex.substring(2 * i, 2 * i + 2), 16);
    return out;
  }

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder();
    for (byte value : bytes) text.append(String.format("%02X", value));
    return text.toString();
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 3) throw new IllegalArgumentException("usage: reader aid tsv");
    CardTerminal terminal = TerminalFactory.getDefault().terminals().getTerminal(args[0]);
    if (terminal == null) throw new IllegalStateException("reader not found: " + args[0]);
    Card card = terminal.connect("*");
    try {
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU select = channel.transmit(
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, fromHex(args[1]), 256));
      System.out.printf("APDU:SELECT response=%s%n", hex(select.getBytes()));
      if (select.getSW() != 0x9000) throw new IllegalStateException("SELECT failed");
      List<String> lines = Files.readAllLines(Paths.get(args[2]));
      int index = 0;
      long totalMs = 0;
      for (String line : lines) {
        String[] fields = line.split("\t");
        index++;
        long start = System.nanoTime();
        ResponseAPDU response = channel.transmit(new CommandAPDU(fromHex(fields[1])));
        long ms = (System.nanoTime() - start) / 1_000_000;
        totalMs += ms;
        String actual = hex(response.getBytes());
        boolean pass = actual.equalsIgnoreCase(fields[2]);
        System.out.printf("APDU:%s %d ms=%d %s response=%s%n", pass ? "PASS" : "FAIL",
            index, ms, fields[0], actual);
        if (!pass) {
          System.out.printf("APDU:EXPECTED %s%n", fields[2]);
          System.out.printf("APDU:SUMMARY pass=%d fail=1 total=%d ms=%d%n", index - 1,
              lines.size(), totalMs);
          System.exit(1);
        }
      }
      System.out.printf("APDU:SUMMARY pass=%d fail=0 total=%d ms=%d%n", index,
          lines.size(), totalMs);
    } finally {
      card.disconnect(false);
    }
  }
}
