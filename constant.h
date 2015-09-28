#ifndef CONSTANT_H_DEFLATE
#define CONSTANT_H_DEFLATE

#define VEC 16                        // throughput per cycle
//#define LEN VEC                     // maximum match length
#define MAX_MATCH_DIST 32768
// Size of local buffer for prefetching; must be a multiple of VEC.
// Also must be larger than Latency * VEC to ensure result available.
#define MAX_OUTPUT_SIZE 64*1024-10
#define HASH_TABLE_SIZE 4096
#define HASH_TABLE_BANKS 32

//#include <ap_cint.h>
// VEC==32
//typedef uint256 vec_t;
//typedef uint512 vec2_t;
// VEC==16
//typedef uint64 vec_t;
//typedef uint128 vec2_t;
// VEC==8
#include "ap_cint.h"
typedef uint128 vec_t;
typedef uint256 vec2_t;

void vec_t2chars(unsigned char dst[VEC], vec_t src);
void vec_t2ints(unsigned* dst, vec_t src);
vec_t chars2vec_t(unsigned char* src);
vec_t ints2vec_t(unsigned* src);
vec2_t short2vec2_t(unsigned short* src);

void huffman_translate(unsigned short l, unsigned short d, uint128 *out_buf, uint128 *outl_buf);
void match_window(unsigned char data_window[2*VEC], unsigned input_pos,
    unsigned in_len, unsigned short l[VEC], unsigned short d[VEC]);
void deflate259(uint512* input, unsigned* in_len_p0, uint512* tree, unsigned* tree_len_p0, uint512* output, unsigned* out_len);

//#define COUNT_FREQ
#endif
