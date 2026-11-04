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

    private static boolean checkIntWrapping(int left, int right,
                                            int sum, int difference,
                                            int product, int negated) {
        return left + right == sum && left - right == difference &&
               left * right == product && -left == negated;
    }

    private static boolean checkShortWrapping(short left, short right,
                                              short sum, short difference,
                                              short product, short negated) {
        return (short) (left + right) == sum &&
               (short) (left - right) == difference &&
               (short) (left * right) == product && (short) -left == negated;
    }

    private static boolean checkIntShifts(int value, int distance,
                                          int left, int arithmeticRight,
                                          int logicalRight) {
        return value << distance == left && value >> distance == arithmeticRight &&
               value >>> distance == logicalRight;
    }

    private static boolean checkShortShifts(short value, short distance,
                                            short left, short arithmeticRight,
                                            short logicalRight) {
        return (short) (value << distance) == left &&
               (short) (value >> distance) == arithmeticRight &&
               (short) (value >>> distance) == logicalRight;
    }

    private static boolean checkLocalIncrements() {
        int intValue = Integer.MAX_VALUE;
        intValue++;
        short shortValue = Short.MAX_VALUE;
        shortValue++;
        return intValue == Integer.MIN_VALUE && shortValue == Short.MIN_VALUE;
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
        if (!checkIntWrapping(Integer.MAX_VALUE, 1, Integer.MIN_VALUE,
                              Integer.MAX_VALUE - 1, Integer.MAX_VALUE,
                              -Integer.MAX_VALUE)) ISOException.throwIt((short) 0x6F02);
        if (!checkIntWrapping(Integer.MIN_VALUE, 1, Integer.MIN_VALUE + 1,
                              Integer.MAX_VALUE, Integer.MIN_VALUE,
                              Integer.MIN_VALUE)) ISOException.throwIt((short) 0x6F03);
        if (!checkShortWrapping(Short.MAX_VALUE, (short) 1, Short.MIN_VALUE,
                                (short) (Short.MAX_VALUE - 1), Short.MAX_VALUE,
                                (short) -Short.MAX_VALUE)) ISOException.throwIt((short) 0x6F04);
        if (!checkShortWrapping(Short.MIN_VALUE, (short) 1,
                                (short) (Short.MIN_VALUE + 1), Short.MAX_VALUE,
                                Short.MIN_VALUE, Short.MIN_VALUE)) ISOException.throwIt((short) 0x6F05);
        if (!checkIntShifts(-1, 0, -1, -1, -1)) ISOException.throwIt((short) 0x6F06);
        if (!checkIntShifts(-1, 1, -2, -1, 0x7fffffff)) ISOException.throwIt((short) 0x6F07);
        if (!checkIntShifts(1, 31, Integer.MIN_VALUE, 0, 0)) ISOException.throwIt((short) 0x6F08);
        if (!checkIntShifts(1, 32, 1, 1, 1)) ISOException.throwIt((short) 0x6F09);
        if (!checkShortShifts((short) -1, (short) 0, (short) -1,
                              (short) -1, (short) -1)) ISOException.throwIt((short) 0x6F0A);
        if (!checkShortShifts((short) -1, (short) 1, (short) -2,
                              (short) -1, (short) -1)) ISOException.throwIt((short) 0x6F0B);
        if (!checkShortShifts((short) -1, (short) 16, (short) 0,
                              (short) -1, (short) -1)) ISOException.throwIt((short) 0x6F0C);
        if (!checkShortShifts((short) -1, (short) 31, (short) 0,
                              (short) -1, (short) 1)) ISOException.throwIt((short) 0x6F0D);
        if (!checkIntShifts(1, -1, Integer.MIN_VALUE, 0, 0)) ISOException.throwIt((short) 0x6F0E);
        if (!checkShortShifts((short) -1, (short) -1, (short) 0,
                              (short) -1, (short) 1)) ISOException.throwIt((short) 0x6F0F);
        if (!checkLocalIncrements()) ISOException.throwIt((short) 0x6F10);

        buffer[0] = 1;
        apdu.setOutgoingAndSend((short) 0, (short) 1);
    }
}
