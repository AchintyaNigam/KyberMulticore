#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

typedef struct {
    uint64_t hash_gena;
    uint64_t noise;
    uint64_t ntt;
    uint64_t matmul;
    uint64_t add_reduce;
    uint64_t pack;
} keygen_profile_t;

typedef struct {
    uint64_t unpack;
    uint64_t frommsg;
    uint64_t gen_at;
    uint64_t noise;
    uint64_t matmul;
    uint64_t invntt;
    uint64_t ntt;
    uint64_t add_reduce;
    uint64_t pack;
} enc_profile_t;

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

extern uint32_t prof_runs;

#endif
