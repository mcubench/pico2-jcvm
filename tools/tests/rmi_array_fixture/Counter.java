package rmi.fixture;

import java.rmi.Remote;
import java.rmi.RemoteException;

public interface Counter extends Remote {
  short add(short amount) throws RemoteException;
  short value() throws RemoteException;
  boolean verifySensitiveResult() throws RemoteException;
  Counter child() throws RemoteException;
  short allocationPressure() throws RemoteException;
  byte[] echoBytes(byte[] value) throws RemoteException;
  short sumBytes(byte[] value) throws RemoteException;
  short sumEight(byte[] a, byte[] b, byte[] c, byte[] d, byte[] e,
                 byte[] f, byte[] g, byte[] h) throws RemoteException;
  short[] echoShorts(short[] value) throws RemoteException;
  int[] echoInts(int[] value) throws RemoteException;
  short sumInts(int[] value) throws RemoteException;
  boolean[] invert(boolean[] value) throws RemoteException;
  byte arrayAt(byte[] value, short index) throws RemoteException;
  byte arrayAtOrDefault(byte[] value, short index) throws RemoteException;
  short invalidMemoryType() throws RemoteException;
  void throwIso() throws RemoteException;
  void throwCustomRuntime() throws RemoteException;
  void throwCustomChecked() throws CustomCheckedException, RemoteException;
}
