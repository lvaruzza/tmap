#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <config.h>

#include "../util/tmap_error.h"
#include "../util/tmap_alloc.h"
#include "../util/tmap_progress.h"
#include "../io/tmap_file.h"
#include "tmap_bwt.h"
#include "tmap_bwt_match.h"
#include "tmap_bwt_check.h"

//#define TMAP_BWT_CHECK_DEBUG 1

#ifdef TMAP_BWT_CHECK_DEBUG 
#include "tmap_refseq.h"
#include "tmap_sa.h"
#include "tmap_index.h"
// NB: global variable!
static tmap_index_t *tmap_bwt_index;
#endif

void 
tmap_bwt_check_core2(tmap_bwt_t *bwt, int32_t length, int32_t print_msg, int32_t print_sa, int32_t warn)
{
  uint8_t *seqs[2] = {NULL,NULL};
  char *str = NULL;
  int32_t i, asymmetric, k, l;
  uint64_t hash_j;
  int64_t sum, j;
  tmap_bwt_match_occ_t sa;
  tmap_bwt_int_t n[2];

  for(i=1;i<=length;i++) {
      seqs[0] = tmap_calloc(i, sizeof(uint8_t), "seqs[0]");
      seqs[1] = tmap_calloc(i, sizeof(uint8_t), "seqs[1]");
      str = tmap_calloc(i+1, sizeof(char), "str");
      for(j=0;j<i;j++) {
          seqs[1][j] = 3;
      }

      asymmetric = 0;
      j = 0;
      hash_j = sum = 0;
      while(1) {
          if(i == j) {
              for(k=0;k<i;k++) {
                  seqs[1][k] = 3 - seqs[0][i-k-1];
              }
              for(k=0;k<2;k++) {
                  //n[k] = tmap_bwt_match_exact(bwt, i, seqs[k], &sa);
                  n[k] = tmap_bwt_match_exact_reverse(bwt, i, seqs[k], &sa);
                  if(0 == k) {
                      if(0 < n[k] && TMAP_BWT_INT_MAX != sa.k && sa.k <= sa.l) {
                          sum += n[k];
                      }
                      if(1 == print_msg && 1 == print_sa) {
                          for(l=0;l<i;l++) {
                              str[l] = "ACGTN"[seqs[k][l]];
                          }
                          if(0 < n[k] && TMAP_BWT_INT_MAX != sa.k && sa.k <= sa.l) {
                              tmap_progress_print2("%s\t%llu\t%llu\t%llu", str, sa.k, sa.l, n[k]);
#ifdef TMAP_BWT_CHECK_DEBUG 
                              while(sa.k <= sa.l) {
                                  uint32_t seqid, pos;
                                  uint8_t strand;
                                  tmap_bwt_int_t pacpos = tmap_sa_pac_pos(tmap_bwt_index->sa, bwt, sa.k);
                                  if(0 < tmap_refseq_pac2real(tmap_bwt_index->refseq, pacpos, 1, &seqid, &pos, &strand)) {
                                      tmap_progress_print2("%s\t%llu\t%llu\t%llu\t%c%u:%u", str, sa.k, sa.l, n[k],
                                                           "+-"[strand], seqid, pos);
                                  }
                                  else {
                                      tmap_progress_print2("%s\t%llu\t%llu\t%llu\t%c%u:%u", str, sa.k, sa.l, n[k],
                                                           '?', 0, 0);
                                  }
                                  sa.k++;
                              }
#endif
                          }
                          else {
                              tmap_progress_print2("%s\tNA\tNA\tNA", str);
                          }
                      }
                  }
              }
              if(0 == asymmetric && n[0] != n[1]) {
                  asymmetric = 1;
                  //fprintf(stderr, "n[0]=%u n[1]=%u\n", n[0], n[1]);
                  tmap_error("Asymmetry found", Warn, OutOfRange);
              }

              j--;
              while(0 <= j && 3 == seqs[0][j]) {
                  seqs[0][j] = 0;
                  hash_j >>= 2;
                  j--;
              }
              if(j < 0) break;
              seqs[0][j]++;
              hash_j++;
              j++;
          }
          else {
              hash_j <<= 2;
              j++;
          }
      }

      free(seqs[0]);
      free(seqs[1]);
      free(str);

      j = (sum == (bwt->seq_len - i + 1)) ? 0 : 1; // j==1 on fail
      if(1 == print_msg) {
          if(0 == j) tmap_progress_print2("%d-mer validation passed", i);
          else tmap_progress_print2("%d-mer validation failed: observed (%llu) != expected (%llu)\n", i, sum, bwt->seq_len - i + 1);
      }
      if(0 == warn && 1 == j) {
          tmap_error("inconsistency found in the BWT", Exit, OutOfRange);
      }
  }
}

void 
tmap_bwt_check_core(const char *fn_fasta, int32_t length, int32_t print_sa, int32_t use_hash)
{
  tmap_bwt_t *bwt;
  int32_t hash_width = 0;

#ifdef TMAP_BWT_CHECK_DEBUG 
  tmap_bwt_index = tmap_index_init(fn_fasta, 0);
  bwt = tmap_bwt_index->bwt;
#else
  bwt = tmap_bwt_read(fn_fasta);
#endif
  if(0 == use_hash) {
      hash_width = bwt->hash_width;
      bwt->hash_width = 0;
  }


  tmap_bwt_check_core2(bwt, length, 1, print_sa, 1);

  if(0 == use_hash) {
      bwt->hash_width = hash_width;
  }
#ifdef TMAP_BWT_CHECK_DEBUG 
  tmap_index_destroy(tmap_bwt_index);
#else
  tmap_bwt_destroy(bwt);
#endif
}

int 
tmap_bwt_check(int argc, char *argv[])
{
  int c, length = 12, print_sa = 0, use_hash = 0;

  tmap_progress_set_verbosity(1); 

  while ((c = getopt(argc, argv, "l:pH")) >= 0) {
      switch (c) {
        case 'l': length = atoi(optarg); break;
        case 'p': print_sa = 1; break;
        case 'H': use_hash = 1; break;
        default: return 1;
      }
  }

  if (optind + 1 > argc) {
      tmap_file_fprintf(tmap_file_stderr, "Usage: %s %s [options] <in.fasta>\n", PACKAGE, argv[0]);
      tmap_file_fprintf(tmap_file_stderr, "Options:\n");
      tmap_file_fprintf(tmap_file_stderr, "         -l INT    the kmer length to check\n");
      tmap_file_fprintf(tmap_file_stderr, "         -p        print out the SA intervals for each kmer\n");
      tmap_file_fprintf(tmap_file_stderr, "         -H        use the hash to compute the SA intervals\n");
      tmap_file_fprintf(tmap_file_stderr, "\n");
      return 1;
  }
  tmap_bwt_check_core(argv[optind], length, print_sa, use_hash);
  return 0;
}
