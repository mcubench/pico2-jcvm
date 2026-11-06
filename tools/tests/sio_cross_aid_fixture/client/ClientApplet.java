package sio.client;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.security.CryptoException;
import javacardx.security.SensitiveResult;
import sio.contract.Probe;

public final class ClientApplet extends Applet {
  private static final byte[] SERVICE_AID = {
    (byte) 0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x03, 0x01
  };
  // Keep the alias field first so restoration exercises the persistent object
  // alias path before the canonical field has loaded its target reference.
  private Shareable retainedShareableAlias;
  private Shareable retainedShareable;
  private static Object[] retainedGlobalStatic;
  private Object[] retainedGlobalInstance;

  private ClientApplet() {
    register();
  }

  public static void install(byte[] buffer, short offset, byte length) {
    new ClientApplet();
  }

  private short checkSensitiveResult(Probe probe) {
    short copied = probe.sensitiveCopy();
    try {
      SensitiveResult.assertEquals(copied);
      return copied == 2 ? (short)0x6A28 : (short)0x6A29;
    } catch (SecurityException missingSensitiveResult) {
      return (short)0x6A2A;
    }
  }

  private short checkFailedSensitiveResult(Probe probe) {
    short result = probe.sensitiveFailedCopy();
    if (result != (short)0x0123) return (short)0x6A2C;
    try {
      SensitiveResult.assertEquals((short)0x0123);
      return (short)0x6A2D;
    } catch (SecurityException correctlyUnassigned) {
      return (short)0x6A2B;
    }
  }

  private short checkSensitiveAIDResults(Probe probe) {
    return probe.sensitiveAIDResults();
  }

  private short checkSensitiveGlobalArrayResult(Probe probe) {
    byte[] value = probe.sensitiveGlobalArrayResult();
    if (value == null || value.length != 1 ||
        value[0] != (byte) 0x6D) {
      return (short) 0x6A71;
    }
    try {
      SensitiveResult.assertEquals(value);
      return (short) 0x6A70;
    } catch (SecurityException missingSensitiveResult) {
      return (short) 0x6A72;
    }
  }

  private short checkPreservedSensitiveAID(Probe probe) {
    AID own = JCSystem.getAID();
    short marker = probe.noSensitiveApi();
    if (marker != (short)0x6A33) return (short)0x6A34;
    try {
      SensitiveResult.assertEquals(own);
      return (short)0x6A35;
    } catch (SecurityException resultWasNotPreserved) {
      return (short)0x6A36;
    }
  }

  private short checkSioExceptionPropagation(Probe probe) {
    try {
      probe.throwCryptoException();
      return (short) 0x6A60;
    } catch (CryptoException expected) {
      if (expected.getReason() != (short) 0x1234) return (short) 0x6A61;
    } catch (SecurityException substitutedException) {
      return (short) 0x6A62;
    }

    try {
      probe.throwArrayIndexOutOfBoundsException();
      return (short) 0x6A63;
    } catch (ArrayIndexOutOfBoundsException expected) {
      // Preserve the concrete subclass through the SIO boundary.
    } catch (IndexOutOfBoundsException wrongSubclass) {
      return (short) 0x6A64;
    } catch (SecurityException substitutedException) {
      return (short) 0x6A65;
    }

    return probe.exerciseWide(0x12345678) == 0x76543210
        ? (short) 0x6A66 : (short) 0x6A67;
  }

  private short checkSensitiveDigestResult(Probe probe) {
    short outputLength = probe.sensitiveDigestResult();
    try {
      SensitiveResult.assertEquals(outputLength);
      return outputLength == 32 ? (short)0x6A37 : (short)0x6A38;
    } catch (SecurityException missingSensitiveResult) {
      return (short)0x6A39;
    }
  }

  private short checkFailedSensitiveDigest(Probe probe) {
    AID own = JCSystem.getAID();
    short marker = probe.sensitiveDigestFailure();
    if (marker != (short)0x6A3D) return (short)0x6A3E;
    try {
      SensitiveResult.assertEquals(own);
      return (short)0x6A3F;
    } catch (SecurityException correctlyUnassigned) {
      return (short)0x6A40;
    }
  }

  private short checkSensitiveCipherResult(Probe probe) {
    short outputLength = probe.sensitiveCipherResult();
    try {
      SensitiveResult.assertEquals(outputLength);
      return outputLength == 16 ? (short)0x6A42 : (short)0x6A43;
    } catch (SecurityException missingSensitiveResult) {
      return (short)0x6A44;
    }
  }

  private short checkSensitiveSignatureResult(Probe probe) {
    short result = probe.sensitiveSignatureResult();
    if (result != 1) return result;
    try {
      SensitiveResult.assertTrue();
      return result == 1 ? (short)0x6A50 : (short)0x6A51;
    } catch (SecurityException missingSensitiveResult) {
      return (short)0x6A52;
    }
  }

  private short checkSensitiveSignatureMismatchResult(Probe probe) {
    short result = probe.sensitiveSignatureMismatchResult();
    if (result != 1) return (short)0x6A55;
    try {
      SensitiveResult.assertFalse();
      return (short)0x6A56;
    } catch (SecurityException missingFalseResult) {
      return (short)0x6A57;
    }
  }

  public void process(APDU apdu) {
    if (selectingApplet()) {
      return;
    }
    byte[] buffer = apdu.getBuffer();
    boolean denial_probe = buffer[1] == 0x11;
    Shareable shareable;
    if (buffer[1] == 0x16 || buffer[1] == 0x18 || buffer[1] == 0x19) {
      shareable = buffer[1] == 0x19
          ? retainedShareableAlias : retainedShareable;
    } else {
      AID server = JCSystem.lookupAID(SERVICE_AID, (short) 0,
                                      (byte) SERVICE_AID.length);
      shareable = JCSystem.getAppletShareableInterfaceObject(
          server, denial_probe ? (byte) 0x5B : (byte) 0x5A);
    }
    short result;
    if (denial_probe) {
      result = shareable == null ? (short) 0x6A01 : (short) 0x6A02;
    } else if (buffer[1] == 0x15) {
      retainedShareable = shareable;
      retainedShareableAlias = shareable;
      result = shareable == null ? (short) 0x6A11 : (short) 0x6A10;
    } else if (shareable == null) {
      result = (short) 0x6A04;
    } else if (buffer[1] == 0x1A) {
      Probe probe = (Probe) shareable;
      result = probe.inspectPrivateArray(new byte[] { 0x33 });
    } else if (buffer[1] == 0x1B) {
      Probe probe = (Probe) shareable;
      Object[] global = (Object[])JCSystem.makeGlobalArray(
          JCSystem.ARRAY_TYPE_OBJECT, (short)1);
      global[0] = this;
      boolean staticRejected = false;
      boolean instanceRejected = false;
      boolean arrayRejected = false;
      try {
        retainedGlobalStatic = global;
      } catch (SecurityException expected) {
        staticRejected = true;
      }
      try {
        retainedGlobalInstance = global;
      } catch (SecurityException expected) {
        instanceRejected = true;
      }
      try {
        Object[] sink = new Object[1];
        sink[0] = global;
      } catch (SecurityException expected) {
        arrayRejected = true;
      }
      short foreignResult = probe.inspectGlobalReference(global);
      result = staticRejected && instanceRejected && arrayRejected &&
                       foreignResult == (short)0x6B31
                   ? (short)0x6B33
                   : (short)0x6B34;
    } else if (buffer[1] == 0x1C) {
      byte[] original = new byte[] { 0x2A };
      byte[] returned = ((Probe) shareable).echoArray(original);
      result = returned == original && returned[0] == 0x2A
          ? (short) 0x6A20 : (short) 0x6A21;
    } else if (buffer[1] == 0x1D) {
      Probe probe = (Probe) shareable;
      byte[] original = new byte[] { 0x2B };
      boolean sameForeignReference =
          probe.sameReferences(original, original);
      boolean sameNullReference = probe.sameReferences(null, null);
      result = sameForeignReference && sameNullReference
          ? (short) 0x6A22 : (short) 0x6A23;
    } else if (buffer[1] == 0x1E) {
      Probe probe = (Probe) shareable;
      byte[] original = new byte[] { 0x2C };
      short castResult = probe.inspectForeignCast(original);
      short instanceofResult = probe.inspectForeignInstanceof(original);
      result = castResult == (short)0x6B32 &&
                       instanceofResult == (short)0x6B32
                   ? (short)0x6A24
                   : (short)0x6A25;
    } else if (buffer[1] == 0x1F) {
      Probe probe = (Probe) shareable;
      short nativeResult = probe.inspectPrivateArrayNative(
          new byte[] { 0x2D });
      result = nativeResult == (short)0x6B37
          ? (short)0x6A26 : (short)0x6A27;
    } else if (buffer[1] == 0x20) {
      Probe probe = (Probe) shareable;
      Object[] global = (Object[])JCSystem.makeGlobalArray(
          JCSystem.ARRAY_TYPE_OBJECT, (short)1);
      global[0] = new byte[] { 0x5C };
      result = probe.retainForeignReference(global);
      global[0] = null;
    } else if (buffer[1] == 0x2E) {
      byte[] retained = (byte[])
          ((Probe) shareable).getRetainedForeignArray();
      result = retained != null && retained.length == 1 &&
                       retained[0] == 0x5C
                   ? (short)0x6A2E
                   : (short)0x6A2F;
    } else if (buffer[1] == 0x2F) {
      result = ((Probe) shareable).retainedForeignArrayIsNull()
          ? (short)0x6A6E : (short)0x6A6F;
    } else if (buffer[1] == 0x21) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveResult(probe);
    } else if (buffer[1] == 0x22) {
      Probe probe = (Probe) shareable;
      result = checkFailedSensitiveResult(probe);
    } else if (buffer[1] == 0x23) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveAIDResults(probe);
    } else if (buffer[1] == 0x24) {
      Probe probe = (Probe) shareable;
      result = checkPreservedSensitiveAID(probe);
    } else if (buffer[1] == 0x25) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveDigestResult(probe);
    } else if (buffer[1] == 0x26) {
      Probe probe = (Probe) shareable;
      result = checkFailedSensitiveDigest(probe);
    } else if (buffer[1] == 0x27) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveCipherResult(probe);
    } else if (buffer[1] == 0x28) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveSignatureResult(probe);
    } else if (buffer[1] == 0x29) {
      Probe probe = (Probe) shareable;
      result = checkSensitiveSignatureMismatchResult(probe);
    } else if (buffer[1] == 0x2A) {
      Probe probe = (Probe) shareable;
      short marker = probe.sensitiveObjectResult();
      if (marker != (short)0x6A57) {
        result = (short)0x6A58;
      } else {
        try {
          SensitiveResult.assertEquals(this);
          result = (short)0x6A58;
        } catch (SecurityException foreignOwnerCollision) {
          result = (short)0x6A59;
        }
      }
    } else if (buffer[1] == 0x2B) {
      result = checkSioExceptionPropagation((Probe) shareable);
    } else if (buffer[1] == 0x2C) {
      result = ((Probe) shareable).inspectInheritedInterfaces();
    } else if (buffer[1] == 0x2D) {
      result = ((Probe) shareable).validateNonAtomicUtil();
    } else if (buffer[1] == 0x30) {
      result = checkSensitiveGlobalArrayResult((Probe) shareable);
    } else if (buffer[1] == 0x16) {
      try {
        AID callerAID = JCSystem.getAID();
        Probe probe = (Probe) shareable;
        result = probe.exercise((byte) 0xA5, (short) 0x1234, (short) 0x2345,
                                callerAID, null);
      } catch (SecurityException staleProxy) {
        result = (short) 0x6A0A;
      }
    } else if (buffer[1] == 0x13) {
      Probe probe = (Probe) shareable;
      int wide = probe.exerciseWide(0x12345678);
      buffer[0] = (byte) (wide >> 24);
      buffer[1] = (byte) (wide >> 16);
      buffer[2] = (byte) (wide >> 8);
      buffer[3] = (byte) wide;
      apdu.setOutgoingAndSend((short) 0, (short) 4);
      return;
    } else if (buffer[1] == 0x14) {
      Probe probe = (Probe) shareable;
      try {
        probe.throwSecurityException();
        result = (short) 0x6A06;
      } catch (SecurityException expected) {
        result = (short) 0x6A07;
      }
    } else if (buffer[1] == 0x17) {
      try {
        ((Object) shareable).equals(shareable);
        result = (short) 0x6A08;
      } catch (SecurityException expected) {
        result = (short) 0x6A09;
      }
    } else if (buffer[1] == 0x18 || buffer[1] == 0x19) {
      try {
        ((Object) shareable).equals(shareable);
        result = (short) 0x6A0C;
      } catch (SecurityException staleProxy) {
        result = (short) 0x6A0B;
      }
    } else {
      AID callerAID = JCSystem.getAID();
      Probe probe = (Probe) shareable;
      if (buffer[1] == 0x12) {
        try {
          probe.exercise((byte) 0xA5, (short) 0x1234, (short) 0x2345,
                         callerAID, new byte[] { 0x01 });
          result = (short) 0x6A05;
        } catch (SecurityException expected) {
          result = (short) 0x6A03;
        }
      } else {
        result = probe.exercise((byte) 0xA5, (short) 0x1234,
                                (short) 0x2345, callerAID, null);
      }
    }
    buffer[0] = (byte) (result >> 8);
    buffer[1] = (byte) result;
    apdu.setOutgoingAndSend((short) 0, (short) 2);
  }
}
