#ifndef CONSTANT_H_DEFLATE
#define CONSTANT_H_DEFLATE

#define VEC 8                        // throughput per cycle
//#define LEN VEC                     // maximum match length
#define MAX_MATCH_DIST 32768
// Size of local buffer for prefetching; must be a multiple of VEC.
// Also must be larger than Latency * VEC to ensure result available.
#define MAX_OUTPUT_SIZE 64*1024-10
#define HASH_TABLE_BANKS (VEC*2)
#define BANK_OFFSETS 128
#define HASH_TABLE_SIZE (BANK_OFFSETS*HASH_TABLE_BANKS)

#include "ap_cint.h"
#if VEC==32
typedef uint256 vec_t;
typedef uint512 vec2_t;
#elif VEC==16
typedef uint128 vec_t;
typedef uint256 vec2_t;
#elif VEC==8
typedef uint64 vec_t;
typedef uint128 vec2_t;
#else
#error "VEC must be 8, 16 or 32!"
#endif

// Bit packing functions to avoid pointer casting bug in Xilinx tools.
void vec_t2chars(unsigned char dst[VEC], vec_t src);
void vec_t2ints(unsigned* dst, vec_t src);
vec_t chars2vec_t(unsigned char* src);
vec_t ints2vec_t(unsigned* src);
vec2_t short2vec2_t(unsigned short* src);
uint128 uint32_to_uint128(unsigned src[4]);
void uint128_to_uint32(unsigned dst[4], uint128 src);

void huffman_translate(unsigned short l, unsigned short d, uint128 *out_buf, uint128 *outl_buf);
void match_window(unsigned char data_window[2*VEC], unsigned input_pos,
    unsigned in_len, unsigned short l[VEC], unsigned short d[VEC]);
void deflate259(uint512* input, unsigned* in_len_p0, uint512* tree, unsigned* tree_len_p0, uint512* output, unsigned* out_len);

//#define COUNT_FREQ
#endif
