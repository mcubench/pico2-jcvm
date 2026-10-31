package rmi.contract;

import java.rmi.Remote;
import java.rmi.RemoteException;

public interface Counter extends Remote {
  short value() throws RemoteException;
}
