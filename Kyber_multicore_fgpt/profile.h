#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

/* ================= KEYGEN ================= */

typedef struct {
    /* Core0 local work */
    uint64_t noise;
    uint64_t ntt;
    uint64_t add_reduce;

    /* Parallel phase wall times */
    uint64_t phase_hash_gena;
    uint64_t phase_matmul;
    uint64_t phase_pack;

    /* Core1 actual compute times */
    uint64_t core1_hash_gena;
    uint64_t core1_matmul;
    uint64_t core1_pack;

} keygen_profile_t;

/* ================= ENC ================= */

typedef struct {
    uint64_t unpack;
    uint64_t gen_at;
    uint64_t noise;
    uint64_t ntt;
    uint64_t invntt;
    uint64_t add_reduce;
    uint64_t pack;

    /* Parallel phase wall times */
    uint64_t phase_frommsg;
    uint64_t phase_matmul;

    /* Core1 actual compute */
    uint64_t core1_frommsg;
    uint64_t core1_matmul;

} enc_profile_t;

/* ================= DEC ================= */

typedef struct {
    uint64_t unpack;
    uint64_t ntt;
    uint64_t matmul;
    uint64_t invntt;
    uint64_t sub_reduce;
    uint64_t tomsg;
} dec_profile_t;

extern keygen_profile_t kg_prof;
extern enc_profile_t enc_prof;
extern dec_profile_t dec_prof;

void profile_reset(void);

#endif
