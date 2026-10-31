package rmi.service;

import javacard.framework.service.CardRemoteObject;
import rmi.contract.Counter;

public final class CounterImpl extends CardRemoteObject implements Counter {
  public short value() {
    return 7;
  }
}
