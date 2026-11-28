import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Physical PC/SC probe for transaction rollback and commit-capacity usage. */
public final class JCSystemTransactionPhysicalProbe {
  private static final byte[] APPLET_AID = {
      (byte)0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x01
  };

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static long u16(byte[] bytes, int offset) {
    return ((long)(bytes[offset] & 0xff) << 8) | (bytes[offset + 1] & 0xffL);
  }

  private static byte[] exchange(CardChannel channel, String name,
                                 CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s sw=%04X%n", name,
        hex(command.getBytes()), hex(response.getBytes()), response.getSW());
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response.getData();
  }

  private static byte[] probe(CardChannel channel, int p1) throws Exception {
    return exchange(channel, "JCTransactionProbe_" + p1,
        new CommandAPDU(0x80, 0x00, p1, 0x00, new byte[0], 256));
  }

  private static void benchmarkPersistentArray(CardChannel channel)
      throws Exception {
    int[] expected = {64, 128, 192};
    for (int round = 0; round < expected.length; round++) {
      long started = System.nanoTime();
      byte[] result = probe(channel, 99);
      long elapsedNanos = System.nanoTime() - started;
      if (result.length != 2 || result[0] != 0 ||
          (result[1] & 0xff) != expected[round]) {
        throw new IllegalStateException("persistent array benchmark result=" +
            hex(result) + " round=" + round);
      }
      System.out.printf("BENCHMARK:PERSISTENT_ARRAY_ACCESS round=%d " +
          "iterations=192 elapsed_us=%d%n", round + 1,
          elapsedNanos / 1000);
    }
    System.out.println("TEST:PASS persistent_array_access_benchmark");
  }

  private static void benchmarkPersistentArrayLength(CardChannel channel)
      throws Exception {
    for (int round = 0; round < 3; round++) {
      long started = System.nanoTime();
      byte[] result = probe(channel, 101);
      long elapsedNanos = System.nanoTime() - started;
      if (result.length != 2 || result[0] != 0 || result[1] != 0) {
        throw new IllegalStateException("persistent array length benchmark=" +
            hex(result));
      }
      System.out.printf("BENCHMARK:PERSISTENT_ARRAY_LENGTH round=%d " +
          "iterations=1024 elapsed_us=%d%n", round + 1,
          elapsedNanos / 1000);
    }
    System.out.println("TEST:PASS persistent_array_length_benchmark");
  }

  private static void expectByte(CardChannel channel, int p1, int expected,
                                 String description) throws Exception {
    byte[] data = probe(channel, p1);
    if (data.length != 1 || (data[0] & 0xff) != expected) {
      throw new IllegalStateException(description + ": expected=" +
          String.format("%02X", expected) + " got=" + hex(data));
    }
    System.out.println("TEST:PASS " + description);
  }

  /** Send a raw APDU and require the exact response data and status word. */
  private static void expectRaw(CardChannel channel, String command,
                                String expected, String description)
      throws Exception {
    ResponseAPDU response = channel.transmit(new CommandAPDU(unhex(command)));
    String actual = hex(response.getBytes());
    System.out.printf("APDU:%s command=%s response=%s%n", description, command,
        actual);
    if (!actual.equals(expected)) {
      throw new IllegalStateException(description + ": expected=" + expected +
          " got=" + actual);
    }
    System.out.println("TEST:PASS " + description);
  }

  private static byte[] unhex(String text) {
    byte[] bytes = new byte[text.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte)Integer.parseInt(text.substring(2 * i, 2 * i + 2), 16);
    }
    return bytes;
  }

  /** Cases for docs/review_claude_20261127.md findings. */
  private static void reviewFindingCases(CardChannel channel) throws Exception {
    String counts = hex(probe(channel, 55));
    expectRaw(channel, "00A4040007F000000001234500", "6A82",
        "unknown AID SELECT returns 6A82");
    if (!hex(probe(channel, 55)).equals(counts)) {
      throw new IllegalStateException("unknown AID SELECT changed the selection");
    }
    System.out.println("TEST:PASS unknown AID SELECT preserves the current applet");
    String[][] cases = {
        {"8000850000", "119000", "abort restores an ordinary local array"},
        {"8000860000", "019000", "abort clears a reference allocated in the transaction"},
        {"8000870000", "039000", "Util.arrayCompare rejects null and negative length"},
        {"8000880000", "019000", "JCSystem.isTransient(null) is NOT_A_TRANSIENT_OBJECT"},
        {"8000890000", "019000", "receiveBytes before incoming state is ILLEGAL_USE"},
        {"80008A0000", "6281", "short outgoing length above 256 is BAD_LENGTH"},
        {"80008B0000", "6281", "no-chaining outgoing length above 252 is BAD_LENGTH"},
        {"80008C0000", "426283", "completed output survives a warning status"},
        {"80008D0000", "010203049000", "dense int switch uses 32-bit bounds"},
        {"00C0000001", "429000", "GET RESPONSE without pending data reaches the applet"},
        {"8000900000", "0300009000", "failed atomic Util.setShort closes its transaction"},
        {"8000910000", "009000", "ECDSA accepts a transient EC private key"},
        {"8000920000", "01050203040506060102030405069000",
            "HMAC length change reuses storage and rolls back"},
    };
    for (String[] entry : cases) expectRaw(channel, entry[0], entry[1], entry[2]);
    expectRaw(channel, "80008E0000", "019000", "arm select() rejection");
    expectRaw(channel, "00A4040008" + hex(APPLET_AID), "6999",
        "select() throwing ISOException fails with 6999");
    exchange(channel, "SELECT_AFTER_REJECTED_SELECT",
        new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
  }

  private static void expectTransactionArrayLength(CardChannel channel,
                                                  String description)
      throws Exception {
    byte[] data = probe(channel, 84);
    long length = data.length == 2 ? u16(data, 0) : -1;
    if (length != 32767) {
      throw new IllegalStateException(description + ": expected=32767 got=" +
          hex(data));
    }
    System.out.println("TEST:PASS " + description);
  }

  public static void main(String[] args) throws Exception {
    boolean resetOnly = args.length == 3 && "--reset-only".equals(args[2]);
    boolean probeOnly = args.length == 4 && "--probe-only".equals(args[2]);
    boolean preparePersistence = args.length == 3 &&
        "--prepare-persistence".equals(args[2]);
    boolean verifyPersistence = args.length == 3 &&
        "--verify-persistence".equals(args[2]);
    if ((args.length != 2 && !resetOnly && !probeOnly &&
         !preparePersistence && !verifyPersistence) ||
        !"--reader".equals(args[0]) ||
        (probeOnly && (Integer.parseInt(args[3]) < 1 ||
                       Integer.parseInt(args[3]) > 255))) {
      throw new IllegalArgumentException(
          "usage: JCSystemTransactionPhysicalProbe --reader <exact reader name> [--reset-only | --probe-only <P1> | --prepare-persistence | --verify-persistence]");
    }
    String requestedReader = args[1];
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    CardTerminal target = null;
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
      if (terminal.getName().equals(requestedReader)) target = terminal;
    }
    if (target == null) {
      throw new IllegalStateException("exact PC/SC reader not found: " +
          requestedReader);
    }

    Card card = target.connect("*");
    if (resetOnly) {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      card.disconnect(true);
      System.out.println("PCSC:RESET_PASS reader=" + requestedReader);
      return;
    }
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      exchange(channel, "SELECT_TRANSACTION_PROBE",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      if (!probeOnly && !preparePersistence && !verifyPersistence) {
        expectByte(channel, 41, 0x00,
            "global Object[] retains and returns a same-package object");
      }
      if (probeOnly) {
        probe(channel, Integer.parseInt(args[3]));
        return;
      }
      if (preparePersistence) {
        expectByte(channel, 6, 0x33,
            "persistent field committed before board cycle");
        expectByte(channel, 7, 0x33,
            "persistent field is readable before board cycle");
        expectTransactionArrayLength(channel,
            "large persistent array length is readable before board cycle");
        System.out.println("TEST:PASS prepared_persistent_field_for_board_cycle");
        return;
      }
      if (verifyPersistence) {
        expectByte(channel, 7, 0x33,
            "persistent field survived Pico firmware cycle");
        expectTransactionArrayLength(channel,
            "large persistent array length survived firmware cycle");
        System.out.println(
            "TEST:PASS physical_persistent_field_survived_firmware_cycle");
        return;
      }

      expectByte(channel, 5, 0x11,
          "abort restores persistent applet field");
      expectByte(channel, 7, 0x11,
          "aborted persistent field remains restored");
      expectByte(channel, 6, 0x33,
          "commit preserves persistent applet field");
      expectByte(channel, 7, 0x33,
          "committed persistent field survives next APDU");
      byte[] copyBeyondCommitCapacity = probe(channel, 89);
      if (copyBeyondCommitCapacity.length != 2 ||
          (copyBeyondCommitCapacity[0] & 0xff) != 0x01 ||
          (copyBeyondCommitCapacity[1] & 0xff) != 0x5a) {
        throw new IllegalStateException(
            "non-atomic array copy must work beyond commit capacity and " +
            "survive abort: " + hex(copyBeyondCommitCapacity));
      }
      System.out.println(
          "TEST:PASS Util.arrayCopyNonAtomic exceeds commit capacity and survives abort");
      byte[] copyAbortSameArray = probe(channel, 90);
      if (copyAbortSameArray.length != 2 ||
          (copyAbortSameArray[0] & 0xff) != 0x43 ||
          (copyAbortSameArray[1] & 0xff) != 0x12) {
        throw new IllegalStateException(
            "same-array non-atomic copy rollback mismatch: " +
            hex(copyAbortSameArray));
      }
      System.out.println(
          "TEST:PASS Util.arrayCopyNonAtomic same-array transaction abort");
      byte[] copyAbortOtherArray = probe(channel, 91);
      if (copyAbortOtherArray.length != 2 ||
          (copyAbortOtherArray[0] & 0xff) != 0x11 ||
          (copyAbortOtherArray[1] & 0xff) != 0x54) {
        throw new IllegalStateException(
            "other-array non-atomic copy rollback mismatch: " +
            hex(copyAbortOtherArray));
      }
      System.out.println(
          "TEST:PASS Util.arrayCopyNonAtomic preserves separate non-atomic array");
      byte[] copyThenAtomicAbort = probe(channel, 92);
      if (copyThenAtomicAbort.length != 2 ||
          (copyThenAtomicAbort[0] & 0xff) != 0x63 ||
          (copyThenAtomicAbort[1] & 0xff) != 0x12) {
        throw new IllegalStateException(
            "non-atomic copy followed by atomic write rollback mismatch: " +
            hex(copyThenAtomicAbort));
      }
      System.out.println(
          "TEST:PASS Util.arrayCopyNonAtomic survives later atomic rollback");
      expectTransactionArrayLength(channel,
          "large persistent array length is readable");
      benchmarkPersistentArray(channel);
      benchmarkPersistentArrayLength(channel);

      byte[] overflow = probe(channel, 83);
      if (overflow.length != 2 || (overflow[0] & 0xff) != 3 ||
          (overflow[1] & 0xff) != 0x5a) {
        throw new IllegalStateException(
            "BUFFER_FULL must be catchable and leave the pre-test array byte unchanged: " +
            hex(overflow));
      }
      System.out.println(
          "TEST:PASS physical_BUFFER_FULL_catchable_and_write_rejected");

      card.disconnect(true);
      card = target.connect("*");
      System.out.println("PCSC:ATR_AFTER_RESET " + hex(card.getATR().getBytes()));
      channel = card.getBasicChannel();
      exchange(channel, "SELECT_AFTER_CCID_POWER_CYCLE",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      expectByte(channel, 7, 0x33,
          "persistent field survived CCID card power cycle");
      expectTransactionArrayLength(channel,
          "large persistent array survives CCID card power cycle");

      byte[] capacity = probe(channel, 10);
      if (capacity.length != 24) {
        throw new IllegalStateException("unexpected case 10 response length=" +
            capacity.length + " data=" + hex(capacity));
      }
      long max = u16(capacity, 18);
      long unused = u16(capacity, 20);
      long activeUnused = u16(capacity, 22);
      System.out.printf("JCSYSTEM:COMMIT_CAPACITY max=%d unused=%d " +
          "after_persistent_write=%d%n", max, unused, activeUnused);
      if (max == 0 || unused > max || activeUnused >= unused) {
        throw new IllegalStateException(
            "persistent field update did not reduce the active commit budget");
      }
      System.out.println("TEST:PASS physical_commit_capacity_consumption");
      expectByte(channel, 38, 0x01,
          "global array reference is rejected from instance fields");
      expectByte(channel, 39, 0x01,
          "global array reference is rejected from static fields");
      expectByte(channel, 40, 0x01,
          "global array reference is rejected from array components");
      expectByte(channel, 126, 0x01,
          "ArrayLogic exact and superclass catches preserve reasons");
      expectByte(channel, 40, 0x01, "next APDU after ArrayLogic exceptions");
      expectByte(channel, 127, 0x01,
          "non-atomic ArrayLogic writes survive overlapping atomic rollback");
      expectByte(channel, 128, 0x01,
          "non-atomic ArrayLogic short writes survive overlapping atomic rollback");
      expectByte(channel, 129, 0x01,
          "atomic ArrayLogic rejects over-capacity writes without mutation");
      expectByte(channel, 130, 0x01,
          "non-atomic ArrayLogic updates an oversized record without commit capacity");
      expectByte(channel, 132, 0x01,
          "non-atomic ArrayLogic int writes survive overlapping atomic rollback");
      reviewFindingCases(channel);
      System.out.println("TEST:SUMMARY pass=42 fail=0");
    } finally {
      card.disconnect(false);
    }
  }
}
