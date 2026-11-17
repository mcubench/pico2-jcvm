import javax.smartcardio.*;
import java.util.HexFormat;

// H3 probe: repeated JCAlgTest RSA-1536 prepares each re-assign m_cipher and
// leave about 20 garbage records; each B0E2 (requestObjectDeletion) erases
// them in batches of 8. Tombstone compaction should fire only inside these
// requested passes once 256 tombstones accumulate. The elapsed time of every
// B0E2 is printed so the compaction cost is visible.
// Run: tools/env javac -d build/h3probe tests/hardware/JCAlgTestTombstoneCompactionProbe.java
// then tools/env java -cp build/h3probe JCAlgTestTombstoneCompactionProbe "<exact reader>" [cycles]
// alongside PICO2_HOST_USB_ACCESS=1 tools/monitor --serial <chip> --seconds N.
public class JCAlgTestTombstoneCompactionProbe {
  static final String PREPARE_RSA1536 =
      "B0310000160011000DFFFF000F008000040100FFFF00020032000100";

  static long tx(CardChannel ch, String label, String hex) throws Exception {
    long t = System.nanoTime();
    ResponseAPDU r = ch.transmit(new CommandAPDU(HexFormat.of().parseHex(hex)));
    long ms = (System.nanoTime() - t) / 1_000_000;
    System.out.printf("APDU:RESULT label=%s sw=%04X elapsed_ms=%d%n", label,
        r.getSW(), ms);
    if (r.getSW() != 0x9000) throw new IllegalStateException(label);
    return ms;
  }

  public static void main(String[] a) throws Exception {
    int cycles = a.length > 1 ? Integer.parseInt(a[1]) : 20;
    Card card = TerminalFactory.getDefault().terminals().getTerminal(a[0])
        .connect("*");
    CardChannel ch = card.getBasicChannel();
    tx(ch, "SELECT", "00A404000A4A43416C67546573743100");
    for (int c = 0; c < cycles; c++) {
      tx(ch, "PREPARE_" + c, PREPARE_RSA1536);
      for (int g = 0; g < 4; g++) tx(ch, "GC_" + c + "_" + g, "B0E2000000");
    }
    card.disconnect(false);
  }
}
