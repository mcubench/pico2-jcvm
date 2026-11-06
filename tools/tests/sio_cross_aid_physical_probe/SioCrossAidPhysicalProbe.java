import javax.smartcardio.Card;
import javax.smartcardio.CardChannel;
import javax.smartcardio.CardTerminal;
import javax.smartcardio.CommandAPDU;
import javax.smartcardio.ResponseAPDU;
import javax.smartcardio.TerminalFactory;
import java.util.List;

/** Exact-reader physical regression for cross-AID Shareable dispatch. */
public final class SioCrossAidPhysicalProbe {
  private static final byte[] APPLET_AID = hex("A00000015100000401");

  private static byte[] hex(String value) {
    byte[] bytes = new byte[value.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
    }
    return bytes;
  }

  private static String hex(byte[] bytes) {
    StringBuilder out = new StringBuilder();
    for (byte b : bytes) out.append(String.format("%02X", b & 0xff));
    return out.toString();
  }

  public static void main(String[] args) throws Exception {
    if (args.length < 1 || args.length > 2) {
      throw new IllegalArgumentException(
          "reader name and optional --after-delete/--after-reinstall required");
    }
    boolean afterDelete = args.length == 2 && "--after-delete".equals(args[1]);
    boolean afterReinstall = args.length == 2 &&
        "--after-reinstall".equals(args[1]);
    if (args.length == 2 && !afterDelete && !afterReinstall) {
      throw new IllegalArgumentException("unknown probe mode: " + args[1]);
    }
    List<CardTerminal> terminals = TerminalFactory.getDefault().terminals().list();
    CardTerminal target = terminals.stream().filter(t -> t.getName().equals(args[0]))
        .findFirst().orElseThrow(() -> new IllegalStateException("reader not found: " + args[0]));
    System.out.println("SIO_PHYSICAL:READER " + target.getName());
    Card card = target.connect("*");
    // GPPro's LOAD/INSTALL session may leave SCP03 active until card reset.
    // This probe sends plain applet APDUs, so reset between management and
    // test traffic to avoid correctly receiving 6982 from route authorization.
    card.disconnect(true);
    card = target.connect("*");
    try {
      System.out.println("SIO_PHYSICAL:ATR " + hex(card.getATR().getBytes()));
      CardChannel channel = card.getBasicChannel();
      ResponseAPDU selected = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      System.out.println("SIO_PHYSICAL:SELECT response=" + hex(selected.getBytes()));
      if (selected.getSW() != 0x9000) throw new AssertionError("SELECT failed");

      if (afterReinstall) {
        ResponseAPDU response = channel.transmit(
            new CommandAPDU(hex("8016000000")));
        String actual = hex(response.getBytes());
        System.out.println("SIO_PHYSICAL:staleProxyAfterReinstall response=" + actual);
        if (!actual.equals("6A0A9000")) {
          throw new AssertionError("stale proxy after same-AID reinstall should " +
                                   "raise catchable SecurityException 6A0A9000, got " + actual);
        }
        response = channel.transmit(new CommandAPDU(hex("8010000000")));
        actual = hex(response.getBytes());
        System.out.println("SIO_PHYSICAL:freshLookupAfterReinstall response=" + actual);
        if (!actual.equals("45679000")) {
          throw new AssertionError("fresh lookup after reinstall should return " +
                                   "45679000, got " + actual);
        }
        System.out.println("SIO_PHYSICAL:PASS stale_proxy_after_reinstall fresh_lookup");
        return;
      }

      if (afterDelete) {
        ResponseAPDU response = channel.transmit(
            new CommandAPDU(hex("8016000000")));
        String actual = hex(response.getBytes());
        System.out.println("SIO_PHYSICAL:staleProxyAfterDelete response=" + actual);
        if (!actual.equals("6A0A9000")) {
          throw new AssertionError("stale proxy after DELETE should raise " +
                                   "catchable SecurityException 6A0A9000, got " + actual);
        }
        response = channel.transmit(new CommandAPDU(hex("8010000000")));
        actual = hex(response.getBytes());
        System.out.println("SIO_PHYSICAL:lookupAfterDelete response=" + actual);
        if (!actual.equals("6A049000")) {
          throw new AssertionError("fresh lookup after DELETE should return " +
                                   "6A049000, got " + actual);
        }
        System.out.println("SIO_PHYSICAL:PASS stale_proxy_after_delete fresh_lookup");
        return;
      }

      ResponseAPDU response = channel.transmit(
          new CommandAPDU(hex("8010000000")));
      String actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:crossAidShareable response=" + actual);
      if (!actual.equals("45679000")) {
        throw new AssertionError("expected 45679000, got " + actual);
      }
      System.out.println("SIO_PHYSICAL:PASS cross_aid_shareable");

      response = channel.transmit(new CommandAPDU(hex("803C000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:JCSystemSensitiveResults response=" + actual);
      if (!actual.equals("6A8B9000")) {
        throw new AssertionError("JCSystem lookupAID and " +
                                 "getAppletShareableInterfaceObject " +
                                 "SensitiveResult assertions should pass " +
                                 "with 6A8B9000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("801A000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:privateArrayFirewall response=" + actual);
      if (!actual.equals("5A319000")) {
        throw new AssertionError("expected caught SecurityException " +
                                 "5A319000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("801F000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:nativeUtilForeignArray response=" + actual);
      if (!actual.equals("6A269000")) {
        throw new AssertionError("native Util arrayCopy on foreign array " +
                                 "should raise catchable SecurityException " +
                                 "6A269000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("801B000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:globalObjectArrayFirewall response=" + actual);
      if (!actual.equals("6B339000")) {
        throw new AssertionError("expected global identity check and caught " +
                                 "field/array retention denials 6B339000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8020000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:foreignReferenceRetention response=" + actual);
      if (!actual.equals("6B439000")) {
        throw new AssertionError("expected applet-owned volatile wildcard " +
                                 "Object[] retention with static/instance " +
                                 "stores denied (6B439000), got " + actual);
      }
      response = channel.transmit(new CommandAPDU(hex("802E000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:foreignArrayAfterGc response=" + actual);
      if (!actual.equals("6A2E9000")) {
        throw new AssertionError("foreign byte array was not retained across " +
                                 "client root clearing and GC (6A2E9000), got " +
                                 actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8015000000")));
      actual = hex(response.getBytes());
      if (!actual.equals("6A109000")) {
        throw new AssertionError("proxy retain expected 6A109000, got " + actual);
      }
      card.disconnect(true);
      card = target.connect("*");
      System.out.println("SIO_PHYSICAL:RESET_ATR " + hex(card.getATR().getBytes()));
      channel = card.getBasicChannel();
      selected = channel.transmit(new CommandAPDU(
          0x00, 0xA4, 0x04, 0x00, APPLET_AID, 256));
      if (selected.getSW() != 0x9000) {
        throw new AssertionError("client reselect after reset failed: " +
                                 hex(selected.getBytes()));
      }
      response = channel.transmit(new CommandAPDU(hex("8016000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:staleProxyAfterReset response=" + actual);
      if (!actual.equals("6A0A9000")) {
        throw new AssertionError("stale proxy should fail closed with 6A0A9000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8013000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:intArgumentReturn response=" + actual);
      if (!actual.equals("765432109000")) {
        throw new AssertionError("expected 765432109000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8014000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:exceptionUnwind response=" + actual);
      if (!actual.equals("6A079000")) {
        throw new AssertionError("expected 6A079000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8010000000")));
      actual = hex(response.getBytes());
      if (!actual.equals("45679000")) {
        throw new AssertionError("post-exception expected 45679000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8011000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:deniedLookup response=" + actual);
      if (!actual.equals("6A019000")) {
        throw new AssertionError("expected 6A019000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8012000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:opaqueNonNullReference response=" + actual);
      if (!actual.equals("6A059000")) {
        throw new AssertionError("opaque non-null reference should cross for " +
                                 "null comparison, expected 6A059000, got " + actual);
      }

      response = channel.transmit(new CommandAPDU(hex("8017000000")));
      actual = hex(response.getBytes());
      System.out.println("SIO_PHYSICAL:proxyEquals response=" + actual);
      if (!actual.equals("6A099000")) {
        throw new AssertionError("expected 6A099000, got " + actual);
      }
      System.out.println("SIO_PHYSICAL:PASS int_argument_return exception_unwind " +
                         "context_restore denied_lookup local_reference_firewall " +
                         "proxy_method_firewall");
    } finally {
      card.disconnect(false);
    }
  }
}
