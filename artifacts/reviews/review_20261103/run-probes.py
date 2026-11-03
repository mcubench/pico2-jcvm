#!/usr/bin/env python3
"""Build and run offline review probes. Exit 1 means functionality defects remain."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

folder = Path(__file__).resolve().parent
root = folder.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--ubsan', action='store_true')
args = parser.parse_args()
build = root / 'build' / ('review-20261103-ubsan' if args.ubsan else 'review-20261103')
configure = [str(root / 'tools/env'), 'cmake', '-S', str(folder), '-B', str(build)]
if args.ubsan:
    configure.append('-DREVIEW_UBSAN=ON')
for command in [configure, [str(root / 'tools/env'), 'cmake', '--build', str(build), '--parallel', '2']]:
    subprocess.run(command, check=True, cwd=root, timeout=240)
if args.ubsan:
    cases = [('ubsan-unsigned-shift', 'review_probe', ['unsigned-shift']),
             ('ubsan-overflow', 'review_probe', ['overflow'])]
else:
    modes = ['division', 'unsigned-shift', 'overflow', 'foreign-array-gc',
             'sensitive-owner', 'int-array-size', 'reset-clearing',
             'signature-array-overload', 'restored-transient-size']
    cases = [(mode, 'review_probe', [mode]) for mode in modes]
    cases += [('static-abort', 'persistence_probe', []),
              ('array-retarget', 'persistence_probe', ['array-retarget']),
              ('session-adapted', 'session_probe', [])]
results = []
for name, executable, parameters in cases:
    process = subprocess.run([str(build / executable), *parameters],
                             capture_output=True, text=True, timeout=30, cwd=root)
    output = process.stdout + process.stderr
    (folder / (name + '.log')).write_text(output + '\nexit=' + str(process.returncode) + '\n')
    sanitizer_failure = 'runtime error:' in output
    failed = process.returncode != 0 or sanitizer_failure
    print(('TEST:FAIL ' if failed else 'TEST:PASS ') + name)
    print(output.strip())
    results.append({'probe': name, 'exit': process.returncode,
                    'sanitizer_failure': sanitizer_failure, 'failed': failed})
(folder / ('ubsan-results.json' if args.ubsan else 'probe-results.json')).write_text(
    json.dumps(results, indent=2) + '\n')
failures = sum(row['failed'] for row in results)
print(f'TEST:SUMMARY pass={len(results) - failures} fail={failures}')
sys.exit(1 if failures else 0)
