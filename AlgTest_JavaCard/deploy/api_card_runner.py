#!/usr/bin/env python3
"""
Runs the JavaCard API support test on a PHYSICAL card in one go and collects everything in ONE csv file.

For every API area (Core, Security, Crypto, ...) the script
  1. UPLOAD   uploads the CAP of the area with gp.jar (GlobalPlatformPro),
  2. TEST     runs the client (ApiTestJClient) against the installed applet,
  3. CLEANUP  removes the CAP again (so that a card with little EEPROM can take the next one),
and writes one record per stage plus one record per tested API call into the same csv (see COLUMNS). Every failure is classified (failure_type) so that
"the card does not have the package" (UPLOAD_REFUSED_LOAD), "the card could not be reached" (UPLOAD_NO_CONNECT), "the test run broke" (TEST_*) and
"the CAP could not be removed" (CLEANUP_*) can be told apart. Partial results are written after every step (the file survives a crash or Ctrl-C).

Prerequisites: Java (the same `java` runs gp.jar and the client), the CAPs (see docs/API_COVERAGE.md: `./api_dev.sh caps`, or the prebuilt files in !uploader),
the client jar (`ant clean jar` in the repository root -> AlgTest_JClient/dist/AlgTestJClient.jar), a PC/SC reader with the card inserted.
Typical use (card in the contact slot, GlobalPlatform default keys):

    python3 api_card_runner.py --cardname "My card"                 # all areas, API version detected from the card
    python3 api_card_runner.py --cardname "My card" --api 304 --areas Core,Security
    python3 api_card_runner.py --list                               # only shows the plan (no card needed)

Options worth knowing: --reader-name "<exact PC/SC reader name>" pins GPPro and resolves the official Java client's terminal index; --reader-index is available when an index is intentionally pinned. --gp-args accepts extra gp.jar arguments such as `-d` or `--key ...`. `tools/run-deploy-parallel` distributes areas over up to three named Pico readers into separate CSV and log files. --keep leaves CAPs installed; --retries controls reconnect attempts.
"""
import shlex
import argparse
import csv
import datetime
import glob
import os
import re
import pathlib
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
COLUMNS = ['row_type', 'timestamp', 'card_name', 'atr', 'api_version', 'area', 'stage', 'stage_result', 'failure_type', 'detail',
           'class', 'method', 'argument', 'status', 'call_details']

# Multi CAP (MultiSelectable scenarios): package and applet AIDs of jcbuild_api.xml
MULTI_PKG = '4a43416c675465737441706d'
MULTI_APPLETS = [MULTI_PKG + '41', MULTI_PKG + '42']

# (area, CAP name part, client --areas, class filter of the client or None, scenario CAPs installed with it)
AREAS = [
    ('Core', 'Core', 'Core', None, True),
    ('Security', 'Security', 'Security', None, False),
    ('Crypto', 'Crypto', 'Crypto', None, False),
    ('Javacardx', 'Javacardx', 'Javacardx', None, False),            # javacardx.framework.* (util, math, string, tlv), javacardx.security, javacardx.apdu.util
    ('JavacardxExt', 'JavacardxExt', 'Javacardx', '0x4f,0x50,0x51', False),   # javacardx.external only
    ('JavacardxSec', 'JavacardxSec', 'Javacardx', '0x53,0x54', False),   # javacardx.security and javacardx.apdu.util only (API 3.0.5 CAP)
    ('JavacardxInt', 'JavacardxInt', 'Javacardx', '0x42', False),    # javacardx.framework.util.intx.JCint, needs int support
    ('Service', 'Service', 'Service', None, False),                  # javacard.framework.service, java.rmi
    ('Biometry', 'Biometry', 'Biometry', None, False),               # javacardx.biometry
]
API_VERSIONS = ['305', '304', '303', '222', '221']      # highest first, as detected on the card

CONNECT_MARKERS = ('proto_mismatch', 'unpowered', 'no card', 'no_smartcard', 'scard_e_no_readers', 'could not connect', 'no smart card readers', 'not_transacted', 'failed to communicate with card', 'card removed', 'scard_w_removed_card', 'scard_e_no_service')   # the reader name alone is no marker (it is in every output)


class Out:
    """the single csv file, flushed after every record"""

    def __init__(self, path, delimiter, card, atr_ref):
        self.f = open(path, 'a', newline='', encoding='utf-8')
        self.w = csv.writer(self.f, delimiter=delimiter)
        if os.path.getsize(path) == 0:
            self.w.writerow(COLUMNS)
        self.card = card
        self.atr = ''
        self.api = ''
        self.path = path

    def row(self, **k):
        r = dict.fromkeys(COLUMNS, '')
        r.update(k)
        r['timestamp'] = datetime.datetime.now().isoformat(timespec='seconds')
        r['card_name'] = self.card
        r['atr'] = self.atr
        r['api_version'] = self.api if 'api_version' not in k else k['api_version']
        self.w.writerow([r[c] for c in COLUMNS])
        self.f.flush()

    def stage(self, area, stage, result, failure='', detail=''):
        self.row(row_type='STAGE', area=area, stage=stage, stage_result=result, failure_type=failure, detail=detail.replace('\n', ' | ')[:600])
        print('  %-13s %-8s %-8s %s %s' % (area, stage, result, failure, detail.replace('\n', ' | ')[:140]))


def run(cmd, timeout, stdin=None):
    try:
        p = subprocess.run(cmd, input=stdin, capture_output=True, text=True, timeout=timeout)
        return p.returncode, (p.stdout or '') + (p.stderr or ''), False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout.decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or ''))
        return -1, out, True


LOG_PREFIXES = ('A>>', 'A<<', '[TRACE]', '[DEBUG]', '[INFO]', '[WARN]', 'SCard', 'Warning', 'GlobalPlatformPro', 'Running on', '# ', '[*]')


def clean_lines(text):
    """output lines without APDU/log traces (the log lines of gp.jar contain session keys: they never go into the csv)"""
    return [l.strip() for l in text.splitlines() if l.strip() and not l.startswith(LOG_PREFIXES) and 'key' not in l.lower().replace('keys given', '') and not l.startswith('Could not connect')]


def classify_gp(text, ok_if_quiet=False):
    """('ok'|'refused_load'|'refused_install'|'no_space'|'auth'|'no_connect'|'other', detail) of the output of one gp.jar call.
    A dual (contact + contactless) reader prints a connect error for the slot without a card even when the call worked: the sections of the readers are
    judged together, the call reached the card when at least one reader section has no connect error."""
    low = text.lower()
    lines = clean_lines(text)
    tail = ' | '.join(lines[-3:])[:300]
    sections = [x for x in re.split(r'(?m)^# ', text)[1:] if not x.startswith('Detected readers')]
    connected = [x for x in sections if 'could not connect' not in x.lower()]
    m = re.search(r'(load|install \[[^\]]*\]|delete[^\n]*) failed:? *(?:sw: *)?(0x)?([0-9a-f]{4})', low)
    if m:
        sw = m.group(3).upper()
        kind = 'refused_load' if m.group(1).startswith('load') else 'refused_install'
        if sw in ('6A84', '6A85'):
            kind = 'no_space'
        elif sw in ('6982', '6300'):
            kind = 'auth'
        return kind, 'SW=%s %s' % (sw, tail)
    # GlobalPlatformPro can print "CAP loaded" after the LOAD phase and then
    # report a failure from the subsequent INSTALL [for install and make
    # selectable] command. Check explicit failures first so that a partial
    # load is never counted as a successful applet installation.
    if 'cap loaded' in low or ' deleted.' in low or re.search(r'(?m)^isd:', low):
        return 'ok', tail
    if 'more than one applet' in low:
        return 'other', 'CAP contains several applets: ' + tail
    if 'authentication' in low or 'security status' in low or '6982' in low:
        return 'auth', tail
    if sections and not connected:
        return 'no_connect', ' | '.join(l for l in text.splitlines() if 'Could not connect' in l)[:300]
    if not sections and any(x in low for x in CONNECT_MARKERS):
        return 'no_connect', tail
    if ok_if_quiet and (connected or not sections) and not re.search(r'failed|error|exception', low):   # one reader: gp prints no per-reader section
        return 'ok', tail
    return 'other', tail


class Gp:
    def __init__(self, a, out=None):
        self.out = out
        self.cmd = [a.java, '-jar', a.gp] + (shlex.split(a.gp_args) if a.gp_args else [])
        self.retries = a.retries
        self.dump_dir = a.dump_dir
        self.pause = a.pause
        self.timeout = a.gp_timeout
        self.verbose = a.verbose

    def call(self, args, ok_if_quiet=False):
        """runs gp.jar, reconnects while the card cannot be reached; returns (kind, detail, attempts). ok_if_quiet: the call prints nothing on success (--create)"""
        kind, detail = 'no_connect', ''
        for attempt in range(1, self.retries + 1):
            rc, text, timed_out = run(self.cmd + args, self.timeout)
            if self.out is not None and not self.out.atr:   # gp.jar -d prints the ATR of the card it connects to
                m = re.search(r'-> T=[01*], ([0-9A-Fa-f]{10,})', text)
                if m:
                    self.out.atr = m.group(1).upper()
            if timed_out:
                kind, detail = 'timeout', 'gp.jar timed out after %d s (card or reader hanging)' % self.timeout
            else:
                kind, detail = classify_gp(text, ok_if_quiet)
                if kind == 'other' and not detail:
                    detail = '(no recognisable output lines)'
            if kind == 'other' and self.dump_dir:   # unclassified output for debugging, without key material
                os.makedirs(self.dump_dir, exist_ok=True)
                safe = '\n'.join(l for l in text.splitlines() if not re.search(r'key|ENC=|MAC=|DEK=', l, re.I))
                name = os.path.join(self.dump_dir, 'gp_%s_%d.txt' % (re.sub(r'[^A-Za-z0-9]+', '_', ' '.join(args))[:60], int(time.time())))
                open(name, 'w').write('args: %s\nrc: %s\n%s\n' % (' '.join(args), rc, safe))
            if self.verbose:
                print('    gp %s -> %s (%s)' % (' '.join(args), kind, detail[:100]))
            if kind not in ('no_connect', 'timeout'):
                return kind, detail, attempt
            time.sleep(self.pause)
        return kind, detail, self.retries


UPLOAD_FAILURE = {'timeout': 'UPLOAD_TIMEOUT', 'no_connect': 'UPLOAD_NO_CONNECT', 'refused_load': 'UPLOAD_REFUSED_LOAD', 'refused_install': 'UPLOAD_REFUSED_INSTALL',
                  'no_space': 'UPLOAD_NO_SPACE', 'auth': 'UPLOAD_AUTH', 'other': 'UPLOAD_OTHER'}
UPLOAD_HINT = {'UPLOAD_REFUSED_LOAD': 'the card refused the CAP (LOAD): 6A80 usually means an imported API package or API version is not available on the card, 6985 that the security domain does not allow the load (privileges, lifecycle, memory) or, on some cards (seen on a 3.0.5 card), that an imported package is missing',
               'UPLOAD_NO_CONNECT': 'the card could not be reached (reader, protocol, power)'}


def cap_path(a, name, version):
    return os.path.join(a.cap_dir, 'AlgTestApi%s_%s.cap' % (name, version))


def rollback_partial_install(g, out, area, stage, cap, kind, detail):
    """Remove a package if GPPro completed LOAD but its INSTALL phase failed."""
    if kind != 'refused_install' or not re.search(
            r'\bCAP loaded\b', detail, re.IGNORECASE):
        return
    rollback_kind, rollback_detail, attempts = g.call(['--uninstall', cap])
    if rollback_kind == 'ok':
        out.stage(area, stage, 'OK', '',
                  '%s removed after INSTALL refusal; attempts=%d' %
                  (os.path.basename(cap), attempts))
        return
    failure = {
        'no_connect': 'UPLOAD_ROLLBACK_NO_CONNECT',
        'timeout': 'UPLOAD_ROLLBACK_TIMEOUT',
    }.get(rollback_kind, 'UPLOAD_ROLLBACK_FAILED')
    out.stage(area, stage, 'FAILED', failure,
              '%s remains after INSTALL refusal; attempts=%d; %s' %
              (os.path.basename(cap), attempts, rollback_detail))


def upload(g, out, a, area, name, version):
    """uploads the CAP of the area; returns True when it is on the card"""
    cap = cap_path(a, name, version)
    if not os.path.exists(cap):
        out.stage(area, 'UPLOAD', 'SKIPPED', 'UPLOAD_NO_CAP_FILE', cap)
        return False
    kind, detail, attempts = g.call(['--install', cap])
    if kind != 'ok':
        f = UPLOAD_FAILURE[kind]
        out.stage(area, 'UPLOAD', 'FAILED', f, '%s; attempts=%d; %s' % (UPLOAD_HINT.get(f, ''), attempts, detail))
        # GPPro sends LOAD before INSTALL. If LOAD succeeded but the applet's
        # INSTALL failed, the CAP package remains resident and blocks the next
        # upload of the same AID. Roll back that partial upload immediately.
        rollback_partial_install(g, out, area, 'UPLOAD_ROLLBACK', cap,
                                 kind, detail)
        return False
    out.stage(area, 'UPLOAD', 'OK', '', 'attempts=%d' % attempts)
    return True


def upload_scenarios(g, out, a, version):
    """Multi CAP (two applets: --load + --create each) and Ext CAP (extended length), used by the Core scenarios; returns the list of CAPs on the card"""
    on_card = []
    multi = cap_path(a, 'Multi', version)
    if os.path.exists(multi):
        kind, detail, n = g.call(['--load', multi])
        if kind == 'ok':
            ok = True
            for aid in MULTI_APPLETS:
                k2, d2, _ = g.call(['--create', aid, '--applet', aid, '--package', MULTI_PKG], ok_if_quiet=True)
                if k2 != 'ok':
                    ok = False
                    out.stage('Core', 'UPLOAD_MULTI', 'FAILED', 'UPLOAD_MULTI_' + k2.upper(), 'applet %s: %s' % (aid, d2))
            on_card.append(multi)
            if ok:
                out.stage('Core', 'UPLOAD_MULTI', 'OK', '', 'two MultiSelectable applets created')
        else:
            out.stage('Core', 'UPLOAD_MULTI', 'FAILED', 'UPLOAD_MULTI_' + kind.upper(), detail)
    ext = cap_path(a, 'Ext', version)
    if os.path.exists(ext):
        kind, detail, n = g.call(['--install', ext])
        if kind == 'ok':
            on_card.append(ext)
            out.stage('Core', 'UPLOAD_EXT', 'OK', '', 'extended length applet installed')
        else:
            out.stage('Core', 'UPLOAD_EXT', 'FAILED', 'UPLOAD_EXT_' + kind.upper(), detail)
            rollback_partial_install(g, out, 'Core', 'UPLOAD_EXT_ROLLBACK',
                                     ext, kind, detail)
    return on_card


def parse_client_csv(path):
    rows = []
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\r\n')
        parts = [p.strip() for p in line.split('; ')]
        if len(parts) >= 4 and '.' in parts[0] and not parts[0].startswith(('Execution', 'Used reader', 'JavaCard API', 'Tested')):
            rows.append(parts)
    return rows


def test(out, a, area, client_area, classes, version, tmp):
    """runs the client for the area; writes the CALL records, returns the number of calls"""
    for f in glob.glob(os.path.join(tmp, '*.csv')):
        os.remove(f)
    props = []
    if classes:
        props.append('-Dapitest.classes=' + classes)
    if a.pcsc_lib:
        props.append('-Dsun.security.smartcardio.library=' + a.pcsc_lib)
    if a.exclude:
        props.append('-Dapitest.exclude=' + a.exclude)
    cmd = [a.java] + props + ['-cp', a.client, 'algtestjclient.ApiTestJClient', '-cardname', 'probe', '-outpath', tmp + os.sep, '--areas', client_area]
    rc, text, timed_out = run(cmd, a.test_timeout, stdin='%s\n' % a.reader_index)
    csvs = glob.glob(os.path.join(tmp, '*_APISUPPORT_*.csv'))
    for f in csvs:   # the ATR is part of the name of the client result file
        m = re.search(r'_APISUPPORT_([0-9a-fA-F]+)\.csv$', f)
        if m and not out.atr:
            out.atr = m.group(1).upper()
    if timed_out:
        failure, detail = 'TEST_TIMEOUT', 'client did not finish within %d s' % a.test_timeout
    elif 'Unable to connect to the card or no applet' in text:
        failure, detail = 'TEST_NO_APPLET', 'the applet of the area cannot be selected (not installed, or the card cannot be reached)'
    elif rc != 0:
        failure, detail = 'TEST_CLIENT_ERROR', 'client exited with status %d%s' % (rc, '; partial results were written' if csvs else '')
    elif not csvs:
        failure, detail = 'TEST_CLIENT_ERROR', 'no result file; ' + ' | '.join(l for l in text.splitlines() if l.strip())[-300:]
    else:
        failure, detail = '', ''
    rows = parse_client_csv(csvs[0]) if csvs else []
    for p in rows:
        out.row(row_type='CALL', area=area, stage='TEST', **{'class': p[0], 'method': p[1], 'argument': p[2] if len(p) > 2 else '',
                                                           'status': p[3] if len(p) > 3 else '', 'call_details': '; '.join(p[4:])})
    if failure:
        out.stage(area, 'TEST', 'FAILED', failure, detail)
        return len(rows)
    counts = {}
    for p in rows:
        counts[p[3]] = counts.get(p[3], 0) + 1
    summary = ', '.join('%s=%d' % kv for kv in sorted(counts.items(), key=lambda kv: -kv[1]))
    if 'too many consecutive card failures' in text:
        first = next((p for p in rows if p[3] == 'CARD_ERROR'), None)   # the call during which the card or reader gave up
        culprit = ('; first failing call: %s | %s | %s' % (first[0].split('.')[-1], first[1][:60], first[2][:60])) if first else ''
        out.stage(area, 'TEST', 'FAILED', 'TEST_ABORTED_CARD_FAILURES', 'client aborted after consecutive card failures; %d calls recorded%s; %s' % (len(rows), culprit, summary))
    elif not rows:
        out.stage(area, 'TEST', 'FAILED', 'TEST_NO_RESULTS', 'result file without calls')
    else:
        broken = counts.get('SW_ERROR', 0) + counts.get('CARD_ERROR', 0)
        out.stage(area, 'TEST', 'OK', '', '%d calls; %s%s' % (len(rows), summary, ('; %d SW_ERROR/CARD_ERROR calls' % broken) if broken else ''))
    return len(rows)


def resolve_reader_index(a, reader_name):
    """Resolve the official Java client's one-based index from its full PC/SC list."""
    build_dir = os.path.join(HERE, '..', '..', 'build', 'api-reader-index')
    os.makedirs(build_dir, exist_ok=True)
    helper = os.path.join(HERE, '..', '..', 'tools', 'JCAlgTestReaderIndex.java')
    subprocess.run([a.javac, '-d', build_dir, helper],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    props = []
    if a.pcsc_lib:
        props.append('-Dsun.security.smartcardio.library=' + a.pcsc_lib)
    result = subprocess.run([a.java] + props +
                            ['-cp', build_dir, 'JCAlgTestReaderIndex', reader_name],
                            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    return result.stdout.strip()


def require_host_access(parser):
    if os.environ.get('PICO2_HOST_USB_ACCESS') != '1':
        ap_error = 'PC/SC access requires PICO2_HOST_USB_ACCESS=1 and elevated host access'
        parser.error(ap_error)
    try:
        status = pathlib.Path('/proc/self/status').read_text(encoding='ascii')
        match = re.search(r'^NoNewPrivs:\s*(\d+)$', status, re.MULTILINE)
    except OSError:
        match = None
    if not match or match.group(1) != '0':
        parser.error('cannot verify elevated PC/SC host access; use the require_escalated execution path')


def cleanup(g, out, area, caps):
    for cap in caps:
        kind, detail, attempts = g.call(['--uninstall', cap])
        name = os.path.basename(cap)
        if kind == 'ok':
            out.stage(area, 'CLEANUP', 'OK', '', '%s removed' % name)
        else:
            out.stage(area, 'CLEANUP', 'FAILED', 'CLEANUP_' + {'no_connect': 'NO_CONNECT', 'timeout': 'TIMEOUT'}.get(kind, 'FAILED'), '%s: %s' % (name, detail))


def detect_version(g, out, a):
    """highest API version whose Core CAP the card loads (the Core CAP stays on the card when found, the caller removes it)"""
    for v in API_VERSIONS:
        cap = cap_path(a, 'Core', v)
        if not os.path.exists(cap):
            continue
        kind, detail, attempts = g.call(['--install', cap])
        if kind == 'ok':
            out.stage('Core', 'DETECT_VERSION', 'OK', '', 'the card loaded the Core CAP of API version %s' % v)
            return v
        out.stage('Core', 'DETECT_VERSION', 'FAILED', UPLOAD_FAILURE[kind], 'API %s: %s' % (v, detail))
        if kind in ('no_connect', 'timeout', 'auth'):
            return None
    return None


def main():
    sys.stdout.reconfigure(line_buffering=True)   # progress is visible when the output is redirected to a file
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--cardname', default='card', help='name written to the csv')
    ap.add_argument('--out', default=None, help='the single result csv (default api_card_results_<cardname>_<date>.csv)')
    ap.add_argument('--areas', default='', help='comma separated subset of: ' + ','.join(x[0] for x in AREAS))
    ap.add_argument('--api', default='auto', help='API version of the CAPs: auto (detect from the card), ' + ', '.join(API_VERSIONS))
    ap.add_argument('--cap-dir', default=os.path.join(HERE, '!uploader'))
    ap.add_argument('--gp', default=os.path.join(HERE, '!uploader', 'gp.jar'))
    ap.add_argument('--gp-args', default='-d', help='extra arguments for every gp.jar call (default -d: the debug mode gave the most stable connection on the tested reader), e.g. "-r \\"reader name\\"" (split on blanks)')
    ap.add_argument('--client', default=os.path.join(HERE, '..', 'AlgTest_JClient', 'dist', 'AlgTestJClient.jar'))
    ap.add_argument('--java', default='java')
    ap.add_argument('--javac', default='javac')
    ap.add_argument('--pcsc-lib', default=None, help='path of libpcsclite for Java smartcardio on Linux, e.g. /usr/lib/x86_64-linux-gnu/libpcsclite.so.1')
    ap.add_argument('--reader-index', default=None, help='explicit one-based Java SmartcardIO terminal index')
    ap.add_argument('--reader-name', default=None, help='exact PC/SC reader name; resolves Java index and pins GlobalPlatformPro')
    ap.add_argument('--retries', type=int, default=4, help='gp.jar attempts while the card cannot be reached')
    ap.add_argument('--pause', type=int, default=20, help='seconds between attempts')
    ap.add_argument('--gp-timeout', type=int, default=240)
    ap.add_argument('--test-timeout', type=int, default=3600, help='seconds per area for the client')
    ap.add_argument('--exclude', default=None, help='regex on "class; method; argument": matching calls are not run (reported as SKIPPED_BY_USER), e.g. a very slow key generation that makes a reader drop the card: "ALG_RSA_CRT, LENGTH_RSA_2048"')
    ap.add_argument('--dump-dir', default=None, help='directory for the raw (key-free) output of gp.jar calls which could not be classified')
    ap.add_argument('--keep', action='store_true', help='do not remove the CAPs after the test of the area')
    ap.add_argument('--no-scenarios', action='store_true', help='do not install the Multi and Ext CAPs for the Core scenarios')
    ap.add_argument('--delimiter', default=';')
    ap.add_argument('--list', action='store_true', help='show the plan and the CAP files and exit (no card access)')
    ap.add_argument('--test-only', action='store_true',
                    help='test an already-installed applet; skip GP upload and cleanup')
    ap.add_argument('--verbose', action='store_true')
    a = ap.parse_args()
    if a.reader_name or not a.list:
        require_host_access(ap)
    if a.reader_name and a.reader_index:
        ap.error('--reader-name and --reader-index are mutually exclusive')
    if a.reader_name:
        supplied_gp_args = shlex.split(a.gp_args) if a.gp_args else []
        if '-r' in supplied_gp_args or '--reader' in supplied_gp_args:
            ap.error('do not pass a GPPro reader selector with --reader-name')
        a.gp_args = shlex.join(supplied_gp_args + ['-r', a.reader_name])
        a.reader_index = resolve_reader_index(a, a.reader_name)
    elif a.reader_index is None:
        a.reader_index = '1'
    a.gp, a.client, a.cap_dir = os.path.abspath(a.gp), os.path.abspath(a.client), os.path.abspath(a.cap_dir)
    wanted = [x for x in a.areas.split(',') if x] or [x[0] for x in AREAS]
    unknown = [x for x in wanted if x not in [y[0] for y in AREAS]]
    if unknown:
        sys.exit('unknown area(s): %s' % ', '.join(unknown))

    versions = API_VERSIONS if a.api == 'auto' else [a.api]
    if a.list:
        print('gp.jar: %s (%s)\nclient: %s (%s)\nCAP dir: %s' % (a.gp, 'found' if os.path.exists(a.gp) else 'MISSING', a.client, 'found' if os.path.exists(a.client) else 'MISSING', a.cap_dir))
        for area, name, client_area, classes, scen in AREAS:
            if area in wanted:
                print('%-13s CAPs: %s' % (area, ' '.join(v for v in versions if os.path.exists(cap_path(a, name, v))) or 'none found'))
        return
    required_tools = [(a.client, 'client jar (run `ant clean jar`)')]
    if not a.test_only:
        required_tools.append((a.gp, 'gp.jar'))
    for p, what in required_tools:
        if not os.path.exists(p):
            sys.exit('%s not found: %s' % (what, p))
    if a.out is None:
        a.out = 'api_card_results_%s_%s.csv' % (re.sub(r'[^A-Za-z0-9_.-]+', '_', a.cardname), datetime.date.today().isoformat())
    out = Out(a.out, a.delimiter, a.cardname, None)
    g = Gp(a, out)
    print('results: %s' % os.path.abspath(a.out))

    tmp = tempfile.mkdtemp(prefix='apitest_card_')
    version = a.api if a.api != 'auto' else None
    if a.test_only:
        if version is None:
            sys.exit('--test-only requires an explicit --api version')
        for area, _name, client_area, classes, _scen in AREAS:
            if area not in wanted:
                continue
            out.api = version
            print('== %s (already installed)' % area)
            test(out, a, area, client_area, classes, version, tmp)
        return

    started_core = False
    for area, name, client_area, classes, scen in AREAS:
        if area not in wanted:
            continue
        print('== %s' % area)
        if version is None:
            # detect with the Core CAP (stays on the card): Core is the first area in the list, otherwise detect anyway
            version = detect_version(g, out, a)
            if version is None:
                out.stage(area, 'DETECT_VERSION', 'FAILED', 'DETECT_NO_API_VERSION', 'no Core CAP of any API version could be uploaded; stopping')
                break
            out.api = version
            installed = [cap_path(a, 'Core', version)]
            if area != 'Core':
                cleanup(g, out, 'Core', installed)
            else:
                started_core = True
        out.api = version
        caps = []
        if area == 'Core' and started_core:
            caps.append(cap_path(a, 'Core', version))
            out.stage(area, 'UPLOAD', 'OK', '', 'uploaded by the version detection')
            ok = True
        else:
            ok = upload(g, out, a, area, name, version)
            if ok:
                caps.append(cap_path(a, name, version))
        if ok and area == 'Core' and scen and not a.no_scenarios:
            caps += upload_scenarios(g, out, a, version)
        if ok:
            test(out, a, area, client_area, classes, version, tmp)
        if not a.keep:
            cleanup(g, out, area, list(reversed(caps)))
    print('done: %s' % os.path.abspath(a.out))


if __name__ == '__main__':
    main()
