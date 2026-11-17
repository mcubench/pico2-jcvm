package algtest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;

/** Fill the normal persistent object store with rooted arrays for DELETE tests. */
public final class FilesystemCapacityProbe extends Applet {
  private static final byte MAX_CHUNKS = 3;
  private static final short CHUNK_BYTES = 30000;
  private static Object[] retained;
  private static short[] memoryWords;
  private byte allocated;

  private FilesystemCapacityProbe() {
    if (retained == null) retained = new Object[MAX_CHUNKS];
    if (memoryWords == null) memoryWords = new short[2];
    register();
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    new FilesystemCapacityProbe();
  }

  private short appendMemory(byte[] buffer, short offset, byte type) {
    short scalar = JCSystem.getAvailableMemory(type);
    JCSystem.getAvailableMemory(memoryWords, (short)0, type);
    buffer[offset++] = (byte)(scalar >> 8);
    buffer[offset++] = (byte)scalar;
    buffer[offset++] = (byte)(memoryWords[0] >> 8);
    buffer[offset++] = (byte)memoryWords[0];
    buffer[offset++] = (byte)(memoryWords[1] >> 8);
    buffer[offset++] = (byte)memoryWords[1];
    return offset;
  }

  public void process(APDU apdu) throws ISOException {
    if (selectingApplet()) return;
    byte[] buffer = apdu.getBuffer();
    byte command = buffer[ISO7816.OFFSET_P1];
    if (command == 1) {
      if (allocated >= MAX_CHUNKS) {
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
      }
      byte[] chunk = new byte[CHUNK_BYTES];
      chunk[0] = (byte)(allocated + 1);
      chunk[(short)(CHUNK_BYTES - 1)] = (byte)0x5a;
      retained[allocated] = chunk;
      allocated++;
    } else if (command == 2) {
      for (byte index = 0; index < allocated; index++) {
        byte[] chunk = (byte[])retained[index];
        if (chunk == null || chunk.length != CHUNK_BYTES ||
            chunk[0] != (byte)(index + 1) ||
            chunk[(short)(CHUNK_BYTES - 1)] != (byte)0x5a) {
          ISOException.throwIt(ISO7816.SW_DATA_INVALID);
        }
      }
    } else {
      ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
    }

    buffer[0] = allocated;
    short offset = 1;
    offset = appendMemory(buffer, offset, JCSystem.MEMORY_TYPE_PERSISTENT);
    offset = appendMemory(buffer, offset, JCSystem.MEMORY_TYPE_TRANSIENT_RESET);
    apdu.setOutgoingAndSend((short)0, offset);
  }
}
