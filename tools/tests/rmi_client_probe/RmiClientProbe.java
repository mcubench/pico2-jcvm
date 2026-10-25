package rmi.fixture;

import com.sun.javacard.javax.smartcard.rmiclient.CardAccessor;
import com.sun.javacard.javax.smartcard.rmiclient.JavaCardRMIConnect;
import com.sun.javacard.ocfrmiclientimpl.JCCardProxyFactory;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Path;
import java.rmi.Remote;
import java.time.Duration;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/** Interoperability probe using the Java Card SDK's official RMI client. */
public final class RmiClientProbe {
  private static final byte[] APPLET_AID = hex("A00000015100000101");

  private static byte[] hex(String value) {
    byte[] result = new byte[value.length() / 2];
    for (int i = 0; i < result.length; i++) {
      result[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
    }
    return result;
  }

  private static final class VpcdAccessor implements CardAccessor, AutoCloseable {
    private final Socket socket;
    private final DataInputStream input;
    private final DataOutputStream output;

    VpcdAccessor(Socket socket) throws Exception {
      this.socket = socket;
      socket.setSoTimeout(10_000);
      input = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
      output = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
      send(new byte[] {1}); // VPCD power on
      send(new byte[] {4}); // VPCD ATR request
      byte[] atr = receive();
      if (atr.length != 4 || (atr[0] & 0xff) != 0x3b) {
        throw new IllegalStateException("Unexpected ATR: " + toHex(atr));
      }
    }

    private void send(byte[] message) throws Exception {
      output.writeShort(message.length);
      output.write(message);
      output.flush();
    }

    private byte[] receive() throws Exception {
      int length = input.readUnsignedShort();
      byte[] message = new byte[length];
      input.readFully(message);
      return message;
    }

    @Override
    public byte[] exchangeAPDU(byte[] command) throws java.io.IOException {
      try {
        byte[] wireCommand = command;
        send(wireCommand);
        byte[] response = receive();
        if (response.length < 2) throw new java.io.IOException("short R-APDU");
        byte[] clientResponse = new byte[response.length];
        clientResponse[0] = response[response.length - 2];
        clientResponse[1] = response[response.length - 1];
        System.arraycopy(response, 0, clientResponse, 2, response.length - 2);
        System.out.println("JCRMI_CLIENT:APDU command=" + toHex(command) +
                           " wire=" + toHex(wireCommand) + " response=" +
                           toHex(response) + " client=" + toHex(clientResponse));
        return clientResponse;
      } catch (Exception e) {
        throw new java.io.IOException("VPCD exchange failed", e);
      }
    }

    @Override
    public short getSessionIdentifier() {
      return 1;
    }

    @Override
    public void close() throws Exception {
      socket.close();
    }
  }

  private static String toHex(byte[] data) {
    StringBuilder text = new StringBuilder();
    for (byte value : data) text.append(String.format("%02X", value & 0xff));
    return text.toString();
  }

  private interface RemoteCall {
    void invoke() throws Throwable;
  }

  private static void expectRemoteException(String name, RemoteCall call)
      throws Exception {
    Throwable observed = null;
    try {
      call.invoke();
    } catch (Throwable exception) {
      observed = exception;
    }
    if (observed == null) {
      throw new AssertionError(name + " did not throw");
    }
    System.out.println("JCRMI_CLIENT:EXCEPTION name=" + name + " class=" +
                       observed.getClass().getName());
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 5) {
      throw new IllegalArgumentException(
          "usage: RmiClientProbe <emulator> <memory> <cap-code> <rom-mask> <log>");
    }
    Path emulator = Path.of(args[0]);
    Path memory = Path.of(args[1]);
    Path capCode = Path.of(args[2]);
    Path romMask = Path.of(args[3]);
    Path log = Path.of(args[4]);

    try (ServerSocket listener = new ServerSocket(
             0, 1, InetAddress.getByName("127.0.0.1"))) {
      // First-run ROM/APDU initialization currently takes about 12 seconds.
      // Leave bounded headroom so the client does not fail before VPCD connects.
      listener.setSoTimeout(60_000);
      String endpoint = "127.0.0.1:" + listener.getLocalPort();
      Process emulatorProcess = new ProcessBuilder(
          emulator.toString(), "-m", memory.toString(), "-s", "--vpcd", endpoint,
          "--applet-aid", toHex(APPLET_AID), "--package-id", "63", "--cap-code",
          capCode.toString(), "--rom-mask", romMask.toString(), "--install", "-s")
          .directory(new File("."))
          .redirectErrorStream(true)
          .redirectOutput(log.toFile())
          .start();

      try (Socket cardSocket = listener.accept();
           VpcdAccessor accessor = new VpcdAccessor(cardSocket)) {
        JCCardProxyFactory factory = new JCCardProxyFactory(accessor);
        factory.setINSByte((byte) 0x38);
        JavaCardRMIConnect connection = new JavaCardRMIConnect(accessor);
        connection.selectApplet(APPLET_AID, factory);
        Remote remote = connection.getInitialReference();
        if (!(remote instanceof Counter)) {
          throw new AssertionError("SDK client returned " + remote.getClass());
        }
        Counter counter = (Counter) remote;
        short result = counter.add((short) 3);
        if (result != 3) throw new AssertionError("add returned " + result);
        System.out.println("JCRMI_CLIENT:PASS add(short)=" + result);

        byte[] bytes = {0, 1, -1, 127};
        if (!Arrays.equals(bytes, counter.echoBytes(bytes)) ||
            counter.echoBytes(null) != null ||
            counter.sumBytes(bytes) != 127) {
          throw new AssertionError("byte[] RMI marshalling mismatch");
        }
        System.out.println("JCRMI_CLIENT:PASS byte[] parameter/return/null");

        short[] shorts = {0, 1, -1, 32767};
        if (!Arrays.equals(shorts, counter.echoShorts(shorts)) ||
            counter.echoShorts(null) != null) {
          throw new AssertionError("short[] RMI marshalling mismatch");
        }
        System.out.println("JCRMI_CLIENT:PASS short[] parameter/return/null");

        boolean[] flags = {true, false, false, true};
        if (!Arrays.equals(new boolean[] {false, true, true, false},
                           counter.invert(flags)) ||
            counter.invert(null) != null) {
          throw new AssertionError("boolean[] RMI marshalling mismatch");
        }
        System.out.println("JCRMI_CLIENT:PASS boolean[] parameter/return/null");

        int[] ints = {1, -2, 0x12345678};
        if (!Arrays.equals(ints, counter.echoInts(ints)) ||
            counter.echoInts(null) != null ||
            counter.sumInts(ints) != (short)(1 - 2 + 0x5678)) {
          throw new AssertionError("int[] RMI marshalling mismatch");
        }
        System.out.println("JCRMI_CLIENT:PASS int[] parameter/return/null");

        Counter child = counter.child();
        if (child == null || child.add((short) 7) != 7 || child.value() != 7) {
          throw new AssertionError("remote-interface return invocation mismatch");
        }
        System.out.println("JCRMI_CLIENT:PASS returned remote interface invocation");

        if (!counter.verifySensitiveResult()) {
          throw new AssertionError("SensitiveResult checks returned false");
        }
        System.out.println("JCRMI_CLIENT:PASS SensitiveResult same-context assertions");

        try {
          counter.throwCustomRuntime();
          throw new AssertionError("RuntimeException subclass was not reported");
        } catch (RuntimeException expected) {
          System.out.println("JCRMI_CLIENT:PASS runtime-subclass=" +
                             expected.getClass().getName());
        }

        try {
          counter.throwCustomChecked();
          throw new AssertionError("Exception subclass was not reported");
        } catch (Exception expected) {
          System.out.println("JCRMI_CLIENT:PASS checked-subclass=" +
                             expected.getClass().getName());
        }

        expectRemoteException("card", counter::throwCardReason);
        expectRemoteException("cardRuntime", counter::throwCardRuntimeReason);
        expectRemoteException("apdu", counter::throwApduReason);
        expectRemoteException("pin", counter::throwPinReason);
        expectRemoteException("system", counter::throwSystemReason);
        expectRemoteException("transaction", counter::throwTransactionReason);
        expectRemoteException("user", counter::throwUserReason);
        expectRemoteException("crypto", counter::throwCryptoReason);
        expectRemoteException("customCrypto", counter::throwCustomCryptoReason);
        System.out.println("JCRMI_CLIENT:SUMMARY pass=18 fail=0");
      } finally {
        if (emulatorProcess.isAlive()) {
          emulatorProcess.destroy();
          if (!emulatorProcess.waitFor(Duration.ofSeconds(5).toMillis(),
                                       TimeUnit.MILLISECONDS)) {
            emulatorProcess.destroyForcibly();
          }
        }
      }
    }
  }
}
