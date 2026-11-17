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
mode_group = parser.add_mutually_exclusive_group()
mode_group.add_argument('--ubsan', action='store_true')
mode_group.add_argument('--iteration2', action='store_true')
mode_group.add_argument('--asan', action='store_true')
args = parser.parse_args()
build = root / 'build' / ('review-20261103-asan' if args.asan else
                          'review-20261103-ubsan' if args.ubsan else 'review-20261103')
configure = [str(root / 'tools/env'), 'cmake', '-S', str(folder), '-B', str(build)]
if args.ubsan:
    configure.append('-DREVIEW_UBSAN=ON')
if args.asan:
    configure.append('-DREVIEW_ASAN=ON')
for command in [configure, [str(root / 'tools/env'), 'cmake', '--build', str(build), '--parallel', '2']]:
    subprocess.run(command, check=True, cwd=root, timeout=240)
if args.iteration2:
    cases = [(name, 'persistence_probe', [name]) for name in
             ['object-retarget', 'object-self-assignment', 'instance-write-failure',
              'failed-first-persist']]
elif args.asan:
    cases = [('short-serialization-overflow', 'persistence_probe', ['short-serialization-overflow'])]
elif args.ubsan:
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
output_folder = folder / 'iteration2' if args.iteration2 or args.asan else folder
output_folder.mkdir(exist_ok=True)
results = []
for name, executable, parameters in cases:
    process = subprocess.run([str(build / executable), *parameters],
                             capture_output=True, text=True, timeout=30, cwd=root)
    output = process.stdout + process.stderr
    (output_folder / (name + '.log')).write_text(output + '\nexit=' + str(process.returncode) + '\n')
    sanitizer_failure = 'runtime error:' in output or 'ERROR: AddressSanitizer:' in output
    failed = process.returncode != 0 or sanitizer_failure
    print(('TEST:FAIL ' if failed else 'TEST:PASS ') + name)
    print(output.strip())
    results.append({'probe': name, 'exit': process.returncode,
                    'sanitizer_failure': sanitizer_failure, 'failed': failed})
(output_folder / ('asan-results.json' if args.asan else
                  'ubsan-results.json' if args.ubsan else
                  'results.json' if args.iteration2 else 'probe-results.json')).write_text(
    json.dumps(results, indent=2) + '\n')
failures = sum(row['failed'] for row in results)
print(f'TEST:SUMMARY pass={len(results) - failures} fail={failures}')
sys.exit(1 if failures else 0)
