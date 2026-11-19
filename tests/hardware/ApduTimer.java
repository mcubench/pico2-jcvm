import javax.smartcardio.*;
import java.util.HexFormat;

// Send APDUs to one exact PC/SC reader and print the status word and elapsed
// time of each. Arguments after the reader are LABEL=HEX pairs; an optional
// xN suffix on the label repeats the APDU (e.g. B0E2x3=B0E2000000).
// A transport failure is printed as APDU:TRANSPORT_FAIL and ends the run
// with exit status 1. The bare argument RESET resets the card (disconnect
// with reset, then reconnect) and prints APDU:RESET.
// Build: tools/env javac -d build/apdu-timer tests/hardware/ApduTimer.java
// Run:   tools/env java -cp build/apdu-timer ApduTimer "<exact reader>" \
//          SELECT=00A404000A4A43416C67546573743100 B0E2x3=B0E2000000
public class ApduTimer {
  public static void main(String[] args) throws Exception {
    CardTerminal terminal =
        TerminalFactory.getDefault().terminals().getTerminal(args[0]);
    Card card = terminal.connect("*");
    CardChannel channel = card.getBasicChannel();
    int status = 0;
    outer:
    for (int index = 1; index < args.length; index++) {
      if (args[index].equals("RESET")) {
        card.disconnect(true);
        card = terminal.connect("*");
        channel = card.getBasicChannel();
        System.out.println("APDU:RESET");
        continue;
      }
      String[] pair = args[index].split("=", 2);
      String label = pair[0];
      int repeat = 1;
      int marker = label.lastIndexOf('x');
      if (marker > 0 && label.substring(marker + 1).matches("[0-9]+")) {
        repeat = Integer.parseInt(label.substring(marker + 1));
        label = label.substring(0, marker);
      }
      byte[] apdu = HexFormat.of().parseHex(pair[1]);
      for (int round = 0; round < repeat; round++) {
        long start = System.nanoTime();
        try {
          ResponseAPDU response = channel.transmit(new CommandAPDU(apdu));
          long ms = (System.nanoTime() - start) / 1_000_000;
          System.out.printf("APDU:RESULT label=%s round=%d sw=%04X data=%s "
              + "elapsed_ms=%d%n", label, round, response.getSW(),
              HexFormat.of().formatHex(response.getData()).toUpperCase(), ms);
        } catch (CardException error) {
          long ms = (System.nanoTime() - start) / 1_000_000;
          System.out.printf("APDU:TRANSPORT_FAIL label=%s round=%d "
              + "elapsed_ms=%d error=%s%n", label, round, ms, error);
          status = 1;
          break outer;
        }
      }
    }
    try {
      card.disconnect(false);
    } catch (CardException ignored) {
      // The reader may already be gone after a transport failure.
    }
    System.exit(status);
  }
}
