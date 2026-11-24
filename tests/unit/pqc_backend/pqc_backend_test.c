/* Line-oriented driver for tests/unit/test_pqc_backend.py.
 * Each stdin line: "<op> <set> <hex>..."; each stdout line: "<rc> <hex>..."
 * followed by the arena high-water mark ("peak=<bytes>"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pqc_arena.h"
#include "pqc_backend.h"

int choupi_pqc_progress(void) { return 1; }

static size_t unhex(const char *hex, uint8_t **out)
{
  size_t n = strlen(hex) / 2;
  *out = malloc(n ? n : 1);
  for (size_t i = 0; i < n; i++)
  {
    unsigned v;
    sscanf(hex + 2 * i, "%2x", &v);
    (*out)[i] = (uint8_t)v;
  }
  return n;
}

static void puthex(const uint8_t *p, size_t n)
{
  putchar(' ');
  for (size_t i = 0; i < n; i++) printf("%02X", p[i]);
}

int main(void)
{
  char *line = NULL;
  size_t cap = 0;
  static uint8_t a[8192], b[8192];
  while (getline(&line, &cap, stdin) > 0)
  {
    char *argv[6] = {0};
    int argc = 0;
    for (char *tok = strtok(line, " \n"); tok && argc < 6; tok = strtok(NULL, " \n"))
      argv[argc++] = tok;
    if (argc < 2) continue;
    int set = atoi(argv[1]);
    uint8_t *in[4] = {0};
    size_t len[4] = {0};
    for (int i = 2; i < argc; i++) len[i - 2] = unhex(argv[i], &in[i - 2]);
    /* Arena sized exactly as the library documents, so an overrun fails. */
    size_t scratch = strncmp(argv[0], "mlkem", 5) == 0 ? pqc_mlkem_scratch(set)
                                                         : pqc_mldsa_scratch(set);
    void *arena = malloc(scratch + 1);
    pqc_arena_begin(arena, scratch);
    pqc_mlkem_sizes_t ks = {0};
    pqc_mldsa_sizes_t ds = {0};
    pqc_mlkem_sizes(set, &ks);
    pqc_mldsa_sizes(set, &ds);
    int rc = -1;
    if (!strcmp(argv[0], "mlkem_keygen"))
    {
      rc = pqc_mlkem_keypair(set, a, b, in[0]);
      printf("%d", rc);
      puthex(a, ks.ek);
      puthex(b, ks.dk);
    }
    else if (!strcmp(argv[0], "mlkem_encaps"))
    {
      rc = pqc_mlkem_encaps(set, a, b, in[0], in[1]);
      printf("%d", rc);
      puthex(a, ks.ct);
      puthex(b, 32);
    }
    else if (!strcmp(argv[0], "mlkem_decaps"))
    {
      rc = pqc_mlkem_decaps(set, a, in[1], in[0]);
      printf("%d", rc);
      puthex(a, 32);
    }
    else if (!strcmp(argv[0], "mlkem_check_ek"))
      printf("%d", pqc_mlkem_check_ek(set, in[0]));
    else if (!strcmp(argv[0], "mlkem_check_dk"))
      printf("%d", pqc_mlkem_check_dk(set, in[0]));
    else if (!strcmp(argv[0], "mldsa_keygen"))
    {
      rc = pqc_mldsa_keypair(set, a, b, in[0]);
      printf("%d", rc);
      puthex(a, ds.pk);
      puthex(b, ds.sk);
    }
    else if (!strcmp(argv[0], "mldsa_pk_from_sk"))
    {
      rc = pqc_mldsa_pk_from_sk(set, a, in[0]);
      printf("%d", rc);
      puthex(a, ds.pk);
    }
    else if (!strcmp(argv[0], "mldsa_sign_mu"))
    { /* sk mu rnd */
      rc = pqc_mldsa_sign_mu(set, a, in[1], in[2], in[0]);
      printf("%d", rc);
      puthex(a, ds.sig);
    }
    else if (!strcmp(argv[0], "mldsa_verify_mu"))
      printf("%d", pqc_mldsa_verify_mu(set, in[2], in[1], in[0])); /* pk mu sig */
    else if (!strcmp(argv[0], "mldsa_mu_sk") || !strcmp(argv[0], "mldsa_mu_pk"))
    { /* key ctx msg ("-" = empty) */
      uint8_t tr[64], mu[64];
      if (argv[0][9] == 's')
        memcpy(tr, in[0] + 64, 64);
      else
        pqc_mldsa_tr_from_pk(in[0], len[0], tr);
      pqc_shake256_t st;
      pqc_mldsa_mu_init(&st, tr, in[1], (uint8_t)len[1]);
      /* Absorb in uneven chunks, as successive Signature.update calls do. */
      for (size_t off = 0, step = 1; off < len[2]; off += step, step = step * 3 + 1)
        pqc_shake256_absorb(&st, in[2] + off, step < len[2] - off ? step : len[2] - off);
      pqc_shake256_final(&st, mu, 64);
      printf("0");
      puthex(mu, 64);
    }
    else
      printf("unknown");
    printf(" peak=%zu\n", pqc_arena_end());
    fflush(stdout);
    free(arena);
    for (int i = 0; i < 4; i++) free(in[i]);
  }
  free(line);
  return 0;
}
