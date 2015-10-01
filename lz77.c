/* lz77.c
 * Created by Libo Wang for CS259 project, 6/3/15
 *
 * In this file we create a set of VEC static hash tables for lookup.
 *
 * Hash tables contain the location and data for each entry.
 */

#include <stdio.h>
#include "constant.h"
#include <string.h>

#ifdef BANK_OFFSETS
static vec_t hash_content[BANK_OFFSETS][HASH_TABLE_BANKS];
static unsigned hash_position[BANK_OFFSETS][HASH_TABLE_BANKS];
static unsigned char hash_valid[BANK_OFFSETS][HASH_TABLE_BANKS];

static vec_t prev_hash_content[HASH_TABLE_BANKS];
static unsigned prev_hash_position[HASH_TABLE_BANKS];
static unsigned char prev_hash_valid[HASH_TABLE_BANKS];
static int prev_offset[HASH_TABLE_BANKS];
#else
static vec_t hash_content[HASH_TABLE_SIZE];
static unsigned hash_position[HASH_TABLE_SIZE];
static unsigned char hash_valid[HASH_TABLE_SIZE];
#endif

// Peichen
void vec_t2chars(unsigned char* dst, vec_t src)
{
    int j;

#pragma HLS inline
    for (j=0; j<VEC; j++) {
#pragma HLS unroll
        dst[j] = apint_get_range(src, (j+1)*8-1, j*8);
    }
}

vec_t chars2vec_t(unsigned char* src)
{
    int j; 
    vec_t dst;

#pragma HLS inline
    for (j=0; j<VEC; j++) {
#pragma HLS unroll
        dst = apint_set_range(dst, (j+1)*8-1, j*8, src[j]);
    }
    return dst;
}

void vec_t2ints(unsigned* dst, vec_t src)
{       
    int j;
        
#pragma HLS inline
    for (j=0; j<VEC/4; j++) {
#pragma HLS unroll
        dst[j] = apint_get_range(src, (j+1)*32-1, j*32);
    }   
}       

vec_t ints2vec_t(unsigned* src)
{
    int j;
    vec_t dst;

#pragma HLS inline
    for (j=0; j<VEC/4; j++) {
#pragma HLS unroll
        dst = apint_set_range(dst, (j+1)*32-1, j*32, src[j]);
    }
    return dst;
}

vec2_t short2vec2_t(unsigned short* src)
{
    int j; 
    vec2_t dst;

#pragma HLS inline
    for (j=0; j<VEC; j++) {
#pragma HLS unroll
        dst = apint_set_range(dst, (j+1)*16-1, j*16, src[j]);
    }
    return dst;
} 

uint128 uint32_to_uint128(unsigned src[4]) {
  int j;
  uint128 dst;
#pragma HLS inline
  for (j=0; j<4; j++) {
#pragma HLS unroll
    dst = apint_set_range(dst, (j+1)*32-1, j*32, src[j]);
  }
  return dst;
}

void uint128_to_uint32(unsigned dst[4], uint128 src) {
  int j;
#pragma HLS inline
  for (j=0; j<4; j++) {
#pragma HLS unroll
    dst[j] = apint_get_range(src, (j+1)*32-1, j*32);
  }
}

void min_reduction(unsigned short input[VEC], unsigned short *max, 
  unsigned short *max_i)
{
#pragma HLS INLINE off
  unsigned i;
  unsigned short maxv, maxi;
#if VEC==32
  unsigned short red16[16];
  unsigned short red16i[16];
#pragma HLS ARRAY_PARTITION variable=red16 complete
#pragma HLS ARRAY_PARTITION variable=red16i complete
#endif
  unsigned short red8[8];
  unsigned short red4[4];
  unsigned short red2[2];
  unsigned short red8i[8];
  unsigned short red4i[4];
  unsigned short red2i[2];
#pragma HLS ARRAY_PARTITION variable=red8 complete
#pragma HLS ARRAY_PARTITION variable=red4 complete
#pragma HLS ARRAY_PARTITION variable=red2 complete
#pragma HLS ARRAY_PARTITION variable=red8i complete
#pragma HLS ARRAY_PARTITION variable=red4i complete
#pragma HLS ARRAY_PARTITION variable=red2i complete
  maxv = 0;
  maxi = VEC;
/*
  for (i=0; i<VEC; i++) {
    if (i==0 || input[i] > maxv) {
      maxv = input[i];
      maxi = i;
    }
  }
*/
#if VEC>=16
#if VEC==32
  for (i=0; i<16; i++) {
    unsigned short l, r, li, ri;
    l = input[2*i]; r = input[2*i+1]; li = 2*i; ri = 2*i+1;
    if(r < l) {
      red16[i] = r; red16i[i] = ri;
    } else {
      red16[i] = l; red16i[i] = li;
    }
  }
#endif

  for (i=0; i<8; i++) {
    unsigned short l, r, li, ri;
#if VEC==32
    l = red16[2*i]; r = red16[2*i+1]; li = red16i[2*i]; ri = red16i[2*i+1];
#else
    l = input[2*i]; r = input[2*i+1]; li = 2*i; ri = 2*i+1;
#endif
    if(r < l) {
      red8[i] = r; red8i[i] = ri;
    } else {
      red8[i] = l; red8i[i] = li;
    }
  }
  for (i=0; i<4; i++) {
    unsigned short l, r, li, ri;
    l = red8[2*i]; r = red8[2*i+1]; li = red8i[2*i]; ri = red8i[2*i+1];
    if(r < l) {
      red4[i] = r; red4i[i] = ri;
    } else {
      red4[i] = l; red4i[i] = li;
    }
  }
  for (i=0; i<2; i++) {
    unsigned short l, r, li, ri;
    l = red4[2*i]; r = red4[2*i+1]; li = red4i[2*i]; ri = red4i[2*i+1];
    if(r < l) {
      red2[i] = r; red2i[i] = ri;
    } else {
      red2[i] = l; red2i[i] = li;
    }
  }
  if (red2[1] < red2[0]) {
    maxv = red2[1]; maxi = red2i[1];
  } else {
    maxv = red2[0]; maxi = red2i[0];
  }
#else
  for (i=0; i<VEC; i++) {
    if (i==0 || input[i] < maxv) {
      maxv = input[i];
      maxi = i;
    }
  }
#endif
  *max = maxv; *max_i = maxi;
}


// Compute hash value from 4 bytes of input.
unsigned compute_hash(unsigned char input[VEC])
{
#pragma HLS ARRAY_PARTITION variable=input complete
  return ((input[0]<<5) ^ (input[1]<<4) ^ (input[2]<<3) 
      ^ (input[3] <<2) ^ (input[4]<<1) ^ input[5]) & 0xfffU;
}

// Clear the hash table
void reset_history()
{
  int i,j;
#ifdef BANK_OFFSETS
  for (i=0; i<BANK_OFFSETS; i++) {
    for (j=0; j<HASH_TABLE_BANKS; j++) {
      hash_valid[i][j] = 0;
    }
  }
#else
  for (j=0; j<HASH_TABLE_SIZE; j++) {
    hash_valid[j] = 0;
  }
#endif
}

// Compare two strings and calculate the length of match
unsigned short calc_match_len(unsigned char input[VEC], unsigned char record[VEC]) 
{
/*
#pragma HLS ARRAY_PARTITION variable=input complete
#pragma HLS ARRAY_PARTITION variable=record complete
*/
#if VEC==8
  unsigned match_len, mismatch, i;
  match_len = 0;
  mismatch = 0;
  for (i=0; i<VEC; i++) {
    if (!mismatch && input[i] == record[i]) match_len++;
    else mismatch = 1;
  }
  return match_len;
#else
  unsigned short match_id[VEC];
#pragma HLS ARRAY_PARTITION variable=match_id complete
  unsigned char i;
  for (i=0; i<VEC; i++) {
    if (input[i] == record[i]) match_id[i] = VEC;
    else match_id[i] = i;
  }
  unsigned short maxv, maxi;
  min_reduction(match_id, &maxv, &maxi);
  if (maxv == VEC) {
    maxi = VEC;
  }
  return maxi;
#endif
}

// Return raw string match result based on dictionary lookup, and also
// update dictionary accordingly.
// We first read all VEC hash tables for match, then select a match with max
// distance.
// If there is match with length > 3, then l will take match length - 3.
// and d will take the corresponding match distance.
void match_window(unsigned char data_window[2*VEC], unsigned input_pos,
    unsigned in_len, unsigned short l[VEC], unsigned short d[VEC])
{
//#pragma HLS inline
//#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=data_window complete
#pragma HLS ARRAY_PARTITION variable=l complete
#pragma HLS ARRAY_PARTITION variable=d complete
#if 0
#pragma HLS ARRAY_PARTITION variable=hash_valid cyclic factor=32 dim=2
#pragma HLS ARRAY_PARTITION variable=hash_position cyclic factor=32 dim=2
#pragma HLS ARRAY_PARTITION variable=hash_content cyclic factor=32 dim=2
#else
#pragma HLS ARRAY_PARTITION variable=hash_valid complete dim=2
#pragma HLS ARRAY_PARTITION variable=hash_position complete dim=2
#pragma HLS ARRAY_PARTITION variable=hash_content complete dim=2
#pragma HLS ARRAY_PARTITION variable=prev_hash_valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_hash_position complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_hash_content complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_offset complete dim=1
#endif
  unsigned short hash_v[VEC];
#pragma HLS ARRAY_PARTITION variable=hash_v complete
  unsigned short bank_num[VEC];
#pragma HLS ARRAY_PARTITION variable=bank_num complete
  unsigned short bank_num1[VEC];
#pragma HLS ARRAY_PARTITION variable=bank_num1 complete
  unsigned short bank_occupied[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=bank_occupied complete
  unsigned short bank_offset[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=bank_offset complete

  vec_t updates[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=updates complete

  vec_t match_results[VEC];
  unsigned match_positions[VEC];
  unsigned char match_valid[VEC];
#pragma HLS ARRAY_PARTITION variable=match_results complete
#pragma HLS ARRAY_PARTITION variable=match_positions complete
#pragma HLS ARRAY_PARTITION variable=match_valid complete

  unsigned short index, i, j, ltemp, dtemp, match;
  unsigned char current[VEC][VEC];
#pragma HLS ARRAY_PARTITION variable=current complete

  vec_t match_candidates[HASH_TABLE_BANKS];
  unsigned match_positions_c[HASH_TABLE_BANKS];
  unsigned char match_valid_c[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=match_candidates complete
#pragma HLS ARRAY_PARTITION variable=match_positions_c complete
#pragma HLS ARRAY_PARTITION variable=match_valid_c complete

#pragma HLS dependence variable=hash_content inter RAW false
#pragma HLS dependence variable=hash_position inter RAW false
#pragma HLS dependence variable=hash_valid inter RAW false

  // Access offset for each bank.
  for (i=0; i<HASH_TABLE_BANKS; i++) {
    bank_offset[i] = HASH_TABLE_SIZE/HASH_TABLE_BANKS;
    bank_occupied[i] = VEC;
  }

  // Compute hash values for all entries
  for (i=0; i<VEC; i++) {
    for (j=0; j<VEC; j++) {
      current[i][j] = data_window[i+j];
    }
    hash_v[i] = compute_hash(&current[i][0]);
    bank_num[i] = hash_v[i] % HASH_TABLE_BANKS;
    bank_num1[i] = bank_num[i];
  }

  // Analyze bank conflict profile and avoid conflicted access
  for (i=0; i<VEC; i++) {
    for (j=0; j<VEC; j++) {
      if (j > i && bank_num[j] == bank_num[i]) {
        bank_num[j] = HASH_TABLE_BANKS; // set to invalid bank number
      }
    }
  }

  for (j=0; j<HASH_TABLE_BANKS; j++) {
    for (i=0; i<VEC; i++) {
      if (bank_num[i] == j) {
        bank_offset[j] = hash_v[i] / HASH_TABLE_BANKS;
        bank_occupied[j] = i;
      }
    }
  }

  // Prepare update line for the hash table
  for (i=0; i<HASH_TABLE_BANKS; i++) {
    unsigned char pos = bank_occupied[i];
    unsigned char real_pos;
    unsigned char update[VEC];
#pragma HLS ARRAY_PARTITION variable=update complete
    real_pos = pos;
    if (pos == VEC) pos = 0;
    for (j=0; j<VEC; j++) {
      update[j] = current[pos][j];
    }
    if (real_pos != VEC) {
//      updates[i] = *((vec_t*)update);
      updates[i] = chars2vec_t(update);
    } else {
      updates[i] = 0;
    }
  }

  // Perform conflict free memory access from all the banks
  for (j=0; j<HASH_TABLE_BANKS; j++) {
    unsigned char pos = bank_occupied[j];
    if (pos != VEC) {
#ifdef BANK_OFFSETS
      int loffset = bank_offset[j];

      if (loffset != prev_offset[j]) {
      match_candidates[j] = hash_content[bank_offset[j]][j];
      match_positions_c[j] = hash_position[bank_offset[j]][j];
      match_valid_c[j] = hash_valid[bank_offset[j]][j];
      } else {
//printf("a hit at %d\n", loffset);
      match_candidates[j] = prev_hash_content[j];
      match_positions_c[j] = prev_hash_position[j];
      match_valid_c[j] = prev_hash_valid[j];
      }
#else
      match_candidates[j] = hash_content[bank_offset[j]*HASH_TABLE_BANKS+j];
      match_positions_c[j] = hash_position[bank_offset[j]*HASH_TABLE_BANKS+j];
      match_valid_c[j] = hash_valid[bank_offset[j]*HASH_TABLE_BANKS+j];
#endif
    } else {
      match_valid_c[j] = 0;
    }
  }

  // Perform hash table update
  for (j=0; j<HASH_TABLE_BANKS; j++) {
    unsigned char pos = bank_occupied[j];
    if (pos != VEC) {
#ifdef BANK_OFFSETS
      hash_content[bank_offset[j]][j] = updates[j];
      hash_position[bank_offset[j]][j] = input_pos + pos;
      hash_valid[bank_offset[j]][j] = 1;

      prev_offset[j] = bank_offset[j];
      prev_hash_content[j] = updates[j];
      prev_hash_position[j] = input_pos + pos;
      prev_hash_valid[j] = 1;
#else
      hash_content[bank_offset[j]*HASH_TABLE_BANKS+j] = updates[j];
      hash_position[bank_offset[j]*HASH_TABLE_BANKS+j] = input_pos + pos;
      hash_valid[bank_offset[j]*HASH_TABLE_BANKS+j] = 1;
#endif
    }
  }

  // Gather match results from correct banks.
  // Now match_result[i][j] is the potential match for string current[j] in
  // dictionaty i.
  for (j=0; j<VEC; j++) { 
    match_results[j] = match_candidates[bank_num1[j]];
    match_positions[j] = match_positions_c[bank_num1[j]];
    match_valid[j] = match_valid_c[bank_num1[j]];
  }

  // For each substring in the window do parallel matching and 
  // LZ77 encoding
  for (i=0; i<VEC; i++) {
    unsigned char k, mismatch;
    unsigned short ltemp1 = 0;
    unsigned char history[VEC];

#pragma HLS ARRAY_PARTITION variable=history complete
//    *((vec_t*)history) = match_results[i];
    vec_t2chars(history, match_results[i]);

    ltemp1 = calc_match_len(&current[i][0], history);
    unsigned dist = input_pos + i - match_positions[i];
    if (input_pos+i+VEC-1 < in_len && match_valid[i] && ltemp1 >= 3
        && dist <= MAX_MATCH_DIST) {
      l[i] = ltemp1 - 3;
      d[i] = dist;
    } else {
      l[i] = current[i][0];
      d[i] = 0;
    }
  }
}



