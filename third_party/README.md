# Vendored third-party code

| Directory | Upstream | Revision | License |
|---|---|---|---|
| `mlkem-native/` | <https://github.com/pq-code-package/mlkem-native> | `438f0da19dc3` | Apache-2.0 OR ISC OR MIT |
| `mldsa-native/` | <https://github.com/pq-code-package/mldsa-native> | `d2149e63337e` | Apache-2.0 OR ISC OR MIT |

Both were copied unchanged from the monobuild trees used by the sibling
`pico2-crypto-bench` project (its benchmark-only `MLD_CONFIG_REDUCE_RAM` edit to
`mldsa_native_config.h` was reverted). Project settings live in
`choupi/src/pqc/pqc_mlkem_config.h` and `pqc_mldsa_config.h`, selected with
`MLK_CONFIG_FILE` / `MLD_CONFIG_FILE`; do not edit the vendored files.
