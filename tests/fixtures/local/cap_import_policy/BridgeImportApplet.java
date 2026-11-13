package capimportpolicy;

import fr.gouv.ssi.nativeimpl.NativeImplementation;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.APDU;

public final class BridgeImportApplet extends Applet {
    private BridgeImportApplet() {
        register();
    }

    public static void install(byte[] buffer, short offset, byte length) {
        new BridgeImportApplet();
    }

    public void process(APDU apdu) throws ISOException {
        if (selectingApplet()) return;
        NativeImplementation.getAID();
    }
}
