package divisionprobe;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;

public final class DivisionProbe extends Applet {
    private DivisionProbe() {
        register();
    }

    public static void install(byte[] data, short offset, byte length) {
        new DivisionProbe();
    }

    private static boolean checkInt(int dividend, int divisor,
                                    int quotient, int remainder) {
        return dividend / divisor == quotient && dividend % divisor == remainder;
    }

    private static boolean checkShort(short dividend, short divisor,
                                      short quotient, short remainder) {
        return (short) (dividend / divisor) == quotient &&
               (short) (dividend % divisor) == remainder;
    }

    private static boolean intDivideByZeroThrows(int divisor) {
        try {
            int ignored = 7 / divisor;
            return false;
        } catch (ArithmeticException expected) {
            return true;
        }
    }

    private static boolean shortDivideByZeroThrows(short divisor) {
        try {
            short ignored = (short) (7 / divisor);
            return false;
        } catch (ArithmeticException expected) {
            return true;
        }
    }

    public void process(APDU apdu) {
        if (selectingApplet()) return;
        byte[] buffer = apdu.getBuffer();
        if (buffer[ISO7816.OFFSET_INS] != (byte) 0x01) {
            ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
        }

        boolean valid = true;
        valid &= checkInt(Integer.MIN_VALUE, -1, Integer.MIN_VALUE, 0);
        valid &= checkInt(-7, 2, -3, -1);
        valid &= checkInt(7, -2, -3, 1);
        valid &= checkInt(-7, -2, 3, -1);
        valid &= checkShort(Short.MIN_VALUE, (short) -1, Short.MIN_VALUE, (short) 0);
        valid &= checkShort((short) -7, (short) 2, (short) -3, (short) -1);
        valid &= checkShort((short) 7, (short) -2, (short) -3, (short) 1);
        valid &= checkShort((short) -7, (short) -2, (short) 3, (short) -1);
        valid &= intDivideByZeroThrows(0);
        valid &= shortDivideByZeroThrows((short) 0);
        if (!valid) ISOException.throwIt((short) 0x6F01);

        buffer[0] = 1;
        apdu.setOutgoingAndSend((short) 0, (short) 1);
    }
}
