import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import java.util.List;
import java.util.Arrays;

/** Send bounded, applet-independent GlobalPlatform GET DATA probes. */
public final class GPGetDataApduProbe {
  private static final byte[] STATIC_KEY =
      fromHex("404142434445464748494A4B4C4D4E4F");
  private static final byte[] CARD_DATA =
      fromHex("6618731606072A864886FC6B01640B06092A864886FC6B040310");
  private static final byte[] CPLC_DATA = fromHex(
      "9F7F2A096E23504A43630400016304000000000000096E6304096E6304" +
      "096E630400000000096E630400000000");

  private static byte[] fromHex(String text) {
    String compact = text.replaceAll("\\s", "");
    byte[] bytes = new byte[compact.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(compact.substring(i * 2, i * 2 + 2), 16);
    }
    return bytes;
  }

  private static String hex(byte[] bytes) {
    StringBuilder text = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      text.append(String.format("%02X", value & 0xff));
    }
    return text.toString();
  }

  private static ResponseAPDU transmit(CardChannel channel, String name,
                                       CommandAPDU command) throws Exception {
    ResponseAPDU response = channel.transmit(command);
    System.out.printf("APDU:%s command=%s response=%s sw=%04X%n", name,
        hex(command.getBytes()), hex(response.getBytes()), response.getSW());
    return response;
  }

  private static byte[] aesBlock(Cipher cipher, byte[] input) throws Exception {
    return cipher.doFinal(input);
  }

  private static byte[] shiftBlock(byte[] input) {
    byte[] output = new byte[16];
    int carry = 0;
    for (int index = 15; index >= 0; index--) {
      int value = input[index] & 0xff;
      output[index] = (byte)((value << 1) | carry);
      carry = value >>> 7;
    }
    if (carry != 0) output[15] ^= (byte)0x87;
    return output;
  }

  private static byte[] xor(byte[] left, byte[] right) {
    byte[] output = new byte[left.length];
    for (int index = 0; index < left.length; index++) {
      output[index] = (byte)(left[index] ^ right[index]);
    }
    return output;
  }

  private static byte[] aesCmac(byte[] key, byte[] message) throws Exception {
    Cipher cipher = Cipher.getInstance("AES/ECB/NoPadding");
    cipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"));
    byte[] firstSubkey = shiftBlock(aesBlock(cipher, new byte[16]));
    byte[] secondSubkey = shiftBlock(firstSubkey);
    int blockCount = Math.max(1, (message.length + 15) / 16);
    boolean completeLast = message.length != 0 && message.length % 16 == 0;
    byte[] state = new byte[16];

    for (int block = 0; block < blockCount - 1; block++) {
      byte[] input = Arrays.copyOfRange(message, block * 16, block * 16 + 16);
      state = aesBlock(cipher, xor(state, input));
    }

    int lastOffset = (blockCount - 1) * 16;
    int lastLength = Math.max(0, message.length - lastOffset);
    byte[] last = new byte[16];
    if (lastLength != 0) {
      System.arraycopy(message, lastOffset, last, 0, lastLength);
    }
    if (completeLast) {
      last = xor(last, firstSubkey);
    } else {
      last[lastLength] = (byte)0x80;
      last = xor(last, secondSubkey);
    }
    return aesBlock(cipher, xor(state, last));
  }

  private static byte[] kdf(byte[] key, int label, byte[] context,
                            int outputBits) throws Exception {
    byte[] input = new byte[32];
    input[11] = (byte)label;
    input[13] = (byte)(outputBits >>> 8);
    input[14] = (byte)outputBits;
    input[15] = 1;
    System.arraycopy(context, 0, input, 16, 16);
    return aesCmac(key, input);
  }

  private static byte[] concat(byte[]... parts) {
    int length = 0;
    for (byte[] part : parts) length += part.length;
    byte[] output = new byte[length];
    int offset = 0;
    for (byte[] part : parts) {
      System.arraycopy(part, 0, output, offset, part.length);
      offset += part.length;
    }
    return output;
  }

  private static byte[] firstEight(byte[] input) {
    return Arrays.copyOf(input, 8);
  }

  private static byte[] establishScp03(CardChannel channel) throws Exception {
    byte[] hostChallenge = new byte[8];
    new java.security.SecureRandom().nextBytes(hostChallenge);
    ResponseAPDU init = transmit(channel, "SCP03_INITIALIZE_UPDATE",
        new CommandAPDU(0x80, 0x50, 0x00, 0x00, hostChallenge, 256));
    byte[] initData = init.getData();
    if (init.getSW() != 0x9000 || initData.length != 29 ||
        initData[11] != 0x03) {
      throw new IllegalStateException("SCP03 INITIALIZE UPDATE response invalid");
    }

    byte[] context = concat(hostChallenge, Arrays.copyOfRange(initData, 13, 21));
    byte[] sessionMacKey = kdf(STATIC_KEY, 0x06, context, 128);
    byte[] expectedCardCryptogram = firstEight(
        kdf(sessionMacKey, 0x00, context, 64));
    if (!Arrays.equals(expectedCardCryptogram,
                       Arrays.copyOfRange(initData, 21, 29))) {
      throw new IllegalStateException("SCP03 card cryptogram mismatch");
    }
    byte[] hostCryptogram = firstEight(kdf(sessionMacKey, 0x01, context, 64));
    byte[] header = fromHex("8482010010");
    byte[] macInput = concat(new byte[16], Arrays.copyOf(header, 4),
                             new byte[] {0x10}, hostCryptogram);
    byte[] externalAuthMac = aesCmac(sessionMacKey, macInput);
    byte[] externalAuth = concat(header, hostCryptogram,
                                 firstEight(externalAuthMac), new byte[] {0x00});
    ResponseAPDU auth = transmit(channel, "SCP03_EXTERNAL_AUTHENTICATE",
                                 new CommandAPDU(externalAuth));
    if (auth.getSW() != 0x9000) {
      throw new IllegalStateException("SCP03 EXTERNAL AUTHENTICATE failed");
    }
    System.out.println("SCP03:AUTHENTICATED level=C-MAC");
    return concat(sessionMacKey, externalAuthMac);
  }

  private static ResponseAPDU transmitProtected(CardChannel channel,
                                                 byte[] sessionState,
                                                 String name,
                                                 CommandAPDU command,
                                                 byte[] expectedData)
      throws Exception {
    byte[] sessionMacKey = Arrays.copyOfRange(sessionState, 0, 16);
    byte[] chainingValue = Arrays.copyOfRange(sessionState, 16, 32);
    byte[] plain = command.getBytes();
    byte[] data = plain.length > 5
        ? Arrays.copyOfRange(plain, 5, plain.length - 1) : new byte[0];
    int lc = data.length + 8;
    byte[] header = { (byte)0x84, plain[1], plain[2], plain[3], (byte)lc };
    byte[] mac = aesCmac(sessionMacKey,
        concat(chainingValue, Arrays.copyOf(header, 4),
               new byte[] { (byte)lc }, data));
    System.arraycopy(mac, 0, sessionState, 16, mac.length);
    byte[] protectedCommand = concat(header, data, firstEight(mac),
                                     new byte[] {0x00});
    ResponseAPDU response = transmit(channel, name,
        new CommandAPDU(protectedCommand));
    if (response.getSW() != 0x9000 ||
        !Arrays.equals(response.getData(), expectedData)) {
      throw new IllegalStateException("authenticated GET DATA response mismatch");
    }
    return response;
  }

  public static void main(String[] args) throws Exception {
    String requestedReader = args.length == 0 ? null : args[0];
    List<CardTerminal> terminals =
        TerminalFactory.getDefault().terminals().list();
    CardTerminal terminal = null;
    for (CardTerminal candidate : terminals) {
      System.out.println("PCSC:READER " + candidate.getName());
      if (requestedReader == null
          ? candidate.getName().contains("JCVM CCID")
          : candidate.getName().equals(requestedReader)) {
        terminal = candidate;
      }
    }
    if (terminal == null) {
      throw new IllegalStateException("requested PC/SC reader not found: " +
          requestedReader == null ? "JCVM CCID" : requestedReader);
    }

    Card card = terminal.connect("*");
    try {
      System.out.println("PCSC:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU manager = transmit(channel, "SELECT_DEFAULT_ISD",
          new CommandAPDU(0x00, 0xA4, 0x04, 0x00, new byte[0], 256));
      if (manager.getSW() != 0x9000) {
        throw new IllegalStateException("default ISD SELECT failed");
      }

      ResponseAPDU cardData = transmit(channel, "GET_DATA_CARD_RECOGNITION",
          new CommandAPDU(0x80, 0xCA, 0x00, 0x66, 256));
      String expected = "6618731606072A864886FC6B01640B06092A864886FC6B040310";
      if (cardData.getSW() != 0x9000 || !expected.equals(hex(cardData.getData()))) {
        throw new IllegalStateException("unexpected Card Recognition Data response");
      }
      ResponseAPDU cardDataValue = transmit(channel,
          "ISO_GET_DATA_CARD_RECOGNITION_VALUE",
          new CommandAPDU(0x00, 0xCB, 0x00, 0x66, 256));
      if (cardDataValue.getSW() != 0x9000 ||
          !expected.substring(4).equals(hex(cardDataValue.getData()))) {
        throw new IllegalStateException("unexpected ISO Card Data value");
      }

      ResponseAPDU cplc = transmit(channel, "GET_DATA_CPLC",
          new CommandAPDU(0x80, 0xCA, 0x9F, 0x7F, 256));
      if (cplc.getSW() != 0x9000 ||
          !Arrays.equals(cplc.getData(), CPLC_DATA)) {
        throw new IllegalStateException("unexpected CPLC response");
      }
      ResponseAPDU cplcValue = transmit(channel, "ISO_GET_DATA_CPLC_VALUE",
          new CommandAPDU(0x00, 0xCB, 0x9F, 0x7F, 256));
      if (cplcValue.getSW() != 0x9000 ||
          !Arrays.equals(cplcValue.getData(),
                         Arrays.copyOfRange(CPLC_DATA, 3, CPLC_DATA.length))) {
        throw new IllegalStateException("unexpected ISO CPLC value");
      }
      byte[] expectedApplications = fromHex(
          "610A4F08A000000151000000");
      ResponseAPDU applications = transmit(channel, "GET_DATA_APPLICATIONS",
          new CommandAPDU(0x80, 0xCA, 0x2F, 0x00,
                          new byte[] {0x5c, 0x00}, 256));
      if (applications.getSW() != 0x9000 ||
          !Arrays.equals(applications.getData(), expectedApplications)) {
        throw new IllegalStateException("unexpected GP application list");
      }
      ResponseAPDU applicationsIso = transmit(channel,
          "ISO_GET_DATA_APPLICATIONS",
          new CommandAPDU(0x00, 0xCB, 0x2F, 0x00,
                          new byte[] {0x5c, 0x00}, 256));
      if (applicationsIso.getSW() != 0x9000 ||
          !Arrays.equals(applicationsIso.getData(), expectedApplications)) {
        throw new IllegalStateException("unexpected ISO application list");
      }
      byte[] session = establishScp03(channel);
      transmitProtected(channel, session, "GET_DATA_CARD_RECOGNITION_AUTHENTICATED",
          new CommandAPDU(0x80, 0xCA, 0x00, 0x66, 256), CARD_DATA);
      transmitProtected(channel, session, "GET_DATA_CPLC_AUTHENTICATED",
          new CommandAPDU(0x80, 0xCA, 0x9F, 0x7F, 256), CPLC_DATA);
      transmitProtected(channel, session, "GET_DATA_APPLICATIONS_AUTHENTICATED",
          new CommandAPDU(0x80, 0xCA, 0x2F, 0x00,
                          new byte[] {0x5c, 0x00}, 256),
          expectedApplications);
      System.out.println("TEST:PASS gp_get_data_open_and_authenticated");
    } finally {
      card.disconnect(false);
    }
  }
}
