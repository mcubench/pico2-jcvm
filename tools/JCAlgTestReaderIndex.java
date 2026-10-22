import javax.smartcardio.CardException;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CardTerminals;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Resolve a PC/SC reader name to the one-based index used by JCAlgTest 1.8.2. */
public final class JCAlgTestReaderIndex {
  private JCAlgTestReaderIndex() {}

  public static void main(String[] args) {
    if (args.length == 1 && "--list".equals(args[0])) {
      try {
        List<CardTerminal> readers = TerminalFactory.getDefault().terminals().list();
        for (int i = 0; i < readers.size(); i++) {
          System.out.println((i + 1) + "\t" + readers.get(i).getName());
        }
      } catch (CardException | SecurityException e) {
        System.err.println("Cannot enumerate PC/SC readers: " + e.getMessage());
        System.exit(5);
      }
      return;
    }
    if (args.length != 1) {
      System.err.println("Usage: JCAlgTestReaderIndex <exact reader name> | --list");
      System.exit(2);
    }

    try {
      List<CardTerminal> readers = TerminalFactory.getDefault().terminals().list();
      int match = -1;
      for (int i = 0; i < readers.size(); i++) {
        if (!readers.get(i).getName().equals(args[0])) continue;
        if (match != -1) {
          System.err.println("Reader name is ambiguous: " + args[0]);
          System.exit(3);
        }
        match = i;
      }
      if (match == -1) {
        System.err.println("PC/SC reader not found: " + args[0]);
        System.err.println("Readers visible to Java SmartcardIO:");
        for (CardTerminal reader : readers) System.err.println("  " + reader.getName());
        System.exit(4);
      }
      System.out.println(match + 1);
    } catch (CardException | SecurityException e) {
      System.err.println("Cannot enumerate PC/SC readers: " + e.getMessage());
      System.exit(5);
    }
  }
}
