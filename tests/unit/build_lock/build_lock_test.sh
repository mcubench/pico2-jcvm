#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
helper="${repo_root}/tools/lib/build_lock.sh"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT
lock_file="${tmp_dir}/shared-build.lock"

(
  exec 9>"${lock_file}"
  flock -x 9
  timeout 3s bash -c '
    source "$1"
    acquire_build_lock "$2" nested-child
    printf "NESTED:PASS\n"
  ' _ "${helper}" "${lock_file}" > "${tmp_dir}/nested.log"
)
grep -Fq 'acquired scope=nested-child inherited=1' "${tmp_dir}/nested.log"
grep -Fq 'NESTED:PASS' "${tmp_dir}/nested.log"

worker() {
  local name="$1"
  bash -c '
    source "$1"
    acquire_build_lock "$2" "$3"
    printf "%s start\n" "$3" >> "$4"
    sleep 0.1
    printf "%s end\n" "$3" >> "$4"
  ' _ "${helper}" "${lock_file}" "${name}" "${tmp_dir}/timeline.log" \
    > "${tmp_dir}/${name}.log"
}

worker A &
pid_a=$!
worker B &
pid_b=$!
wait "${pid_a}"
wait "${pid_b}"
awk '
  NR % 2 == 1 { active = $1; if ($2 != "start") exit 1 }
  NR % 2 == 0 { if ($1 != active || $2 != "end") exit 1 }
  END { if (NR != 4) exit 1 }
' "${tmp_dir}/timeline.log"

printf 'TEST:PASS build_lock_nested_and_serialized\n'
