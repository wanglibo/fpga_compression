#ifndef CONSTANT_H
#define CONSTANT_H
#include <ap_int.h>

////////////////////////////////////////////////////////////////////////////
//
//  Macros and definitaions.
//
////////////////////////////////////////////////////////////////////////////
// Design parameters and switches
// Parallelism
#define VEC 8
// Match window size: must be a multiple of VEC
#define LEN 16
#if LEN%VEC != 0
#error "LEN must be a multiple of VEC."
#endif
#define MAX_MATCH_DIST 32768
#define BANK_OFFSETS 128
#define HASH_TABLE_BANKS (VEC*2)

#define HASH_TABLE_SIZE (BANK_OFFSETS*HASH_TABLE_BANKS)
#if VEC!=8 && VEC!=16 && VEC!=32
#error "VEC must be 8, 16 or 32!"
#endif

typedef ap_uint<LEN*8> len_t;

typedef ap_uint<VEC*8> vec_t;
typedef ap_uint<VEC*16> vec_2t;
typedef ap_uint<VEC*32> vec_4t;
typedef ap_uint<VEC*64> vec_8t;
typedef ap_uint<8> uint8;
typedef ap_uint<16> uint16;
typedef ap_uint<32> uint32;
typedef ap_uint<64> uint64;
typedef ap_uint<128> uint128;
typedef ap_uint<512> uint512;

////////////////////////////////////////////////////////////////////////////
//
//  Helper functions to avoid pointer casting related error in HLS.
//
////////////////////////////////////////////////////////////////////////////
void uint64_to_uint16(uint16 dst[4], uint64 src);
uint64 uint16_to_uint64(uint16 src[4]);

////////////////////////////////////////////////////////////////////////////
//
//  Other functions.
//
////////////////////////////////////////////////////////////////////////////
void huffman_translate(uint16 l, uint16 d, uint16 *total_len,
                       uint64 *out_buf, uint64 *outl_buf);
#endif  // CONSTANT
