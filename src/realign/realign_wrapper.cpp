/* Copyright (C) 2013 Ion Torrent Systems, Inc. All Rights Reserved */
extern "C"
{
#include "realign_wrapper.h"
}
#include "realign_wrapper_imp.h"


RealignProxy* realigner_create ()
{
    return createRealigner ();
}

RealignProxy* realigner_create_spec (unsigned reserve_size, unsigned clipping_size)
{
    return createRealigner (reserve_size, clipping_size);
}

void realigner_destroy (RealignProxy* r)
{
    delete r;
}

void realigner_set_verbose (RealignProxy* r, uint8_t verbose)
{
    r->set_verbose ((bool) verbose);
}

void realigner_set_debug (RealignProxy* r, uint8_t debug)
{
    r->set_debug ((bool) debug);
}

uint8_t  realigner_invalid_input_cigar (RealignProxy* r)
{
    return r->invalid_input_cigar () ? 1 : 0;
}

// parameters setup
void realigner_set_scores (RealignProxy* r, int mat, int mis, int gip, int gep)
{
    r->set_scores (mat, mis, gip, gep);
}

void realigner_set_bandwidth (RealignProxy* r, int bandwidth)
{
    r->set_bandwidth (bandwidth);
}

void realigner_set_clipping (RealignProxy* r, enum CLIPTYPE clipping)
{
    r->set_clipping (clipping);
}

// alignment setup and run
uint8_t realigner_compute_alignment (RealignProxy* r, 
                                     const char* q_seq,
				     uint32_t q_len,
                                     const char* r_seq, 
				     uint32_t r_len,
                                     int r_pos, 
                                     uint8_t forward, 
                                     const uint32_t* cigar, 
                                     unsigned cigar_sz, 
                                     uint32_t** cigar_dest, 
                                     unsigned* cigar_dest_sz, 
                                     int* new_r_pos,
                                     uint64_t* num_realign_already_perfect,
				     uint64_t* num_realign_not_clipped,
                                     uint64_t* num_realign_sw_failures,
                                     uint64_t* num_realign_unclip_failures)
{
    bool already_perfect = false;
    bool clip_failed = false;
    bool alignment_failed = false;
    bool unclip_failed = false;
    
    bool result = r->compute_alignment (q_seq, q_len, r_seq, r_len, r_pos, (bool) forward, cigar, cigar_sz, *cigar_dest, *cigar_dest_sz, *new_r_pos, already_perfect, clip_failed, alignment_failed, unclip_failed);
    
    if (num_realign_already_perfect && already_perfect) ++(*num_realign_already_perfect);
    if (num_realign_not_clipped && clip_failed) ++(*num_realign_not_clipped);
    if (num_realign_sw_failures && alignment_failed) ++(*num_realign_sw_failures);
    if (num_realign_unclip_failures && unclip_failed) ++(*num_realign_unclip_failures);
    
    return result;
}

char* qry_mem (struct RealignProxy* r, unsigned len)
{
    return r->qry_buf (len);
}

char* ref_mem (struct RealignProxy* r, unsigned len)
{
    return r->ref_buf (len);
}
