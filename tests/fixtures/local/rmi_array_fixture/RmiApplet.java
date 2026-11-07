package rmi.fixture;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.service.Dispatcher;
import javacard.framework.service.RMIService;

public final class RmiApplet extends Applet {
  private final Dispatcher dispatcher;

  private RmiApplet() {
    RMIService service = new RMIService(new CounterImpl());
    dispatcher = new Dispatcher((short)1);
    dispatcher.addService(service, Dispatcher.PROCESS_COMMAND);
    register();
  }

  public static void install(byte[] buffer, short offset, byte length)
      throws ISOException {
    new RmiApplet();
  }

  public void process(APDU apdu) throws ISOException {
    dispatcher.process(apdu);
  }
}
