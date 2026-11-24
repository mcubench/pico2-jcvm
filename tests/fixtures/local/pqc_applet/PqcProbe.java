package pqctest;

import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.CardRuntimeException;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Util;
import javacard.security.AESKey;
import javacard.security.CryptoException;
import javacard.security.Key;
import javacard.security.KeyBuilder;
import javacard.security.KeyPair;
import javacard.security.MessageDigest;
import javacard.security.NamedParameterSpec;
import javacard.security.PrivateKey;
import javacard.security.PublicKey;
import javacard.security.Signature;
import javacardx.crypto.Cipher;
import javacardx.crypto.kem.KEM;
import javacardx.security.pqc.EncodableKey;
import javacardx.security.pqc.MLKEMPrivateKey;
import javacardx.security.pqc.PQCKeyFactory;

/**
 * Exercises the Java Card 3.2 preview PQC API (ML-KEM / ML-DSA) for the
 * repository's VPCD and PC/SC tests. Built with the SDK 26.1 converter
 * (-enable-preview) by tools/build-pqc-probe. Large inputs and outputs live
 * in BUF and are moved with WRITE/READ/HASH; offsets are big-endian shorts.
 *
 * Status words: 9000 ok; 9Cxx CryptoException reason xx; 9Dxx
 * SystemException reason xx; 9E01 NullPointerException; 9E02
 * ArrayIndexOutOfBounds; 9E03 ClassCastException; 9E04 SecurityException;
 * 9E05 other CardRuntimeException; 9E06 other exception.
 */
public class PqcProbe extends Applet {
  private static final short BUF_SIZE = 12288;
  private static final short SW_CRYPTO = (short) 0x9C00;
  private static final short SW_SYSTEM = (short) 0x9D00;
  private static final short SW_NPE = (short) 0x9E01;
  private static final short SW_AIOOBE = (short) 0x9E02;
  private static final short SW_CCE = (short) 0x9E03;
  private static final short SW_SECURITY = (short) 0x9E04;
  private static final short SW_CARD_RUNTIME = (short) 0x9E05;
  private static final short SW_OTHER = (short) 0x9E06;
  private final byte[] buf;
  private final Key[] keys = new Key[3];
  private KEM kem;
  private Signature sig;
  private AESKey aes;
  private final MessageDigest sha;

  private PqcProbe() {
    buf = new byte[BUF_SIZE];
    sha = MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false);
    aes = (AESKey) KeyBuilder.buildKey(KeyBuilder.TYPE_AES,
                                       KeyBuilder.LENGTH_AES_256, false);
  }

  public static void install(byte[] array, short offset, byte length) {
    new PqcProbe().register();
  }

  private static short s(byte[] b, short o) { return Util.getShort(b, o); }

  public void process(APDU apdu) {
    if (selectingApplet()) return;
    byte[] a = apdu.getBuffer();
    short len = apdu.setIncomingAndReceive();
    short d = ISO7816.OFFSET_CDATA;
    byte p1 = a[ISO7816.OFFSET_P1];
    byte p2 = a[ISO7816.OFFSET_P2];
    short out = 0;
    try {
      switch (a[ISO7816.OFFSET_INS]) {
      case 0x10: // WRITE: P1P2 = offset, data -> BUF
        Util.arrayCopyNonAtomic(a, d, buf, s(a, ISO7816.OFFSET_P1), len);
        break;
      case 0x11: { // READ: P1P2 = offset, data[0] = length
        short n = (short) (a[d] & 0xFF);
        Util.arrayCopyNonAtomic(buf, s(a, ISO7816.OFFSET_P1), a, (short) 0, n);
        out = n;
        break;
      }
      case 0x12: // HASH: SHA-256(BUF[off..off+len])
        out = sha.doFinal(buf, s(a, d), s(a, (short) (d + 2)), a, (short) 0);
        break;
      case 0x13: // COMPARE: data = off1, off2, len -> 1 if equal
        a[0] = (byte) (Util.arrayCompare(buf, s(a, d), buf, s(a, (short) (d + 2)),
                                         s(a, (short) (d + 4))) == 0 ? 1 : 0);
        out = 1;
        break;
      case 0x14: // FLIP: P1P2 = offset; BUF[offset] ^= 0x01
        buf[s(a, ISO7816.OFFSET_P1)] ^= 0x01;
        break;
      case 0x20: { // BUILD: P1 = slot, data = name, attributes
        keys[p1] = PQCKeyFactory.buildKey(s(a, d), s(a, (short) (d + 2)));
        break;
      }
      case 0x21: // SET_ENCODED: P1 = slot, data = off, len
        ((EncodableKey) keys[p1]).setEncoded(buf, s(a, d), s(a, (short) (d + 2)));
        break;
      case 0x22: // GET_ENCODED: P1 = slot, data = off -> end offset
        Util.setShort(a, (short) 0,
                      ((EncodableKey) keys[p1]).getEncoded(buf, s(a, d)));
        out = 2;
        break;
      case 0x23: // GENERATE: P1 = public slot, P2 = private slot
        PQCKeyFactory.generateKeyPair((PublicKey) keys[p1], (PrivateKey) keys[p2]);
        break;
      case 0x24: { // EXPAND: P1 = seed slot, data = pub slot, priv slot (FF = null)
        PublicKey pub = null;
        PrivateKey priv = null;
        if (a[d] != (byte) 0xFF) pub = (PublicKey) keys[a[d]];
        if (a[(short) (d + 1)] != (byte) 0xFF)
          priv = (PrivateKey) keys[a[(short) (d + 1)]];
        PQCKeyFactory.generateKeyPair((PrivateKey) keys[p1], pub, priv);
        break;
      }
      case 0x25: { // INFO: P1 = slot
        EncodableKey k = (EncodableKey) keys[p1];
        a[0] = (byte) (k.isInitialized() ? 1 : 0);
        a[1] = k.getType();
        Util.setShort(a, (short) 2, k.getSize());
        Util.setShort(a, (short) 4, k.getEncodingLength());
        Util.setShort(a, (short) 6, k.getParams().getName());
        a[8] = (byte) ((keys[p1] instanceof PrivateKey ? 1 : 0)
                       | (keys[p1] instanceof PublicKey ? 2 : 0)
                       | (keys[p1] instanceof MLKEMPrivateKey ? 4 : 0));
        out = 9;
        break;
      }
      case 0x26: // KEM_PUBLIC: P1 = slot, data = off -> end offset
        Util.setShort(a, (short) 0,
                      ((MLKEMPrivateKey) keys[p1]).getPublicKey(buf, s(a, d)));
        out = 2;
        break;
      case 0x27: // CLEAR: P1 = slot
        ((EncodableKey) keys[p1]).clearKey();
        break;
      case 0x28: // KEYPAIR: KeyPair(pub, priv).genKeyPair()
        new KeyPair((PublicKey) keys[p1], (PrivateKey) keys[p2]).genKeyPair();
        break;
      case 0x29: // NAMED: NamedParameterSpec.getInstance(name).getName()
        Util.setShort(a, (short) 0, NamedParameterSpec.getInstance(s(a, d)).getName());
        out = 2;
        break;
      case 0x30: // KEM_INSTANCE: data = algorithm, name, mode
        kem = KEM.getInstance(s(a, d),
                              NamedParameterSpec.getInstance(s(a, (short) (d + 2))),
                              s(a, (short) (d + 4)));
        a[0] = (byte) ((kem instanceof KEM.Encapsulator ? 1 : 0)
                       | (kem instanceof KEM.Decapsulator ? 2 : 0));
        out = 1;
        break;
      case 0x31: { // ENCAPS: P1 0 = encKey bytes, 1 = public key slot 0;
                   // P2 0 = bytes, 1 = AES-256 key from [from, to)
                   // data = ekOff, ekLen, ctOff, ssOff/from, to
        KEM.Encapsulator e = (KEM.Encapsulator) kem;
        short n;
        if (p1 == 0 && p2 == 0)
          n = e.encapsulate(buf, s(a, d), s(a, (short) (d + 2)), buf,
                            s(a, (short) (d + 4)), buf, s(a, (short) (d + 6)));
        else if (p1 == 0)
          n = e.encapsulate(buf, s(a, d), s(a, (short) (d + 2)), buf,
                            s(a, (short) (d + 4)), aes, s(a, (short) (d + 6)),
                            s(a, (short) (d + 8)));
        else if (p2 == 0)
          n = e.encapsulate((PublicKey) keys[0], buf, s(a, (short) (d + 4)),
                            buf, s(a, (short) (d + 6)));
        else
          n = e.encapsulate((PublicKey) keys[0], buf, s(a, (short) (d + 4)),
                            aes, s(a, (short) (d + 6)), s(a, (short) (d + 8)));
        Util.setShort(a, (short) 0, n);
        Util.setShort(a, (short) 2, e.cipherTextSize());
        Util.setShort(a, (short) 4, e.sharedSecretSize());
        out = 6;
        break;
      }
      case 0x32: { // DECAPS: P1 = private slot; P2 0 = bytes, 1 = AES key;
                   // data = ctOff, ssOff/from, to
        KEM.Decapsulator dc = (KEM.Decapsulator) kem;
        short n;
        if (p2 == 0)
          n = dc.decapsulate((PrivateKey) keys[p1], buf, s(a, d), buf,
                             s(a, (short) (d + 2)));
        else
          n = dc.decapsulate((PrivateKey) keys[p1], buf, s(a, d), aes,
                             s(a, (short) (d + 2)), s(a, (short) (d + 4)));
        Util.setShort(a, (short) 0, n);
        out = 2;
        break;
      }
      case 0x33: // AES_GET: AES key bytes -> response
        out = aes.getKey(a, (short) 0);
        break;
      case 0x40: // SIG_INSTANCE: P1 = cipher algorithm
        sig = Signature.getInstance(MessageDigest.ALG_NULL, p1, Cipher.PAD_NULL,
                                    false);
        a[0] = sig.getCipherAlgorithm();
        out = 1;
        break;
      case 0x41: // SIG_INIT: P1 = slot, P2 = mode; data = ctxOff, ctxLen
                 // (no data = init without parameters)
        if (len == 0)
          sig.init(keys[p1], p2);
        else
          sig.init(keys[p1], p2, buf, s(a, d), s(a, (short) (d + 2)));
        break;
      case 0x42: { // SIGN: data = msgOff, msgLen, sigOff, chunk (0 = one call)
        short mo = s(a, d), ml = s(a, (short) (d + 2));
        short chunk = s(a, (short) (d + 6));
        if (chunk > 0) {
          while (ml > chunk) {
            sig.update(buf, mo, chunk);
            mo += chunk;
            ml -= chunk;
          }
        }
        Util.setShort(a, (short) 0, sig.sign(buf, mo, ml, buf, s(a, (short) (d + 4))));
        Util.setShort(a, (short) 2, sig.getLength());
        out = 4;
        break;
      }
      case 0x43: { // VERIFY: data = msgOff, msgLen, sigOff, sigLen, chunk
        short mo = s(a, d), ml = s(a, (short) (d + 2));
        short chunk = s(a, (short) (d + 8));
        if (chunk > 0) {
          while (ml > chunk) {
            sig.update(buf, mo, chunk);
            mo += chunk;
            ml -= chunk;
          }
        }
        a[0] = (byte) (sig.verify(buf, mo, ml, buf, s(a, (short) (d + 4)),
                                  s(a, (short) (d + 6))) ? 1 : 0);
        out = 1;
        break;
      }
      case 0x60: // FREE: drop references and request object deletion (0x50 is GP INITIALIZE UPDATE)
        keys[0] = keys[1] = keys[2] = null;
        kem = null;
        sig = null;
        if (JCSystem.isObjectDeletionSupported()) JCSystem.requestObjectDeletion();
        break;
      default:
        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
      }
    } catch (CryptoException e) {
      ISOException.throwIt((short) (SW_CRYPTO + (e.getReason() & 0x00FF)));
    } catch (javacard.framework.SystemException e) {
      ISOException.throwIt((short) (SW_SYSTEM + (e.getReason() & 0x00FF)));
    } catch (ISOException e) {
      throw e;
    } catch (NullPointerException e) {
      ISOException.throwIt(SW_NPE);
    } catch (ArrayIndexOutOfBoundsException e) {
      ISOException.throwIt(SW_AIOOBE);
    } catch (ClassCastException e) {
      ISOException.throwIt(SW_CCE);
    } catch (SecurityException e) {
      ISOException.throwIt(SW_SECURITY);
    } catch (CardRuntimeException e) {
      ISOException.throwIt(SW_CARD_RUNTIME);
    } catch (Exception e) {
      ISOException.throwIt(SW_OTHER);
    }
    if (out > 0) apdu.setOutgoingAndSend((short) 0, out);
  }
}
