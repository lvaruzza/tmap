/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif
#include "../../util/tmap_alloc.h"
#include "../../util/tmap_sort.h"
#include "../../util/tmap_rand.h"
#include "../../seq/tmap_seq.h"
#include "../../index/tmap_refseq.h"
#include "../../index/tmap_bwtl.h"
#include "../../index/tmap_bwt.h"
#include "../../index/tmap_bwt_match.h"
#include "../../index/tmap_bwt_match_hash.h"
#include "../../index/tmap_sa.h"
#include "../../index/tmap_index.h"
#include "../../sw/tmap_sw.h"
#include "../../sw/tmap_fsw.h"
#include "../util/tmap_map_util.h"
#include "tmap_map2.h"
#include "tmap_map2_core.h"
#include "tmap_map2_chain.h"
#include "tmap_map2_aux.h"

#define __left_lt(a, b) ((a).end > (b).end)
TMAP_SORT_INIT(hit, tmap_map2_hit_t, __left_lt)

#define __hitG_lt(a, b) (((a).G + ((int)(a).n_seeds<<2)) > (b).G + ((int)(b).n_seeds<<2))
TMAP_SORT_INIT(hitG, tmap_map2_hit_t, __hitG_lt)

#define TMAP_MAP2_AUX_IS 0

int32_t
tmap_map2_aux_sa_pac_pos(const tmap_refseq_t *refseq, const tmap_bwt_t *bwt, const tmap_sa_t *sa, tmap_bwt_match_hash_t *hash, tmap_map2_aln_t *b, 
                         int32_t max_seed_hits, int32_t IS, int32_t min_as)
{
  int32_t i, j;
  int64_t n;
  //uint32_t seqid, pos;
  uint8_t is_rev;
  if(b->n == 0) return 1;
  if(NULL != bwt && NULL != sa) { // convert to chromosomal coordinates if suitable
      tmap_map2_aln_t *tmp_b;
      // copy over
      tmp_b = tmap_map2_aln_init();
      tmp_b->hits = b->hits;
      tmp_b->n = b->n;
      tmp_b->max = b->max;
      // nullify
      b->hits = NULL;
      b->n = b->max = 0;
      // go through tmp hits
      for(i = n = 0; i < tmp_b->n; ++i) {
          tmap_map2_hit_t *p = tmp_b->hits + i;
          if(0 == p->k && 0 == p->l && 0 == p->qlen) continue;
          if(p->l - p->k + 1 <= IS && TMAP_MAP2_MINUS_INF < p->G) n += p->l - p->k + 1;
          else if(p->G > min_as) ++n;
          if(max_seed_hits < n) {
              tmap_map2_aln_destroy(tmp_b);
              return 0;
          }
      }
      // realloc
      tmap_map2_aln_realloc(b, n);
      b->n = n;
      // copy over
      for(i = j = 0; i < tmp_b->n; ++i) {
          tmap_map2_hit_t *p = tmp_b->hits + i;
          if(0 == p->k && 0 == p->l && 0 == p->qlen) continue;
          if(p->l - p->k + 1 <= IS && TMAP_MAP2_MINUS_INF < p->G) {
              tmap_bwt_int_t k;
              for(k = p->k; k <= p->l; ++k) {
                  b->hits[j] = *p;
                  b->hits[j].k = tmap_sa_pac_pos_hash(sa, bwt, k, hash); // NB: keep zero based for now
                  b->hits[j].l = 0;
                  is_rev = (refseq->len < b->hits[j].k) ? 1 : 0;
                  b->hits[j].is_rev = is_rev;
                  // TODO: does this hurt/help?
                  b->hits[j].n_seeds = 1;
                  if (is_rev) b->hits[j].k -= p->qlen - 1;
                  ++j;
              }
          } else if(p->G > min_as) {
              b->hits[j] = *p;
              b->hits[j].k = tmap_sa_pac_pos_hash(sa, bwt, p->k, hash); // NB: keep zero based for now
              b->hits[j].l = 0;
              is_rev = (refseq->len < b->hits[j].k) ? 1 : 0;
              b->hits[j].is_rev = is_rev;
              b->hits[j].flag |= 0x1;
              // TODO: does this hurt/help?
              b->hits[j].n_seeds = p->l - p->k + 1;
              if (is_rev) b->hits[j].k -= p->qlen - 1;
              ++j;
          }
      }
      tmap_map2_aln_destroy(tmp_b);
  }
  return 1;
}

int32_t
tmap_map2_aux_resolve_duphits(const tmap_refseq_t *refseq, const tmap_bwt_t *bwt, const tmap_sa_t *sa, tmap_bwt_match_hash_t *hash, tmap_map2_aln_t *b, 
                              int32_t max_seed_hits, int32_t IS, int32_t min_as)
{
  int32_t i, j, n;
  //uint32_t seqid, pos;
  if(NULL == b) return 0;
  if(b->n == 0) return 0;

  // convert to SA positions
  tmap_map2_aux_sa_pac_pos(refseq, bwt, sa, hash, b, max_seed_hits, IS, min_as);
  if(b->n == 0) return 0;

  // sort
  tmap_sort_introsort(hitG, b->n, b->hits);

  // resolve dups
  for(i = 1; i < b->n; ++i) {
      tmap_map2_hit_t *p = b->hits + i;
      if(p->G <= min_as) break;
      for(j = 0; j < i; ++j) {
          tmap_map2_hit_t *q = b->hits + j;
          int32_t compatible = 1;
          if(q->G == 0) continue;
          if (p->is_rev != q->is_rev) continue; // hits from opposite strands are not duplicates
          if(p->l == 0 && q->l == 0) {
              int32_t qol = (p->end < q->end? p->end : q->end) - (p->beg > q->beg? p->beg : q->beg);
              if(qol < 0) qol = 0;
              if((double)qol / (p->end - p->beg) > TMAP_MAP2_MASK_LEVEL 
                 || (double)qol / (q->end - q->beg) > TMAP_MAP2_MASK_LEVEL) {
                  int64_t tol = (int64_t)(p->k + p->tlen < q->k + q->tlen? p->k + p->tlen : q->k + q->tlen)
                    - (int64_t)(p->k > q->k? p->k : q->k);
                  if((double)tol / p->tlen > TMAP_MAP2_MASK_LEVEL || (double)tol / q->tlen > TMAP_MAP2_MASK_LEVEL)
                    compatible = 0;
              }
          }
          if(!compatible) {
              p->G = TMAP_MAP2_MINUS_INF; 
              break;
          }
      }
  }
  n = i;
  for(i = j = 0; i < n; ++i) {
      if(b->hits[i].G <= min_as) continue;
      if(i != j) b->hits[j++] = b->hits[i];
      else ++j;
  }
  b->n = j;
  return b->n;
}

/*
   static int32_t 
   tmap_map2_aux_resolve_query_overlaps(tmap_map2_aln_t *b, double mask_level, int32_t min_as)
   {
   int32_t i, j, n;
   if(b->n == 0) return 0;
   tmap_sort_introsort(hitG, b->n, b->hits);
   { // choose a random one
   int G0 = b->hits[0].G;
   for (i = 1; i < b->n; ++i)
   if (b->hits[i].G != G0) break;
   j = (int)(i * drand48());
   if (j) {
   tmap_map2_hit_t tmp;
   tmp = b->hits[0]; b->hits[0] = b->hits[j]; b->hits[j] = tmp;
   }
   }
   for(i = 1; i < b->n; ++i) {
   tmap_map2_hit_t *p = b->hits + i;
   int32_t all_compatible = 1;
   if(p->G == min_as) break;
   for(j = 0; j < i; ++j) {
   tmap_map2_hit_t *q = b->hits + j;
   int64_t tol = 0;
   int32_t qol, compatible = 0;
   double fol;
   if(q->G == min_as) continue;
   qol = (p->end < q->end? p->end : q->end) - (p->beg > q->beg? p->beg : q->beg);
   if(qol < 0) qol = 0;
   if(p->l == 0 && q->l == 0) {
   tol = (int64_t)(p->k + p->tlen < q->k + q->tlen? p->k + p->tlen : q->k + q->tlen)
   - (p->k > q->k? p->k : q->k);
   if(tol < 0) tol = 0;
   }
   fol = (double)qol / (p->end - p->beg < q->end - q->beg? p->end - p->beg : q->end - q->beg);
   if(fol < mask_level || (tol > 0 && qol < p->end - p->beg && qol < q->end - q->beg)) compatible = 1;
   if(!compatible) {
   if(q->G2 < p->G) q->G2 = p->G;
   all_compatible = 0;
   }
   }
   if(!all_compatible) p->G = min_as;
   }
   n = i;
   for(i = j = 0; i < n; ++i) {
   if(b->hits[i].G == min_as) continue;
   if(i != j) b->hits[j++] = b->hits[i];
   else ++j;
   }
   b->n = j;
   return j;
   }
   */

static void
tmap_map2_hit_nullify(tmap_map2_hit_t * hit)
{
  hit->k = hit->l = hit->flag = hit->n_seeds = 0;
  hit->qlen = hit->tlen = hit->G = hit->G2 = hit->beg = hit->end = 0;
}

tmap_map2_aln_t*
tmap_map2_aln_init()
{
  return tmap_calloc(1, sizeof(tmap_map2_aln_t), "a");
}

void 
tmap_map2_aln_realloc(tmap_map2_aln_t *a, int32_t n)
{
  int32_t i;
  if(NULL == a) return;
  if(n < a->n) {
      for(i=n;i<a->n;i++) {
          tmap_map2_hit_nullify(&a->hits[i]);
      }
      a->n = n;
  }
  else if(a->max < n) { // allocate more memory
      i = a->max; // save for init
      a->max = (0 == a->max && n < 4) ? 4 : tmap_roundup32(n);
      // resize
      a->hits = tmap_realloc(a->hits, sizeof(tmap_map2_hit_t) * a->max, "a->hits");
      // init
      while(i < a->max) {
          tmap_map2_hit_nullify(&a->hits[i]);
          i++;
      }
  }
}

void 
tmap_map2_aln_destroy(tmap_map2_aln_t *a)
{
  if(NULL == a) return;
  free(a->hits);
  free(a);
}

// Note: this is the reverse, not the reverse compliment
#define tmap_map2_rseq_i(_refseq, _i) (tmap_refseq_seq_i(_refseq, _refseq->len-_i-1))

#define tmap_map2_aux_reverse_query(_query, _ql) \
  for(i=0;i<(_ql>>1);i++) { \
      uint8_t tmp = _query[i]; \
      _query[i] = _query[_ql-1-i]; \
      _query[_ql-1-i] = tmp; \
  }

static void 
tmap_map2_aux_merge_hits(tmap_map2_aln_t *b[2], int32_t l, int32_t is_reverse)
{
  int32_t i;
  tmap_map2_aln_realloc(b[0], b[0]->n + b[1]->n);
  for(i = 0; i < b[1]->n; ++i) {
      tmap_map2_hit_t *p = b[0]->hits + b[0]->n + i;
      *p = b[1]->hits[i];
      if(is_reverse) {
          int32_t x = p->beg;
          p->beg = l - p->end;
          p->end = l - x;
          //p->flag |= 0x10;
      }
  }
  b[0]->n += b[1]->n;
  tmap_map2_aln_destroy(b[1]);
  b[1] = NULL;
}

static tmap_map2_aln_t *
tmap_map2_aux_aln(tmap_map_opt_t *opt, 
                  tmap_refseq_t *target_refseq,
                  tmap_bwt_t *target_bwt, 
                  tmap_sa_t *target_sa, 
                  tmap_bwt_match_hash_t *target_hash,
                  tmap_string_t *seq[2], int32_t is_rev, tmap_map2_global_mempool_t *pool)
{
  tmap_map2_aln_t *b[2], **bb[2], **_b, *p;
  int32_t j, k;
      
  tmap_bwtl_t *query = tmap_bwtl_seq2bwtl(seq[0]->l, (uint8_t*)seq[0]->s);
  _b = tmap_map2_core_aln(opt, query, target_refseq, target_bwt, target_sa, target_hash, pool);
  tmap_bwtl_destroy(query);

  for(k = 0; k < 2; ++k) {
      bb[k] = tmap_calloc(2, sizeof(void*), "bb[k]");
      bb[k][0] = tmap_calloc(1, sizeof(tmap_map2_aln_t), "bb[k][0]");
      bb[k][1] = tmap_calloc(1, sizeof(tmap_map2_aln_t), "bb[k][1]");
  }

  for(k = 0; k < 2; ++k) { // separate _b into bb[2] based on the strand
      // resolve duplicates
      // _b[0] are "wide SA hits"
      // _b[1] are "narrow SA hits"
      if(1 == k || 0 != opt->narrow_rmdup) {
          tmap_map2_aux_resolve_duphits(target_refseq, target_bwt, target_sa, target_hash, _b[k], opt->max_seed_hits, opt->max_seed_intv, 0);
      }
      else {
          // only to packed reference coordinates
          if(0 == tmap_map2_aux_sa_pac_pos(target_refseq, target_bwt, target_sa, target_hash, _b[k], opt->max_seed_hits, INT32_MAX, INT32_MIN)) {
              // revert to resolving duplicates narrowly
              tmap_map2_aux_resolve_duphits(target_refseq, target_bwt, target_sa, target_hash, _b[k], opt->max_seed_hits, opt->max_seed_intv, 0);
          }
      }
      for(j = 0; j < _b[k]->n; ++j) {
          tmap_map2_hit_t *q;
          p = bb[_b[k]->hits[j].is_rev][k];
          if (p->n == p->max) {
              p->max = p->max? p->max<<1 : 8;
              p->hits = tmap_realloc(p->hits, p->max * sizeof(tmap_map2_hit_t), "p->hits");
          }
          q = &p->hits[p->n++];
          *q = _b[k]->hits[j];
          if (_b[k]->hits[j].is_rev) {
              int32_t x = q->beg;
              q->beg = seq[0]->l - q->end;
              q->end = seq[0]->l - x;
          }
      }
  }
  // free
  for(k = 0; k < 2; ++k) {
      free(_b[k]->hits);
      free(_b[k]);
  }
  free(_b);
  // resolve duplicates
  for(k = 0; k < 2; ++k) {
      // bb[*][0] are "wide SA hits"
      tmap_map2_aux_resolve_duphits(NULL, NULL, NULL, NULL, bb[k][0], opt->max_seed_hits, opt->max_seed_intv, 0);
      // bb[*][1] are "narrow SA hits"
      if(0 != opt->narrow_rmdup) {
          tmap_map2_aux_resolve_duphits(NULL, NULL, NULL, NULL, bb[k][1], opt->max_seed_hits, opt->max_seed_intv, 0);
      }
  }
  b[0] = bb[0][1]; b[1] = bb[1][1]; // bb[*][1] are "narrow SA hits"
  tmap_map2_chain_filter(opt, seq[0]->l, b); // NB: only unique seeds are chained

  // merge all hits
  for(k = 0; k < 2; ++k) {
      tmap_map2_aux_merge_hits(bb[k], seq[k]->l, 0); // bb[k][1] and bb[k][0] are merged into bb[k][0]
      b[k] = bb[k][0];
      free(bb[k]);		
  }
  tmap_map2_aux_merge_hits(b, seq[0]->l, 1); // b[1] and b[0] are merged into b[0]


  return b[0];
}

/* set ->flag to records the origin of the hit (to forward bwt or reverse bwt) */
static void 
tmap_map2_aux_flag_fr(tmap_map2_aln_t *b[2])
{
  int32_t i, j;
  if(NULL != b[0]) {
      for(i = 0; i < b[0]->n; ++i) {
          tmap_map2_hit_t *p = b[0]->hits + i;
          p->flag |= 0x10000;
      }
  }
  if(NULL != b[1]) {
      for(i = 0; i < b[1]->n; ++i) {
          tmap_map2_hit_t *p = b[1]->hits + i;
          p->flag |= 0x20000;
      }
  }
  if(NULL != b[0] && NULL != b[1] && 0 < b[0]->n && 0 < b[1]->n) {
      for(i = 0; i < b[0]->n; ++i) {
          tmap_map2_hit_t *p = b[0]->hits + i;
          for(j = 0; j < b[1]->n; ++j) {
              tmap_map2_hit_t *q = b[1]->hits + j;
              if(q->beg == p->beg && q->end == p->end && q->k == p->k && q->tlen == p->tlen && q->G == p->G) {
                  q->flag |= 0x30000; p->flag |= 0x30000;
                  break;
              }
          }
      }
  }
}

static tmap_map_sams_t *
tmap_map2_aux_store_hits(tmap_refseq_t *refseq, tmap_map_opt_t *opt, 
                         tmap_map2_aln_t *aln, int32_t seq_len)
{
  int32_t i, j;
  tmap_map_sams_t *sams = NULL;

  if(NULL == aln) return NULL;

  sams = tmap_map_sams_init(NULL);
  tmap_map_sams_realloc(sams, aln->n);

  for(i=j=0;i<aln->n;i++) {
      tmap_map2_hit_t *p = aln->hits + i;
      uint32_t seqid = 0, pos = 0;
      uint8_t strand;
      int32_t beg;
      tmap_map_sam_t *sam = &sams->sams[j];
      tmap_map_sam_init(sam);

      //strand = (p->flag & 0x10) ? 1 : 0;

      // skip over duplicate hits, or sub-optimal hits to the same location
      if(0 < i) {
          tmap_map2_hit_t *q = aln->hits + i - 1;
          if(q->flag == p->flag && q->k == p->k && q->G <= p->G && q->tlen <= p->tlen) continue;
      }
      /*
      if(i < aln->n) {
          tmap_map2_hit_t *q = aln->hits + i + 1;
          if(p->flag == q->flag && p->k == q->k && p->G <= q->G && p->tlen <= q->tlen) continue;
      }
      */

      // adjust for contig boundaries
      if(tmap_refseq_pac2real(refseq, p->k, p->tlen, &seqid, &pos, &strand) <= 0) {
          if(1 == strand) { // reverse
              if(tmap_refseq_pac2real(refseq, p->k + p->tlen - 1, 1, &seqid, &pos, &strand) <= 0) {
                  continue;
              }
              else {
                  // move to the contig and position
                  p->k = refseq->annos[seqid].offset+1;
                  p->tlen = (p->tlen < refseq->annos[seqid].len) ? p->tlen : refseq->annos[seqid].len;
              }
          }
          else {
              if(tmap_refseq_pac2real(refseq, p->k, 1, &seqid, &pos, &strand) <= 0) {
                  continue;
              }
              else {
                  // move to the contig and position
                  p->k = refseq->annos[seqid].offset+1;
                  p->tlen = (p->tlen < refseq->annos[seqid].len) ? p->tlen : refseq->annos[seqid].len;
              }
          }
      }

      // adjust based on where the hit was in the read
      beg = (1 == strand) ? (seq_len - p->end) : p->beg;
      pos = (pos <= beg) ? 1 : (pos - beg); // adjust pos

      if((p->flag & 0x1)) {
          p->G2 = p->G; // Note: the flag indicates a repetitive match, so we need to update the sub-optimal score
      }

      sam->strand = strand;
      sam->seqid = seqid;
      sam->pos = pos-1; // make it zero-based
      sam->algo_id = TMAP_MAP_ALGO_MAP2;
      sam->algo_stage = opt->algo_stage;
      sam->score = p->G;
      sam->score_subo = p->G2;
      sam->target_len = (seq_len < p->tlen) ? p->tlen : seq_len;

      // auxiliary data
      tmap_map_sam_malloc_aux(sam);
      sam->aux.map2_aux->XE = p->n_seeds;
      sam->aux.map2_aux->XF = p->flag >> 16;
      if(p->l) {
          sam->aux.map2_aux->XI = p->l - p->k + 1;
      }
      else {
          sam->aux.map2_aux->XI = 0;
      }
      sam->aux.map2_aux->flag = (p->flag & 0x1) ? 1 : 0;
      j++;
  }
  if(j != aln->n) {
      tmap_map_sams_realloc(sams, j);
  }

  return sams;
}

static void
tmap_map2_aux_core_update_opt(tmap_map_opt_t *dst, tmap_map_opt_t *src, int32_t qlen)
{
    double ll = log(qlen);
    int32_t i, k;
    *dst = *src;
    dst->score_thr = src->score_thr;
    if (dst->score_thr < ll * dst->length_coef) dst->score_thr = (int)(ll * dst->length_coef + .499);
    // set band width: the query length sets a boundary on the maximum band width
    k = (qlen * dst->score_match - 2 * dst->pen_gapo) / (2 * dst->pen_gape + dst->score_match);
    if(0 == dst->pen_gape) {
        i = (qlen * dst->score_match - dst->score_match - dst->score_thr);
    }
    else {
        i = (qlen * dst->score_match - dst->score_match - dst->score_thr) / dst->pen_gape;
    }
    if (k > i) k = i;
    if (k < 1) k = 1; // I do not know if k==0 causes troubles
    dst->bw = src->bw < k? src->bw : k;
}

tmap_map_sams_t *
tmap_map2_aux_core(tmap_map_opt_t *_opt,
                   tmap_seq_t *seqs[4],
                   tmap_refseq_t *refseq,
                   tmap_bwt_t *bwt,
                   tmap_sa_t *sa,
                   tmap_bwt_match_hash_t *hash,
                   tmap_rand_t *rand,
                   tmap_map2_global_mempool_t *pool)
{
  tmap_map_opt_t opt;
  tmap_seq_t *orig_seq = NULL;
  tmap_string_t *seq[2]={NULL, NULL};
  tmap_string_t *rseq[2]={NULL, NULL};
  tmap_map_sams_t *sams = NULL;
  tmap_map2_aln_t *b[2]={NULL,NULL};
  tmap_string_t *bases = NULL;
  int32_t i, k, l, num_n;

  opt = (*_opt);

  // sequence length
  bases = tmap_seq_get_bases(seqs[0]);
  l = bases->l;

  // update the local opt
  tmap_map2_aux_core_update_opt(&opt, _opt, l);

  // set opt->score_thr
  if(pool->max_l < l) { // then enlarge working space for tmap_sw_extend_core()
      int32_t tmp;
      if(0 == opt.pen_gape) {
          tmp = ((l + 1) / 2 * opt.score_match + opt.pen_gape) + l;
      }
      else {
          tmp = ((l + 1) / 2 * opt.score_match + opt.pen_gape) / opt.pen_gape + l;
      }
      pool->max_l = l;
      pool->aln_mem = tmap_realloc(pool->aln_mem, sizeof(uint8_t) * (tmp + 2) * 24, "pool->aln_mem");
  }

  // get the number of Ns
  for(i=num_n=0;i<l;i++) {
      uint8_t c = (uint8_t)tmap_nt_char_to_int[(int)bases->s[i]];
      if(c >= 4) num_n++; // FIXME: ambiguous bases are not properly handled
  }

  // will we always be lower than the score threshold
  if((l*opt.score_match) + (num_n*opt.pen_mm) < opt.score_thr) {
      return tmap_map_sams_init(NULL);
  }
  
  // save sequences
  seq[0] = tmap_seq_get_bases(seqs[0]); 
  seq[1] = tmap_seq_get_bases(seqs[1]);
  rseq[0] = tmap_seq_get_bases(seqs[2]); 
  rseq[1] = tmap_seq_get_bases(seqs[3]);

  // handle ambiguous bases
  if(0 < num_n) {
      // save original to de-randomize later
      orig_seq = tmap_seq_clone(seqs[0]);

      // randomize
      for(i=0;i<l;i++) {
          uint8_t c = (uint8_t)bases->s[i];
          if(c >= 4) {
              c = (int)(tmap_rand_get(rand) * 4); // FIXME: ambiguous bases are not properly handled
              seq[0]->s[i] = c; // original
              seq[1]->s[l-1-i] = 3 - c; // reverse compliment
              rseq[0]->s[l-1-i] = 3 - c; // reverse compliment
              rseq[1]->s[i] = c; // original
              //rseq[0]->s[l-1-i] = c; // reverse 
              //rseq[1]->s[i] = 3 - c; // compliment
          }
      }
  }

  // alignment
  b[0] = tmap_map2_aux_aln(&opt, refseq, bwt, sa, hash, seq, 0, pool);
  for(k = 0; k < b[0]->n; ++k) {
      if(b[0]->hits[k].n_seeds < opt.seeds_rev) break;
  } 
  if(k < b[0]->n) {
      b[1] = tmap_map2_aux_aln(&opt, refseq, bwt, sa, hash, rseq, 1, pool);
      for(i = 0; i < b[1]->n; ++i) {
          tmap_map2_hit_t *p = b[1]->hits + i;
          int x = p->beg;
          p->flag ^= 0x10, p->is_rev ^= 1; // flip the strand
          p->beg = l - p->end;
          p->end = l - x;
          if(p->l == 0) {
              if(refseq->len * 2 < (p->k + p->tlen)) p->k = 0;
              else p->k = 2 * refseq->len - (p->k + p->tlen);
          }
      }
      tmap_map2_aux_merge_hits(b, l, 0);
  } else b[1] = 0;
  
  // set the flag to forward/reverse
  tmap_map2_aux_flag_fr(b);
  
  // tlen may overestimated due to not counting insertions properly, bound it!
  for(i = 0; i < b[0]->n; ++i) {
      if(refseq->len * 2 <= b[0]->hits[i].k + b[0]->hits[i].tlen) {
          b[0]->hits[i].tlen = (refseq->len * 2) - b[0]->hits[i].k;
      }
      else if(b[0]->hits[i].k < refseq->len && refseq->len <= b[0]->hits[i].k + b[0]->hits[i].tlen) {
          b[0]->hits[i].tlen = refseq->len - b[0]->hits[i].k;
      }
  }
  
  // make one-based for pac2real
  for(i = 0; i < b[0]->n; ++i) {
      b[0]->hits[i].k++;
  }
  
  // store in SAM 
  sams = tmap_map2_aux_store_hits(refseq, &opt, b[0], l);

  // free
  tmap_map2_aln_destroy(b[0]);

  // revert ambiguous bases 
  if(0 < num_n) {
      // de-randomize
      bases = tmap_seq_get_bases(orig_seq);
      for(i=0;i<l;i++) {
          uint8_t c = (uint8_t)bases->s[i];
          if(c >= 4) { 
              // NB: always keep them at "4"
              seq[0]->s[i] = c; // original
              seq[1]->s[l-1-i] = c; // reverse compliment
              rseq[0]->s[l-1-i] = c; // reverse compliment
              rseq[1]->s[i] = c; // original
              //rseq[0]->s[l-1-i] = c; // reverse 
              //rseq[1]->s[i] = 3 - c; // compliment
          }
      }
      tmap_seq_destroy(orig_seq);
  }

  return sams;
}
