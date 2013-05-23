/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <config.h>
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#include <unistd.h>
#endif
#include <unistd.h>
#include "../../util/tmap_error.h"
#include "../../util/tmap_alloc.h"
#include "../../util/tmap_definitions.h"
#include "../../util/tmap_progress.h"
#include "../../util/tmap_sam_convert.h"
#include "../../seq/tmap_seq.h"
#include "../../index/tmap_refseq.h"
#include "../../index/tmap_bwt_gen.h"
#include "../../index/tmap_bwt.h"
#include "../../index/tmap_bwt_match.h"
#include "../../index/tmap_bwt_match_hash.h"
#include "../../index/tmap_sa.h"
#include "../../index/tmap_index.h"
#include "../../io/tmap_seq_io.h"
#include "../../server/tmap_shm.h"
#include "../../sw/tmap_sw.h"
#include "../util/tmap_map_stats.h"
#include "../util/tmap_map_util.h"
#include "../tmap_map_driver.h"
#include "tmap_map3_aux.h"
#include "tmap_map3.h"

int32_t
tmap_map3_get_seed_length(uint64_t ref_len)
{
  int32_t k = 0;
  while(0 < ref_len) {
      ref_len >>= 2; // divide by four
      k++;
  }
  k += 2;
  return k;
}

static inline int32_t 
tmap_map3_mapq(tmap_map_sams_t *sams, int32_t seq_len, tmap_map_opt_t *opt)
{
  int32_t i;
  int32_t n_best = 0;
  int32_t best_score, cur_score, best_subo;
  int32_t n_seeds = 0, tot_seeds = 0;
  int32_t mapq;
  int32_t score_thr, score_match;

  score_thr = opt->score_thr;
  score_match = opt->score_match;

  best_score = INT32_MIN;
  best_subo = INT32_MIN;
  n_best = 0;
  for(i=0;i<sams->n;i++) {
      cur_score = sams->sams[i].score;
      tot_seeds += sams->sams[i].n_seeds;
      if(best_score < cur_score) {
          best_subo = best_score;
          best_score = cur_score;
          n_best = 1;
          n_seeds = sams->sams[i].n_seeds;
      }
      else if(cur_score == best_score) { // qual
          n_best++;
      }
      else {
          if(best_subo < cur_score) {
              best_subo = cur_score;
          }
          cur_score = sams->sams[i].score_subo;
          if(best_subo < cur_score) {
              best_subo = cur_score;
          } 
      }
  }
  if(1 < n_best) {
      mapq = 0;
  }
  else {
      double c = 0.0;
      c = n_seeds / (double)tot_seeds;
      if(best_subo < score_thr) best_subo = score_thr;
      mapq = (int32_t)(c * (best_score - best_subo) * (250.0 / best_score + 0.03 / score_match) + .499);
      if(mapq > 250) mapq = 250;
      if(mapq <= 0) mapq = 1;
  }
  for(i=0;i<sams->n;i++) {
      cur_score = sams->sams[i].score;
      if(cur_score == best_score) { // qual
          sams->sams[i].mapq = mapq;
      }
      else {
          sams->sams[i].mapq = 0;
      }
  }

  return 0;
}

// thread data
typedef struct {
    void *ptr; // dummy ptr
} tmap_map3_thread_data_t;

int32_t
tmap_map3_init(void **data, tmap_refseq_t *refseq, tmap_map_opt_t *opt)
{
  // adjust opt for opt->score_match
  opt->score_thr *= opt->score_match;

  // set the seed length
  if(-1 == opt->seed_length) {
      opt->seed_length = tmap_map3_get_seed_length(refseq->len);
      opt->seed_length_set = 1;
      tmap_progress_print("setting the seed length to %d in map3", opt->seed_length);
  }

  return 0;
}

int32_t
tmap_map3_thread_init(void **data, 
                      tmap_map_opt_t *opt)
{
  tmap_map3_thread_data_t *d = NULL;

  d = tmap_calloc(1, sizeof(tmap_map3_thread_data_t), "d");

  (*data) = (void*)d;

  return 0;
}

tmap_map_sams_t *
tmap_map3_thread_map(void **data, tmap_seq_t **seqs, tmap_index_t *index, tmap_bwt_match_hash_t *hash, tmap_rand_t *rand, tmap_map_opt_t *opt)
{
  tmap_map_sams_t *sams = NULL;
  int32_t seq_len;
  //tmap_map3_thread_data_t *d = (tmap_map3_thread_data_t*)(*data);

  seq_len = tmap_seq_get_bases_length(seqs[0]);
  
  if((0 < opt->min_seq_len && seq_len < opt->min_seq_len)
     || (0 < opt->max_seq_len && opt->max_seq_len < seq_len)) {
      return tmap_map_sams_init(NULL);
  }
  
  // align
  sams = tmap_map3_aux_core(seqs[3], index->refseq, index->bwt, index->sa, hash, opt);

  return sams;
}

int32_t
tmap_map3_thread_cleanup(void **data, tmap_map_opt_t *opt)
{
  tmap_map3_thread_data_t *d = (tmap_map3_thread_data_t*)(*data);

  free(d);
  (*data) = NULL;
  return 0;
}

static void
tmap_map3_core(tmap_map_driver_t *driver)
{

  // add this algorithm
  tmap_map_driver_add(driver,
                      tmap_map3_init, 
                      tmap_map3_thread_init, 
                      tmap_map3_thread_map, 
                      tmap_map3_thread_cleanup,
                      NULL,
                      driver->opt);
  

  // run the driver
  tmap_map_driver_run(driver);
}

int 
tmap_map3_main(int argc, char *argv[])
{
  tmap_map_driver_t *driver = NULL;

  // init
  driver = tmap_map_driver_init(TMAP_MAP_ALGO_MAP3, tmap_map3_mapq);
  driver->opt->algo_stage = 1;

  // get options
  if(1 != tmap_map_opt_parse(argc, argv, driver->opt) // options parsed successfully
     || argc != optind  // all options should be used
     || 1 == argc) { // some options should be specified
      return tmap_map_opt_usage(driver->opt);
  }
  else { 
      // check command line arguments
      tmap_map_opt_check(driver->opt);
  }

  // run map3
  tmap_map3_core(driver);

  // destroy 
  tmap_map_driver_destroy(driver);

  tmap_progress_print2("terminating successfully");

  return 0;
}
