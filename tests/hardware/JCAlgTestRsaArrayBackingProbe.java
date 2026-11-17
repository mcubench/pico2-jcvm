import javax.smartcardio.*;
import java.util.HexFormat;

// H2 probe (RAM backing of persistent arrays): RAM cost of RSA-1984 key
// pairs restored from flash (A) versus generated in this session (B). Free memory is read from the
// firmware's CHOUPI:RSA_MEMORY start_free line of each B043 x0.
// Run: tools/env javac -d build/h2probe tests/hardware/JCAlgTestRsaArrayBackingProbe.java
// then tools/env java -cp build/h2probe JCAlgTestRsaArrayBackingProbe "<exact reader>"
// alongside PICO2_HOST_USB_ACCESS=1 tools/monitor --serial <chip> --seconds 150.
public class JCAlgTestRsaArrayBackingProbe {
  static final String PREPARE =
      "B0310000160011000AFFFF000507C00007007CFFFF00010032000100";
  static final String MEASURE_X0 =
      "B0430000160011000AFFFF000507C00007007CFFFF00010000000100";

  static void tx(CardChannel ch, String label, String hex) throws Exception {
    long t = System.nanoTime();
    ResponseAPDU r = ch.transmit(new CommandAPDU(HexFormat.of().parseHex(hex)));
    System.out.printf("APDU:RESULT label=%s sw=%04X elapsed_ms=%d%n", label,
        r.getSW(), (System.nanoTime() - t) / 1_000_000);
    if (r.getSW() != 0x9000) throw new IllegalStateException(label);
  }

  static void gc(CardChannel ch, String label, int n) throws Exception {
    for (int i = 0; i < n; i++) tx(ch, label + "_" + i, "B0E2000000");
  }

  public static void main(String[] a) throws Exception {
    CardTerminal term = TerminalFactory.getDefault().terminals().getTerminal(a[0]);
    Card card = term.connect("*");
    CardChannel ch = card.getBasicChannel();
    tx(ch, "SELECT", "00A404000A4A43416C67546573743100");
    gc(ch, "A_GC_PRE", 3);
    tx(ch, "A_PREPARE", PREPARE);
    gc(ch, "A_GC", 3);
    tx(ch, "A_MEASURE_X0", MEASURE_X0);
    gc(ch, "A_GC_POST", 2);
    tx(ch, "FREE_RSA_CACHE", "B0E2070000");
    gc(ch, "RECLAIM", 12);
    tx(ch, "B_PREPARE_REGENERATE", PREPARE);
    gc(ch, "B_GC", 12);
    tx(ch, "B_MEASURE_X0", MEASURE_X0);
    gc(ch, "B_GC_POST", 2);
    card.disconnect(false);
  }
}
