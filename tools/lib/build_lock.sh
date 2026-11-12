#!/usr/bin/env bash

# Serialize all writers and consumers of shared generated ROM-mask artifacts.
# tools/build passes file descriptor 9 to its nested tools/build-pc invocation;
# reusing that open file description keeps the inner wrapper from deadlocking
# against its caller while standalone tools/build-pc still takes the same lock.

BUILD_LOCK_OWNER=0
BUILD_LOCK_SCOPE=""

acquire_build_lock() {
  local lock_file="$1"
  local scope="$2"
  local fd_target=""
  local inherited=0

  mkdir -p "$(dirname "$lock_file")"
  if [[ -e "/proc/$$/fd/9" ]]; then
    fd_target="$(readlink -f "/proc/$$/fd/9" 2>/dev/null || true)"
  fi
  if [[ "$fd_target" == "$lock_file" ]]; then
    inherited=1
  else
    exec 9>"$lock_file"
  fi

  BUILD_LOCK_SCOPE="$scope"
  printf 'BUILD:LOCK waiting scope=%s inherited=%s at_ns=%s\n' \
    "$scope" "$inherited" "$(date +%s%N)"
  flock -x 9
  if (( inherited == 0 )); then
    BUILD_LOCK_OWNER=1
    trap release_build_lock EXIT
  fi
  printf 'BUILD:LOCK acquired scope=%s inherited=%s at_ns=%s\n' \
    "$scope" "$inherited" "$(date +%s%N)"
}

release_build_lock() {
  (( BUILD_LOCK_OWNER == 1 )) || return 0
  local released_at
  released_at="$(date +%s%N)"
  flock -u 9
  printf 'BUILD:LOCK released scope=%s at_ns=%s\n' \
    "$BUILD_LOCK_SCOPE" "$released_at"
  BUILD_LOCK_OWNER=0
}
