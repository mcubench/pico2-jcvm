package rmi.service;

import javacard.framework.APDU;
import javacard.framework.Applet;

public final class ServiceApplet extends Applet {
  private ServiceApplet() {
    new CounterImpl();
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ServiceApplet();
  }

  public void process(APDU apdu) {}
}
