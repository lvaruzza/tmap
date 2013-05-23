/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef TMAP_BWT_AUX_H
#define TMAP_BWT_AUX_H
/*
 * This is to incorporate the BWT optimizations from Roel Kluin:
 * https://github.com/RoelKluin/bwa
 */

#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>

// TODO
extern const uint32_t tmap_bwt_aux_occ_mask[16];
extern const uint64_t tmap_bwt_aux_n_mask[5];
extern __m128i tmap_bwt_aux_n_mask_128[3];
extern __m64 tmap_bwt_aux_n_mask_64[9];

/*!
  precomputes values required for the optimized bwt lookups
  */
void
tmap_bwt_aux_set_mask();

/*! 
  calculates the SA interval given the previous SA interval and the next base
  @param  bwt  pointer to the bwt structure 
  @param  k    previous lower occurrence
  @param  l    previous upper occurrence
  @param  c    base in two-bit integer format
  @details     more efficient version of bwt_occ but requires that k <= l (not checked)
  */
inline tmap_bwt_int_t 
tmap_bwt_aux_2occ(const tmap_bwt_t *bwt, tmap_bwt_int_t k, tmap_bwt_int_t *l, uint8_t c);

#endif // TMAP_BWT_AUX_H
