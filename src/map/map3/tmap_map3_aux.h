/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef TMAP_MAP3_AUX_H
#define TMAP_MAP3_AUX_H

#include "tmap_map3.h"

/*!
  Holds the seed matches
  */
typedef struct {
    tmap_bwt_int_t k; /*!< the lower SA interval */
    tmap_bwt_int_t l; /*!< the upper SA interval */
    uint16_t start; /*!< the # of bases from the start of the read (0-based) */
    uint16_t seed_length; /*!< the effective seed length */
} tmap_map3_aux_seed_t;

/*!
  Core mapping routine
  @param  seq            the sequence to align (complimented)
  @param  refseq         the reference sequence structure (forward)
  @param  bwt            the BWT structure 
  @param  sa             the SA structure 
  @param  hash           the occurrence hash
  @param  opt            the program options
  @return                the alignments
  the sequences should be in 2-bit format
  */
tmap_map_sams_t *
tmap_map3_aux_core(tmap_seq_t *seq,
                   tmap_refseq_t *refseq,
                   tmap_bwt_t *bwt,
                   tmap_sa_t *sa,
                   tmap_bwt_match_hash_t *hash,
                   tmap_map_opt_t *opt);

#endif // TMAP_MAP3_AUX_H
