import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.ArrayList;
import java.util.List;

/** Minimal, dependency-free PC/SC probe for the installed JCAlgTest applet. */
public final class JCAlgTestApduProbe {
  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static byte[] fromHex(String text) {
    String compact = text.replaceAll("\\s", "");
    if ((compact.length() & 1) != 0) {
      throw new IllegalArgumentException("hex data must contain whole bytes");
    }
    byte[] bytes = new byte[compact.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(compact.substring(2 * i, 2 * i + 2), 16);
    }
    return bytes;
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command) throws Exception {
    long startedAt = System.nanoTime();
    ResponseAPDU response = channel.transmit(command);
    long elapsedMillis = (System.nanoTime() - startedAt) / 1_000_000L;
    System.out.printf("APDU:%s command=%s response=%s sw=%04X elapsed_ms=%d%n",
        name, hex(command.getBytes()), hex(response.getBytes()),
        response.getSW(), elapsedMillis);
    if (response.getSW() != 0x9000) {
      throw new IllegalStateException(name + " returned SW=" +
          String.format("%04X", response.getSW()));
    }
    return response;
  }

  public static void main(String[] args) throws Exception {
    String requestedReader = null;
    boolean skipJcSystemInfo = false;
    List<String> probeArgs = new ArrayList<>();
    for (int i = 0; i < args.length; i++) {
      if ("--reader".equals(args[i])) {
        if (requestedReader != null || i + 1 >= args.length) {
          throw new IllegalArgumentException(
              "--reader must be supplied once with an exact PC/SC reader name");
        }
        requestedReader = args[++i];
      } else if ("--skip-jcsystem-info".equals(args[i])) {
        skipJcSystemInfo = true;
      } else {
        probeArgs.add(args[i]);
      }
    }
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    for (CardTerminal terminal : terminals) {
      System.out.println("PCSC:READER " + terminal.getName());
    }
    CardTerminal terminal = null;
    for (CardTerminal candidate : terminals) {
      if (requestedReader == null
              ? candidate.getName().contains("JCVM CCID")
              : candidate.getName().equals(requestedReader)) {
        terminal = candidate;
        break;
      }
    }
    if (terminal == null) {
      throw new IllegalStateException("PC/SC reader " +
          (requestedReader == null ? "JCVM CCID" : requestedReader) +
          " not found");
    }

    Card card = terminal.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU select = transmit(channel, "SELECT_JCALGTEST",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00,
              new byte[] {0x4A, 0x43, 0x41, 0x6C, 0x67,
                          0x54, 0x65, 0x73, 0x74, 0x31}, 256));
      if (select.getSW() != 0x9000) {
        throw new IllegalStateException("SELECT failed");
      }

      ResponseAPDU version = transmit(channel, "GETVERSION",
          new CommandAPDU(0xB0, 0xE1, 0x00, 0x00, 256));
      String versionText = new String(version.getData(), "US-ASCII");
      if (!"1.8.2_jc305".equals(versionText)) {
        throw new IllegalStateException("unexpected applet version: " + versionText);
      }

      if (!skipJcSystemInfo) {
        transmit(channel, "JCSYSTEM_INFO",
            new CommandAPDU(0xB0, 0x73, 0x01, 0x00,
                new byte[] {0x01}, 256));
      }

      if (probeArgs.size() == 1 && "--version-only".equals(probeArgs.get(0))) {
        System.out.println(
            "TEST:PASS jc_algtest_persistence_probe version=" + versionText);
        return;
      }

      if (probeArgs.size() == 1 &&
          "--unsupported-rsa-oaep-md5".equals(probeArgs.get(0))) {
        // Isolate the exact JCAlgTest ALG_SUPPORT_EXTENDED probe that lost its
        // PC/SC transaction on D92. MD5/RSA/OAEP is unsupported by this
        // firmware profile, so the applet should return class 0x26 and
        // NO_SUCH_ALGORITHM (0x03) in a normal 9000 response.
        ResponseAPDU response = transmit(channel,
            "UNSUPPORTED_RSA_OAEP_MD5",
            new CommandAPDU(0xB0, 0x75, 0x26, 0x00,
                new byte[] {0x02, 0x03, 0x09}, 256));
        byte[] result = response.getData();
        if (result.length < 2 || result[0] != 0x26 || result[1] != 0x03) {
          throw new IllegalStateException(
              "RSA OAEP MD5 should report class 0x26 / " +
              "NO_SUCH_ALGORITHM (0x03)");
        }
        System.out.println("TEST:PASS unsupported_rsa_oaep_md5_no_such_algorithm");
        return;
      }

      if (probeArgs.size() >= 1 && probeArgs.size() <= 2 &&
          "--secure-random-setseed".equals(probeArgs.get(0))) {
        int repeatCount = probeArgs.size() == 2
            ? Integer.parseInt(probeArgs.get(1)) : 50;
        if (repeatCount < 1 || repeatCount > 50) {
          throw new IllegalArgumentException("repeatCount must be 1..50");
        }
        // Isolate the first supported-path failure from the full static run.
        // These settings match ALG_SECURE_RANDOM RandomData_setSeed() in the
        // captured JCAlgTest 1.8.2 ALG_PERFORMANCE_STATIC sequence.
        byte[] setSeedMeasurement = fromHex(
            "0016 0002 FFFF FFFF FFFF 0002 0100 FFFF FFFF 0032 0001");
        setSeedMeasurement[18] = (byte) (repeatCount >>> 8);
        setSeedMeasurement[19] = (byte) repeatCount;
        byte[] setSeedBaseline = fromHex(
            "0016 0002 FFFF FFFF FFFF 0002 0100 FFFF FFFF 0000 0001");
        transmit(channel, "SECURE_RANDOM_SETSEED_PREPARE",
            new CommandAPDU(0xB0, 0x33, 0x00, 0x00,
                setSeedMeasurement, 256));
        for (int sample = 1; sample <= 5; sample++) {
          transmit(channel, "SECURE_RANDOM_SETSEED_BASELINE_" + sample,
              new CommandAPDU(0xB0, 0x42, 0x00, 0x00,
                  setSeedBaseline, 256));
        }
        transmit(channel, "SECURE_RANDOM_SETSEED_" + repeatCount + "_CALLS",
            new CommandAPDU(0xB0, 0x42, 0x00, 0x00,
                setSeedMeasurement, 256));
        System.out.println("TEST:PASS secure_random_setseed_static");
        return;
      }

      if (probeArgs.size() == 1 &&
          "--des-cipher-prepare".equals(probeArgs.get(0))) {
        // Isolate the first supported-looking Cipher prepare that returned
        // 6F00 in ALG_PERFORMANCE_STATIC. These are the exact JC305 settings
        // from the archived official-client APDU for DES CBC no padding,
        // 64-bit key, and doFinal. A failing SW is reported by transmit().
        byte[] desCipherSettings = fromHex(
            "0011 0001 FFFF 0003 0040 0007 0100 FFFF 0002 0032 0001");
        transmit(channel, "DES_CIPHER_PREPARE",
            new CommandAPDU(0xB0, 0x31, 0x00, 0x00,
                desCipherSettings, 256));
        System.out.println("TEST:PASS des_cipher_prepare");
        return;
      }

      if (probeArgs.size() == 1 &&
          "--unsupported-random-prepare".equals(probeArgs.get(0))) {
        // Reproduce ALG_PERFORMANCE_STATIC's ALG_KEYGENERATION prepare APDU.
        // RandomData.getInstance(6) is expected to return NO_SUCH_ALGORITHM
        // in the response data while the APDU itself completes normally.
        byte[] randomSettings = fromHex(
            "0016 0006 FFFF FFFF FFFF 0003 0100 FFFF FFFF 0032 0001");
        ResponseAPDU response = transmit(channel,
            "UNSUPPORTED_RANDOM_KEYGENERATION_PREPARE",
            new CommandAPDU(0xB0, 0x33, 0x00, 0x00, randomSettings, 256));
        byte[] result = response.getData();
        if (result.length != 1 || result[0] != 0x03) {
          throw new IllegalStateException(
              "ALG_KEYGENERATION should return NO_SUCH_ALGORITHM (03)");
        }
        System.out.println("TEST:PASS unsupported_random_prepare_no_such_algorithm");
        return;
      }

      if (probeArgs.size() == 1 && "--aes-cipher-prepare".equals(probeArgs.get(0))) {
        // Reproduce the first official AES-128 CBC-NOPAD preparation from
        // ALG_PERFORMANCE_STATIC. The probe reports the raw APDU status so
        // runtime exceptions are visible instead of being treated as a lost
        // PC/SC response.
        byte[] aesSettings = fromHex(
            "0011 000D FFFF 000F 0080 0007 0100 FFFF 0002 0032 0001");
        CommandAPDU command = new CommandAPDU(0xB0, 0x31, 0x00, 0x00,
            aesSettings, 256);
        ResponseAPDU response = channel.transmit(command);
        System.out.printf("APDU:AES_CIPHER_PREPARE command=%s response=%s sw=%04X%n",
            hex(command.getBytes()), hex(response.getBytes()), response.getSW());
        if (response.getSW() != 0x9000 || response.getData().length != 1 ||
            response.getData()[0] != (byte) 0xAA) {
          throw new IllegalStateException(
              "AES-CBC-NOPAD preparation did not report success");
        }
        System.out.println("TEST:PASS aes_cipher_prepare");
        return;
      }

      if (probeArgs.size() == 1 && "--aes-key-prepare".equals(probeArgs.get(0))) {
        byte[] aesSettings = fromHex(
            "0011 000D FFFF 000F 0080 0007 0100 FFFF 0002 0032 0001");
        ResponseAPDU response = channel.transmit(new CommandAPDU(
            0xB0, 0x30, 0x00, 0x00, aesSettings, 256));
        System.out.printf("APDU:AES_KEY_PREPARE response=%s sw=%04X%n",
            hex(response.getBytes()), response.getSW());
        if (response.getSW() != 0x9000 || response.getData().length != 2 ||
            response.getData()[0] != (byte) 0xAA ||
            response.getData()[1] != 0x00) {
          throw new IllegalStateException("AES key preparation did not report success");
        }
        System.out.println("TEST:PASS aes_key_prepare");
        return;
      }

      if (probeArgs.size() == 1 && "--aes-cipher-static".equals(probeArgs.get(0))) {
        // Match the official JCAlgTest ALG_PERFORMANCE_STATIC AES-128
        // CBC-NOPAD Cipher_doFinal case: prepare with INS 0x31, collect five
        // zero-repeat baselines, then measure 50 doFinal calls with INS 0x43.
        byte[] aesMeasurement = fromHex(
            "0011 000D FFFF 000F 0080 0007 0100 FFFF 0002 0032 0001");
        byte[] aesBaseline = fromHex(
            "0011 000D FFFF 000F 0080 0007 0100 FFFF 0002 0000 0001");
        transmit(channel, "AES_CIPHER_STATIC_PREPARE",
            new CommandAPDU(0xB0, 0x31, 0x00, 0x00, aesMeasurement, 256));
        for (int sample = 1; sample <= 5; sample++) {
          transmit(channel, "AES_CIPHER_STATIC_BASELINE_" + sample,
              new CommandAPDU(0xB0, 0x43, 0x00, 0x00, aesBaseline, 256));
        }
        ResponseAPDU measurement = transmit(channel,
            "AES_CIPHER_STATIC_50_DOFINALS",
            new CommandAPDU(0xB0, 0x43, 0x00, 0x00,
                aesMeasurement, 256));
        byte[] measuredData = measurement.getData();
        if (measuredData.length < 1 || measuredData[0] != (byte) 0xAA) {
          throw new IllegalStateException(
              "50-call AES-CBC-NOPAD workload did not report success");
        }
        System.out.println("TEST:PASS aes_cipher_static");
        return;
      }

      // Reproduce the official ALG_PERFORMANCE_STATIC SHA-256 prelude:
      // prepare once with the measured settings, collect five zero-repeat
      // baseline samples, then submit the 50-call measurement twice.
      byte[] officialStaticSettings = fromHex(
          "0015 0004 FFFF FFFF FFFF 0006 0100 FFFF FFFF 0032 0001");
      transmit(channel, "OFFICIAL_SHA256_STATIC_PREPARE",
          new CommandAPDU(0xB0, 0x34, 0x00, 0x00,
              officialStaticSettings, 256));
      byte[] officialBaselineSettings = fromHex(
          "0015 0004 FFFF FFFF FFFF 0006 0100 FFFF FFFF 0000 0001");
      for (int sample = 1; sample <= 5; sample++) {
        transmit(channel, "OFFICIAL_SHA256_BASELINE_" + sample,
            new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
                officialBaselineSettings, 256));
      }
      for (int measurement = 1; measurement <= 2; measurement++) {
        try {
          transmit(channel, "OFFICIAL_SHA256_STATIC_MEASUREMENT_" + measurement,
              new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
                  officialStaticSettings, 256));
        } catch (IllegalStateException failure) {
          if (!failure.getMessage().contains("SW=F105")) {
            throw failure;
          }
          System.out.println(
              "DIAGNOSTIC: reprepare after F105, then compare baseline reset");
          transmit(channel, "OFFICIAL_SHA256_REPREPARE_AFTER_F105",
              new CommandAPDU(0xB0, 0x34, 0x00, 0x00,
                  officialStaticSettings, 256));
          transmit(channel, "OFFICIAL_SHA256_AFTER_REPREPARE",
              new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
                  officialStaticSettings, 256));
          transmit(channel, "OFFICIAL_SHA256_BASELINE_AFTER_REPREPARE",
              new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
                  officialBaselineSettings, 256));
          transmit(channel, "OFFICIAL_SHA256_AFTER_BASELINE_RESET",
              new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
                  officialStaticSettings, 256));
          break;
        }
      }

      byte[] sha256Settings = fromHex(
          "0015 0004 FFFF FFFF FFFF 0006 0001 FFFF FFFF 0001 0001");
      transmit(channel, "SHA256_DOFINAL_PREPARE",
          new CommandAPDU(0xB0, 0x34, 0x00, 0x00, sha256Settings, 256));
      ResponseAPDU sha256DoFinal = transmit(channel, "SHA256_DOFINAL",
          new CommandAPDU(0xB0, 0x41, 0x00, 0x00, sha256Settings, 256));
      byte[] sha256Result = sha256DoFinal.getData();
      if (sha256Result.length < 1 || sha256Result[0] != (byte) 0xAA) {
        throw new IllegalStateException("SHA-256 doFinal did not report success");
      }
      for (int iteration = 1; iteration <= 10; iteration++) {
        ResponseAPDU repeatedDoFinal = transmit(channel,
            "SHA256_DOFINAL_REPEAT_" + iteration,
            new CommandAPDU(0xB0, 0x41, 0x00, 0x00, sha256Settings, 256));
        byte[] repeatedResult = repeatedDoFinal.getData();
        if (repeatedResult.length < 1 || repeatedResult[0] != (byte) 0xAA) {
          throw new IllegalStateException(
              "repeated SHA-256 doFinal did not report success at iteration " +
              iteration);
        }
      }

      byte[] staticSha256Settings = fromHex(
          "0015 0004 FFFF FFFF FFFF 0006 0100 FFFF FFFF 0032 0001");
      transmit(channel, "SHA256_STATIC_PREPARE",
          new CommandAPDU(0xB0, 0x34, 0x00, 0x00,
              staticSha256Settings, 256));
      ResponseAPDU staticResult = transmit(channel, "SHA256_STATIC_50_DOFINALS",
          new CommandAPDU(0xB0, 0x41, 0x00, 0x00,
              staticSha256Settings, 256));
      if (staticResult.getData().length < 1 ||
          staticResult.getData()[0] != (byte) 0xAA) {
        throw new IllegalStateException(
            "50-call static SHA-256 doFinal did not report success");
      }
      ResponseAPDU postStaticResult = transmit(channel,
          "SHA256_DOFINAL_AFTER_STATIC",
          new CommandAPDU(0xB0, 0x41, 0x00, 0x00, sha256Settings, 256));
      if (postStaticResult.getData().length < 1 ||
          postStaticResult.getData()[0] != (byte) 0xAA) {
        throw new IllegalStateException(
            "SHA-256 doFinal after the 50-call workload did not report success");
      }

      ResponseAPDU unsupportedSignature = transmit(channel,
          "UNSUPPORTED_RSA_SHA1_SIGNATURE",
          new CommandAPDU(0xB0, 0x75, 0x12, 0x00,
              new byte[] {0x03, 0x0A, 0x00, 0x00, 0x00}, 256));
      byte[] unsupportedData = unsupportedSignature.getData();
      if (unsupportedData.length < 2 || unsupportedData[0] != 0x12 ||
          unsupportedData[1] != 0x03) {
        throw new IllegalStateException(
            "RSA-SHA1 should report class 0x12 / NO_SUCH_ALGORITHM (0x03)");
      }
      System.out.println("TEST:PASS jc_algtest_process_apdus version=" + versionText);
    } finally {
      card.disconnect(false);
    }
  }
}
