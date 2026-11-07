"""Names of JavaCard exception reason codes and a parser for the `call_details` / status strings of the API test csv files.

Reason values are taken from the Java Card 3.1 Classic API constant values
(https://docs.oracle.com/en/java/javacard/3.1/jc_api_srvc/api_classic/constant-values.html; e.g. javacard.security.CryptoException).
The csv often carries only `reason=0x0003` (CardRuntimeException); the concrete exception class is then inferred from the tested class
(javacard.security.* -> CryptoException, ...) and the result is marked as inferred.
"""
import re

REASONS = {
    "CryptoException": {1: "ILLEGAL_VALUE", 2: "UNINITIALIZED_KEY", 3: "NO_SUCH_ALGORITHM", 4: "INVALID_INIT", 5: "ILLEGAL_USE"},
    "SystemException": {1: "ILLEGAL_VALUE", 2: "NO_TRANSIENT_SPACE", 3: "ILLEGAL_TRANSIENT", 4: "ILLEGAL_AID", 5: "NO_RESOURCE", 6: "ILLEGAL_USE"},
    "APDUException": {1: "ILLEGAL_USE", 2: "BUFFER_BOUNDS", 3: "BAD_LENGTH", 4: "IO_ERROR", 170: "NO_T0_GETRESPONSE", 171: "T1_IFD_ABORT", 172: "NO_T0_REISSUE"},
    "PINException": {1: "ILLEGAL_VALUE", 2: "ILLEGAL_STATE"},
    "TransactionException": {1: "IN_PROGRESS", 2: "NOT_IN_PROGRESS", 3: "BUFFER_FULL", 4: "INTERNAL_FAILURE", 5: "ILLEGAL_USE"},
    "ServiceException": {1: "ILLEGAL_PARAM", 2: "DISPATCH_TABLE_FULL", 3: "COMMAND_DATA_TOO_LONG", 4: "CANNOT_ACCESS_IN_COMMAND",
                         5: "CANNOT_ACCESS_OUT_COMMAND", 6: "COMMAND_IS_FINISHED", 7: "REMOTE_OBJECT_NOT_EXPORTED"},
    "TLVException": {1: "INVALID_PARAM", 2: "ILLEGAL_SIZE", 3: "EMPTY_TAG", 4: "EMPTY_TLV", 5: "MALFORMED_TAG", 6: "MALFORMED_TLV", 7: "INSUFFICIENT_STORAGE",
                     8: "TAG_SIZE_GREATER_THAN_127", 9: "TAG_NUMBER_GREATER_THAN_32767", 10: "TLV_SIZE_GREATER_THAN_32767", 11: "TLV_LENGTH_GREATER_THAN_32767"},
    "BioException": {1: "ILLEGAL_VALUE", 2: "INVALID_DATA", 3: "NO_SUCH_BIO_TEMPLATE", 4: "NO_TEMPLATES_ENROLLED", 5: "ILLEGAL_USE"},
    "ExternalException": {1: "NO_SUCH_SUBSYSTEM", 2: "INVALID_PARAM", 3: "INTERNAL_ERROR"},
    "UtilException": {1: "ILLEGAL_VALUE", 2: "TYPE_MISMATCHED"},
    "StringException": {1: "UNSUPPORTED_ENCODING", 2: "ILLEGAL_NUMBER_FORMAT", 3: "INVALID_BYTE_SEQUENCE"},
}

# tested class prefix -> exception class (first match wins)
BY_CLASS = [
    ("javacard.security.", "CryptoException"), ("javacardx.crypto.", "CryptoException"),
    ("javacard.framework.service.", "ServiceException"), ("javacard.framework.APDU", "APDUException"),
    ("javacard.framework.PIN", "PINException"), ("javacard.framework.OwnerPIN", "PINException"),
    ("javacard.framework.JCSystem", "SystemException"), ("javacard.framework.Applet", "SystemException"),
    ("javacardx.framework.tlv.", "TLVException"), ("javacardx.biometry.", "BioException"), ("javacardx.external.", "ExternalException"),
    ("javacardx.framework.util.", "UtilException"), ("javacardx.framework.string.", "StringException"), ("javacardx.framework.math.", "UtilException"),
]
TAGS = {"EXC_SYSTEM": "SystemException", "EXC_CRYPTO": "CryptoException", "EXC_APDU": "APDUException", "EXC_PIN": "PINException",
        "EXC_TRANSACTION": "TransactionException", "EXC_SERVICE": "ServiceException", "EXC_TLV": "TLVException", "EXC_BIO": "BioException"}
SW = {0x6700: "wrong length", 0x6982: "security status not satisfied", 0x6983: "authentication method blocked", 0x6985: "conditions of use not satisfied",
      0x6A80: "wrong data / incorrect values in data", 0x6A81: "function not supported", 0x6A82: "file or application not found", 0x6A84: "not enough memory space",
      0x6A86: "incorrect P1/P2", 0x6D00: "INS not supported", 0x6E00: "CLA not supported", 0x6F00: "no precise diagnosis (unknown error)",
      0x6438: "card-specific (GP load failure)", 0x6881: "logical channel not supported", 0x6999: "applet selection failed", 0x9000: "OK"}

_REASON = re.compile(r"reason=(0x[0-9a-fA-F]+|\d+)")
_SW = re.compile(r"\bSW=([0-9a-fA-F]{4})")
_NAMED = re.compile(r"\(([A-Z][A-Z0-9_]+)\)")


def parse(cls, method, status, details):
    """Returns None, or dict(exception, reason, name, inferred, text, short)."""
    text = (status + " " + details)
    m = _REASON.search(text)
    if m:
        reason = int(m.group(1), 0)
        if reason >= 0x6000:  # an ISOException: the reason is a status word
            d = SW.get(reason)
            return {"exception": "ISOException", "reason": reason, "name": d, "inferred": False,
                    "text": "ISOException SW=%04X%s" % (reason, " (%s)" % d if d else ""), "short": "SW %04X" % reason}
        exc, inferred = None, False
        for tag, e in TAGS.items():
            if tag in text:
                exc = e
                break
        if exc is None:
            if "ransaction" in method:
                exc = "TransactionException"
            else:
                for pre, e in BY_CLASS:
                    if cls.startswith(pre):
                        exc = e
                        break
            inferred = exc is not None
        name = REASONS.get(exc, {}).get(reason) if exc else None
        if name is None:  # the csv may already carry the name: "reason=0x0001 (ILLEGAL_VALUE)"
            n = _NAMED.search(details[m.end():])
            name = n.group(1) if n else None
        exc = exc or "CardRuntimeException"
        short = name or ("0x%04X" % reason)
        full = "%s.%s (reason 0x%04X)" % (exc, name or "unknown reason", reason)
        if inferred:
            full += " - class inferred from %s" % cls
        return {"exception": exc, "reason": reason, "name": name, "inferred": inferred, "text": full, "short": short}
    m = _SW.search(text)
    if m:
        sw = int(m.group(1), 16)
        d = SW.get(sw)
        return {"exception": "ISOException/SW", "reason": sw, "name": d, "inferred": False,
                "text": "SW=%04X%s" % (sw, " (%s)" % d if d else ""), "short": "SW %04X" % sw}
    return None


if __name__ == "__main__":
    for a in [("javacard.security.KeyBuilder", "", "UNSUPPORTED_PER_SPEC", "EXC_CARDRUNTIME reason=0x0003;"),
              ("javacard.framework.JCSystem", "", "NO_MEMORY", "EXC_SYSTEM reason=0x0002 (NO_TRANSIENT_SPACE);"),
              ("javacardx.framework.tlv.BERTLV", "", "EXC_CARDRUNTIME", "reason=0x0001;"), ("x", "", "SW_ERROR", "SW=1401;")]:
        print(parse(*a))
