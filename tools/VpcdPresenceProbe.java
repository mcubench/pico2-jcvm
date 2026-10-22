import javax.smartcardio.CardException;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Activate a configured VPCD endpoint with a bounded SmartcardIO query. */
public final class VpcdPresenceProbe {
  private VpcdPresenceProbe() {}

  public static void main(String[] args) throws Exception {
    if (args.length < 2 || args.length > 3) {
      throw new IllegalArgumentException(
          "usage: VpcdPresenceProbe <exact-reader-name> <timeout-ms> [--require-present]");
    }
    String wanted = args[0];
    long timeoutMillis = Long.parseLong(args[1]);
    if (timeoutMillis < 1 || timeoutMillis > 10_000) {
      throw new IllegalArgumentException("timeout must be between 1 and 10000 ms");
    }

    List<CardTerminal> readers = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = null;
    for (CardTerminal reader : readers) {
      if (!reader.getName().equals(wanted)) continue;
      if (target != null) {
        throw new CardException("duplicate PC/SC reader name: " + wanted);
      }
      target = reader;
    }
    if (target == null) {
      throw new CardException("PC/SC reader not found: " + wanted);
    }

    boolean present = target.waitForCardPresent(timeoutMillis);
    System.out.println("VPCD:PRESENCE_QUERY reader=" + wanted + " present=" + present
        + " timeout_ms=" + timeoutMillis);
    if (args.length == 3 && "--require-present".equals(args[2]) && !present) {
      System.exit(1);
    }
  }
}
