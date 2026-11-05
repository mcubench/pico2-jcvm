import apdu4j.pcsc.Readers;
import apdu4j.prefs.Preferences;
import apdu4j.core.BIBO;
import pro.javacard.capfile.AID;
import pro.javacard.capfile.CAPFile;
import pro.javacard.gp.GPException;
import pro.javacard.gp.GPSession;
import pro.javacard.gp.GPUtils;
import pro.javacard.gp.keys.PlaintextKeys;
import apdu4j.core.CommandAPDU;
import apdu4j.core.ResponseAPDU;

import java.io.ByteArrayOutputStream;
import java.nio.file.Path;
import java.util.EnumSet;

/** Test/development loader that retains the optional Descriptor CAP component. */
public final class LoadCapWithDescriptor {
  private static void requireOk(ResponseAPDU response, String operation)
      throws GPException {
    GPException.check(response, operation);
  }

  private static byte[] decodeHex(String hex) {
    if ((hex.length() & 1) != 0 || !hex.matches("[0-9A-Fa-f]*")) {
      throw new IllegalArgumentException("install parameters must be even-length hex");
    }
    byte[] result = new byte[hex.length() / 2];
    for (int i = 0; i < result.length; i++) {
      result[i] = (byte) Integer.parseInt(hex.substring(2 * i, 2 * i + 2), 16);
    }
    return result;
  }

  private static void load(GPSession gp, CAPFile cap, byte[] installParams)
      throws Exception {
    byte[] code = cap.getCode(true);
    AID packageAid = cap.getPackageAID();
    AID domainAid = gp.getAID();

    ByteArrayOutputStream params = new ByteArrayOutputStream();
    params.write(packageAid.getLength());
    params.writeBytes(packageAid.getBytes());
    params.write(domainAid.getLength());
    params.writeBytes(domainAid.getBytes());
    params.write(0); // no load-file-data hash
    params.writeBytes(GPUtils.encodeLength(0)); // no load parameters
    requireOk(gp.transmit(new CommandAPDU(GPSession.CLA_GP,
        GPSession.INS_INSTALL, GPSession.P1_INSTALL_FOR_LOAD, 0, params.toByteArray(), 256)),
        "INSTALL [for load]");

    ByteArrayOutputStream loadBlock = new ByteArrayOutputStream();
    loadBlock.writeBytes(new byte[] {(byte) 0xC4});
    loadBlock.writeBytes(GPUtils.encodeLength(code.length));
    loadBlock.writeBytes(code);
    var blocks = GPUtils.splitArray(loadBlock.toByteArray(), 220);
    for (int i = 0; i < blocks.size(); i++) {
      int p1 = i == blocks.size() - 1 ? 0x80 : 0x00;
      requireOk(gp.transmit(new CommandAPDU(GPSession.CLA_GP,
          GPSession.INS_LOAD, p1, i & 0xff, blocks.get(i), 256)), "LOAD");
    }

    var applets = cap.getAppletAIDs();
    if (applets.size() != 1) {
      throw new IllegalArgumentException("expected exactly one applet, found " + applets.size());
    }
    gp.installAndMakeSelectable(packageAid, applets.get(0), applets.get(0),
        EnumSet.of(pro.javacard.gp.GPRegistryEntry.Privilege.CardReset),
        installParams);
    System.out.printf("DESCRIPTOR_LOAD:PASS package=%s applet=%s code_bytes=%d blocks=%d%n",
        packageAid, applets.get(0), code.length, blocks.size());
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2 && args.length != 3) {
      throw new IllegalArgumentException("usage: LoadCapWithDescriptor <exact reader name> <CAP file> [install-parameters-hex]");
    }
    String reader = args[0];
    CAPFile cap = CAPFile.fromFile(Path.of(args[1]));
    byte[] installParams = args.length == 3 ? decodeHex(args[2]) : null;
    Readers.select(reader).transactions(true).reset(false).run((BIBO channel) -> {
      try {
        GPSession gp = GPSession.discover(channel);
        gp.openSecureChannel(PlaintextKeys.defaultKey(), null, null,
            GPSession.defaultMode.clone());
        load(gp, cap, installParams);
        return null;
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    });
  }
}
