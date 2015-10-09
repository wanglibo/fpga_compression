// Parallel deflate in Dataflow coding style.
//
// Created by Libo Wang on 10/1/15

#include <stdio.h>
#include <string.h>
#include <hls_stream.h>
#include <ap_utils.h>
#include <ap_int.h>

using namespace hls;

// TODO: remove debug print.
#include <iostream>
using namespace std;

#include "constant.h"

#define DIV_CEIL(x, base) ((x + base - 1 )/ base)

////////////////////////////////////////////////////////////////////////////
//
//  Helper functions to avoid pointer casting related error in HLS.
//
////////////////////////////////////////////////////////////////////////////

void uint512_to_vec_t(vec_t dst[64/VEC], uint512 src) {
  int j;
#pragma HLS inline
  for (j=0; j<64/VEC; j++) {
#pragma HLS UNROLL
    dst[j] = src((j+1)*VEC*8-1, j*VEC*8);
  }
}

void vec_t_to_chars(uint8 dst[VEC], vec_t src) {
  int j;
#pragma HLS inline
  for (j=0; j<VEC; j++) {
#pragma HLS UNROLL
    dst[j] = src((j+1)*8-1, j*8);
  }
}

vec_t chars_to_vec_t(uint8 src[VEC]) {
  int j;
  vec_t ret;
#pragma HLS inline
  for (j=0; j<VEC; j++) {
#pragma HLS UNROLL
    ret((j+1)*8-1, j*8) = src[j];
  }
  return ret;
}

vec_2t uint16_to_vec_2t(uint16 src[VEC]) {
  int j;
  vec_2t ret;
#pragma HLS inline
  for (j=0; j<VEC; j++) {
#pragma HLS UNROLL
    ret((j+1)*16-1, j*16) = src[j];
  }
  return ret;
}

void vec_2t_to_uint16(uint16 dst[VEC], vec_2t src) {
  int j;
#pragma HLS inline
  for (j=0; j<VEC; j++) {
#pragma HLS UNROLL
    dst[j] = src((j+1)*16-1, j*16);
  }
}

uint512 chars_to_uint512(uint8 src[64]) {
  int j;
  uint512 ret;
#pragma HLS inline
  for (j=0; j<64; j++) {
#pragma HLS UNROLL
    ret((j+1)*8-1, j*8) = src[j];
  }
  return ret;
}

void uint64_to_uint16(uint16 dst[4], uint64 src) {
  int j;
#pragma HLS inline
  for (j=0; j<4; j++) {
#pragma HLS UNROLL
    dst[j] = src((j+1)*16-1, j*16);
  }
}

uint64 uint16_to_uint64(uint16 src[4]) {
  int j;
  uint64 ret;
#pragma HLS inline
  for (j=0; j<4; j++) {
#pragma HLS UNROLL
    ret((j+1)*16-1, j*16) = src[j];
  }
  return ret;
}

void vec_8t_to_h(uint16 h[4*VEC], vec_8t src) {
#pragma HLS inline
  int j;
  for (j=0; j<4*VEC;j++) {
#pragma HLS UNROLL
    h[j] = src((j+1)*16-1, j*16);
  }
}

vec_8t h_to_vec_8t(uint16 src[VEC]) {
  int j;
  vec_8t ret;
#pragma HLS inline
  for (j=0; j<4*VEC; j++) {
#pragma HLS UNROLL
    ret((j+1)*16-1, j*16) = src[j];
  }
  return ret;
}

vec_2t chars_to_vec_2t(uint8 src[VEC*2]) {
  int j;
  vec_2t ret;
#pragma HLS inline
  for (j=0; j<2*VEC; j++) {
#pragma HLS UNROLL
    ret((j+1)*8-1, j*8) = src[j];
  }
  return ret;
}

// Min with min index
void min_reduction(uint16 input[VEC], uint16 *value, uint16 *index) {
  int i;
  uint16 red8[8]; uint16 red8i[8];
  uint16 red4[4]; uint16 red4i[4];
  uint16 red2[2]; uint16 red2i[2];
#pragma HLS ARRAY_PARTITION variable=red8 complete
#pragma HLS ARRAY_PARTITION variable=red4 complete
#pragma HLS ARRAY_PARTITION variable=red2 complete
#pragma HLS ARRAY_PARTITION variable=red8i complete
#pragma HLS ARRAY_PARTITION variable=red4i complete
#pragma HLS ARRAY_PARTITION variable=red2i complete
  // Here we break the abstraction of VEC.
  for (i=0; i<8; i++) {
    red8[i] = input[i]; red8i[i] = i;
  }
  for (i=0; i<4; i++) {
    uint16 l = red8[2*i], r = red8[2*i+1];
    uint16 li = red8i[2*i], ri = red8i[2*i+1];
    if (l <= r) {
      red4[i] = l; red4i[i] = li;
    } else {
      red4[i] = r; red4i[i] = ri;
    }
  }
  for (i=0; i<2; i++) {
    uint16 l = red4[2*i], r = red4[2*i+1];
    uint16 li = red4i[2*i], ri = red4i[2*i+1];
    if (l <= r) {
      red2[i] = l; red2i[i] = li;
    } else {
      red2[i] = r; red2i[i] = ri;
    }
  }
  uint16 value_out, index_out;
  if (red2[0] <= red2[1]) {
    value_out = red2[0]; index_out = red2i[0];
  } else {
    value_out = red2[1]; index_out = red2i[1];
  }
  *value = value_out; *index = index_out;
}

// Max with min index
void max_reduction(uint16 input[VEC], uint16 *value, uint16 *index) {
  int i;
  uint16 red8[8]; uint16 red8i[8];
  uint16 red4[4]; uint16 red4i[4];
  uint16 red2[2]; uint16 red2i[2];
#pragma HLS ARRAY_PARTITION variable=red8 complete
#pragma HLS ARRAY_PARTITION variable=red4 complete
#pragma HLS ARRAY_PARTITION variable=red2 complete
#pragma HLS ARRAY_PARTITION variable=red8i complete
#pragma HLS ARRAY_PARTITION variable=red4i complete
#pragma HLS ARRAY_PARTITION variable=red2i complete
  // Here we break the abstraction of VEC.
  for (i=0; i<8; i++) {
    red8[i] = input[i]; red8i[i] = i;
  }
  for (i=0; i<4; i++) {
    uint16 l = red8[2*i], r = red8[2*i+1];
    uint16 li = red8i[2*i], ri = red8i[2*i+1];
    if (l >= r) {
      red4[i] = l; red4i[i] = li;
    } else {
      red4[i] = r; red4i[i] = ri;
    }
  }
  for (i=0; i<2; i++) {
    uint16 l = red4[2*i], r = red4[2*i+1];
    uint16 li = red4i[2*i], ri = red4i[2*i+1];
    if (l >= r) {
      red2[i] = l; red2i[i] = li;
    } else {
      red2[i] = r; red2i[i] = ri;
    }
  }
  uint16 value_out, index_out;
  if (red2[0] >= red2[1]) {
    value_out = red2[0]; index_out = red2i[0];
  } else {
    value_out = red2[1]; index_out = red2i[1];
  }
  *value = value_out; *index = index_out;
}

////////////////////////////////////////////////////////////////////////////
void feed(uint512 *in_buf, int in_size, stream<vec_t>& feed_out) {
  int i, j;
  int batch_count = DIV_CEIL(in_size, 64);
  int vec_batch_count = DIV_CEIL(in_size, VEC);
  vec_t split_buffer[64/VEC];
  for (i = 0; i < batch_count; i++) {
#pragma HLS PIPELINE
    uint512_to_vec_t(split_buffer, in_buf[i]);
    for (j = 0; j < 64/VEC; j++) {
      if (i * (64/VEC) + j < vec_batch_count) {
        feed_out.write(split_buffer[j]);
      }
    }
  }
}

// Compute hash value from 4 bytes of input.
uint32 compute_hash(uint8 input[VEC]) {
#pragma HLS ARRAY_PARTITION variable=input complete
  return (((uint32)input[0])<<5) ^
          (((uint32)input[1])<<4) ^
          (((uint32)input[2])<<3) ^
          (((uint32)input[3])<<2) ^
          (((uint32)input[4])<<1) ^
          ((uint32)input[5]);
}

// Compare two strings and calculate the length of match
uint16 calc_match_len(uint8 input[VEC], uint8 record[VEC]) {
  uint16 match_id[VEC];
#pragma HLS inline
#pragma HLS ARRAY_PARTITION variable=match_id complete
  uint16 i;
  for (i=0; i<VEC; i++) {
    if (input[i] == record[i]) match_id[i] = VEC;
    else match_id[i] = i;
  }
  uint16 min_v, min_i;
  min_reduction(match_id, &min_v, &min_i);
  if (min_v == VEC) {
    min_i = VEC;
  }
  return min_i;
}

void hash_match(stream<vec_t> &data_window, int in_size,
    stream<vec_t> &literals,
    stream<vec_2t> &len_raw, stream<vec_2t> &dist_raw) {

  vec_t hash_content[BANK_OFFSETS][HASH_TABLE_BANKS];
  uint32 hash_position[BANK_OFFSETS][HASH_TABLE_BANKS];
  uint8 hash_valid[BANK_OFFSETS][HASH_TABLE_BANKS];

  vec_t prev_hash_content[HASH_TABLE_BANKS];
  uint32 prev_hash_position[HASH_TABLE_BANKS];
  uint8 prev_hash_valid[HASH_TABLE_BANKS];
  uint32 prev_offset[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=hash_valid complete dim=2
#pragma HLS ARRAY_PARTITION variable=hash_position complete dim=2
#pragma HLS ARRAY_PARTITION variable=hash_content complete dim=2
#pragma HLS ARRAY_PARTITION variable=prev_hash_valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_hash_position complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_hash_content complete dim=1
#pragma HLS ARRAY_PARTITION variable=prev_offset complete dim=1
  int i, j, k;
  int batch_count = DIV_CEIL(in_size, VEC);
  vec_t data0 = 0, data1 = 0;

  // Reset hash table
  for (i=0; i<BANK_OFFSETS; i++) {
#pragma HLS PIPELINE
    for (j=0; j<HASH_TABLE_BANKS; j++) {
      hash_valid[i][j] = 0;
    }
  }
  for (i = 0; i < HASH_TABLE_BANKS; i++) {
#pragma HLS UNROLL
    prev_hash_valid[i] = 0;
  }

  for (i = 0; i < batch_count + 1; i++) {
#pragma HLS PIPELINE
    uint16 hash_v[VEC];
#pragma HLS ARRAY_PARTITION variable=hash_v complete
    uint16 bank_num[VEC];
#pragma HLS ARRAY_PARTITION variable=bank_num complete
    uint16 bank_num1[VEC];
#pragma HLS ARRAY_PARTITION variable=bank_num1 complete
    uint16 bank_occupied[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=bank_occupied complete
    uint16 bank_offset[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=bank_offset complete
    vec_t updates[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=updates complete
    vec_t match_results[VEC];
#pragma HLS ARRAY_PARTITION variable=match_results complete
    uint32 match_positions[VEC];
#pragma HLS ARRAY_PARTITION variable=match_positions complete
    uint8 match_valid[VEC];
#pragma HLS ARRAY_PARTITION variable=match_valid complete
    uint8 current[VEC][VEC];
#pragma HLS ARRAY_PARTITION variable=current complete
    vec_t match_candidates[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=match_candidates complete
    uint32 match_positions_c[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=match_positions_c complete
    uint8 match_valid_c[HASH_TABLE_BANKS];
#pragma HLS ARRAY_PARTITION variable=match_valid_c complete
#pragma HLS dependence variable=hash_content inter RAW false
#pragma HLS dependence variable=hash_position inter RAW false
#pragma HLS dependence variable=hash_valid inter RAW false

    if (i < batch_count) {
      if (data_window.empty()) {
        i--;
        continue;
      }
      data0 = data1;
      data_window.read(data1);
    } else {
      data0 = data1;
      data1 = 0;
    }
    vec_t literals_out = data0;

    // Perform hash lookup and calculate match length
    if (i != 0) {
      uint32 input_pos = (i-1) * VEC;
//      cout << "input pos: " << input_pos << endl;
      uint8 data_window0[VEC];
      uint8 data_window1[VEC];

      vec_t_to_chars(data_window0, data0);
      vec_t_to_chars(data_window1, data1);
/*
      cout << hex;
      for (j=0; j<VEC; j++) {
        cout << data_window0[j] << "; ";
      }
      cout << endl;
      for (j=0; j<VEC; j++) {
        cout << data_window1[j] << "; ";
      }
      cout << endl << dec;

*/
      for (j=0; j<HASH_TABLE_BANKS; j++) {
        bank_offset[j] = BANK_OFFSETS;
        bank_occupied[j] = VEC;
      }
      // Compute hash values for all entries
//      cout  << "BN:";
      for (k=0; k<VEC; k++) {
        for (j=0; j<VEC; j++) {
          if (k + j >= VEC) {
            current[k][j] = data_window1[k+j-VEC];
          } else {
            current[k][j] = data_window0[k+j];
          }
        }
        hash_v[k] = compute_hash(&current[k][0]);
        bank_num[k] = hash_v[k] % HASH_TABLE_BANKS;
        bank_num1[k] = bank_num[k];
//        cout  << bank_num[k] <<  "; ";
      }
//      cout << endl;

      // Analyze bank conflict profile and avoid conflicted access
      for (k=0; k<VEC; k++) {
        for (j=0; j<VEC; j++) {
          if (j > k && bank_num[j] == bank_num[k]) {
            bank_num[j] = HASH_TABLE_BANKS; // set to invalid bank number
          }
        }
      }

      // For each bank record its reader/writer.
      for (j=0; j<HASH_TABLE_BANKS; j++) {
        for (k=0; k<VEC; k++) {
          if (bank_num[k] == j) {
            bank_offset[j] = hash_v[k] / HASH_TABLE_BANKS % BANK_OFFSETS;
            bank_occupied[j] = k;
          }
        }
      }
/*
      cout << "Bank occupied: ";
      for (j = 0; j < HASH_TABLE_BANKS; j++) {
        cout << bank_occupied[j] << "; ";
      }
      cout << endl;

      cout << "Bank offset:   ";
      for (j = 0; j < HASH_TABLE_BANKS; j++) {
        cout << bank_offset[j] << "; ";
      }
      cout << endl;
*/
      // Prepare update line for the hash table
      for (k=0; k<HASH_TABLE_BANKS; k++) {
        // bank k occupied by string from position pos
        uint8 pos = bank_occupied[k];
        uint8 real_pos;
        uint8 update[VEC];
#pragma HLS ARRAY_PARTITION variable=update complete
        real_pos = pos;
        if (pos == VEC) pos = 0;
        for (j=0; j<VEC; j++) {
          update[j] = current[pos][j];
        }
        if (real_pos != VEC) {
          updates[k] = chars_to_vec_t(update);
        } else {
          updates[k] = 0;
        }
      }

      // Perform conflict free memory access from all the banks
//      cout << "match_valid_c: ";
      for (j=0; j<HASH_TABLE_BANKS; j++) {
        uint8 pos = bank_occupied[j];
        if (pos != VEC) {
          uint32 loffset = bank_offset[j];

          if (loffset != prev_offset[j]) {
            match_candidates[j] = hash_content[bank_offset[j]][j];
            match_positions_c[j] = hash_position[bank_offset[j]][j];
            match_valid_c[j] = hash_valid[bank_offset[j]][j];
          } else {
            match_candidates[j] = prev_hash_content[j];
            match_positions_c[j] = prev_hash_position[j];
            match_valid_c[j] = prev_hash_valid[j];
          }
        } else {
          match_valid_c[j] = 0;
        }
//        cout << match_valid_c[j] << "; ";
      }
//      cout << endl;

      // Perform hash table update
      for (j=0; j<HASH_TABLE_BANKS; j++) {
        uint8 pos = bank_occupied[j];
        if (pos != VEC) {
          hash_content[bank_offset[j]][j] = updates[j];
          hash_position[bank_offset[j]][j] = input_pos + (uint32)pos;
          hash_valid[bank_offset[j]][j] = 1;

          prev_offset[j] = bank_offset[j];
          prev_hash_content[j] = updates[j];
          prev_hash_position[j] = input_pos + (uint32)pos;
          prev_hash_valid[j] = 1;
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
/*
      cout << "match_positions: ";
      for (j=0; j<VEC; j++) {
          if (match_valid[j] == 0) {
            match_positions[j] = 99999;
          }
          cout << match_positions[j] << " | ";
      }
      cout << endl;
*/
      // For each substring in the window do parallel matching and 
      // LZ77 encoding
      uint16 l[VEC];
      uint16 d[VEC];
      for (k=0; k<VEC; k++) {
        uint8 mismatch;
        uint16 ltemp1 = 0;
        uint8 history[VEC];

#pragma HLS ARRAY_PARTITION variable=history complete
        vec_t_to_chars(history, match_results[k]);

        ltemp1 = calc_match_len(&current[k][0], history);
        uint32 dist = input_pos + k - match_positions[k];
        if (input_pos+k+VEC-1 < in_size && match_valid[k] && ltemp1 >= 3
            && dist <= MAX_MATCH_DIST) {
          l[k] = ltemp1 - 3;
          d[k] = dist;
        } else {
          l[k] = current[k][0];
          d[k] = 0;
        }
//        cout << "i: " << input_pos+k << " (" << l[k] << ", " << d[k] 
//             << ") ~ " << ltemp1 << endl;
      }
      literals.write(literals_out);
      len_raw.write(uint16_to_vec_2t(l));
      dist_raw.write(uint16_to_vec_2t(d));
    }
  }
}

void match_selection(stream<vec_t> &literals, stream<vec_2t> &len_raw,
     stream<vec_2t> &dist_raw, int in_size, stream<vec_2t> &len,
     stream<vec_2t> &dist, stream<vec_t> &valid) {
  int i, j, k;
  int vec_batch_count = DIV_CEIL(in_size, VEC);

  // Loop carried match head position
  uint16 head_match_pos = 0;

  for (i = 0; i < vec_batch_count; i++) {
#pragma HLS PIPELINE
    uint16 reach[VEC];
    uint16 larray[VEC];
    uint16 darray[VEC];
    uint8 ldvalid[VEC];
    uint8 literals_vec[VEC];
    vec_2t len_raw_read, dist_raw_read, len_write, dist_write;
    vec_t literals_read;

    if (len_raw.empty() || dist_raw.empty() || literals.empty()) {
      i--;
      continue;
    }

    len_raw.read(len_raw_read);
    dist_raw.read(dist_raw_read);
    literals.read(literals_read);
    vec_2t_to_uint16(larray, len_raw_read);
    vec_2t_to_uint16(darray, dist_raw_read);
    vec_t_to_chars(literals_vec, literals_read);

    // First we compute how far each match / literal can reach
    for (j=0; j<VEC; j++) {
      if (darray[j] != 0) {
        // cannot exceed input_pos + 2 * VEC - 1: within next window.
        reach[j] = j + larray[j] + 3;
      } else {
        reach[j] = j + 1;
      }
    }

    // reach_1[k][j] is the reach[j] given the previous match covers
    // until index k. We essentialy speculate the max reach for
    // all cases.
    uint16 reach_1[VEC][VEC];
#pragma HLS ARRAY_PARTITION variable=reach_1 complete
    for (k=0; k<VEC; k++) {
      for (j=0; j<VEC; j++) {
        if (j>=k) {
          reach_1[k][j] = reach[j];
        } else {
          reach_1[k][j] = 0;
        }
      }
    }

    uint16 max_reaches[VEC];
    uint16 max_reach_indices[VEC];
#pragma HLS ARRAY_PARTITION variable=max_reaches complete
#pragma HLS ARRAY_PARTITION variable=max_reach_indices complete

    // Update first valid position in the next batch. max_reach is guaranteed
    // to be no less than VEC. Trim the best match to avoid head overlap.
    for (k=0; k<VEC; k++) {
      max_reduction(&reach_1[k][0], &max_reaches[k],
          &max_reach_indices[k]);
    }

    uint16 old_head_match_pos = head_match_pos;
    uint16 max_reach = max_reaches[old_head_match_pos];
    uint16 max_reach_index = max_reach_indices[old_head_match_pos];

    head_match_pos = max_reach - VEC;


    // Perform pipelined match selection: set valid bit for all matches.
    // Pass 1: Eliminate any matches that (1) extend beyond last match starting
    // position; (2) already handled by last cycle; (3) already handles by
    // the last match. Can be performed in parallel.
    for (k=0; k<VEC; k++) {
      if (k < old_head_match_pos || k > max_reach_index) {
        ldvalid[k] = 0;
      } else if (k == max_reach_index) {
        ldvalid[k] = 1; // Last match / literal must be valid
      } else {
        // Trim a match if it overlaps with the final match
        if (darray[k] != 0 && reach[k] > max_reach_index) {
          uint16 trimmed_len = larray[k] + 3 + max_reach_index - reach[k];
          if (trimmed_len < 3) {
            larray[k] = literals_vec[k];
            darray[k] = 0;
          } else {
            larray[k] = trimmed_len - 3;
          }
        }
        ldvalid[k] = 1;
      }
    }

    uint16 processed_len = old_head_match_pos;
    // Pass 2: For all the remaining matches, filter with lazy evaluztion.
    for (k=0; k<VEC-1; k++) {
      // Make sure we don't touch the tail match 
      if (ldvalid[k] && k != max_reach_index) {
        if (k < processed_len) {
          ldvalid[k] = 0;
        } else if (darray[k] == 0) { // literal should be written out
          processed_len++;
        } else { // current position is a match candidate
          // When the next match is better: commit literal here instead of match
          if (ldvalid[k+1] && darray[k+1] > 0 && larray[k+1] > larray[k]) {
            larray[k] = literals_vec[k];
            darray[k] = 0;
            processed_len++;
          } else {
            processed_len += larray[k] + 3;
          }
        }
      }
    }
    len.write(uint16_to_vec_2t(larray));
    dist.write(uint16_to_vec_2t(darray));
    valid.write(chars_to_vec_t(ldvalid));
  }
}

void mock_lz77(stream<vec_t> &literals, stream<vec_2t> &len,
    stream<vec_2t> &dist, stream<vec_t> &valid, int in_size) {
  int i, j;
  uint8 char_in[VEC];
  uint16 short_out[VEC];
  uint8 valid_out[VEC];
  int vec_batch_count = DIV_CEIL(in_size, VEC);
  for (i = 0; i < vec_batch_count; i++) {
#pragma HLS PIPELINE
    vec_t literals_read;
    if (literals.empty()) {
      i--;
      continue;
    }
    literals.read(literals_read);
    vec_t_to_chars(char_in, literals_read);
    for (j = 0; j < VEC; j++) {
      short_out[j] = char_in[j];
      valid_out[j] = 1;
    }
    len.write(uint16_to_vec_2t(short_out));
    dist.write(0);
    valid.write(chars_to_vec_t(valid_out));
  }
}

void parallel_huffman_encode(stream<vec_2t> &len, stream<vec_2t> &dist,
    stream<vec_t> &valid, int in_size, stream<uint16> &total_len,
    stream<vec_8t> &hcode8, stream<vec_8t> &hlen8) {
  int i, j;

  //cout << "Started huffman. " << endl;
  // Injects end code 256.
  int vec_batch_count = DIV_CEIL(in_size+1, VEC);

  int counter = 0;

  for (i = 0; i < vec_batch_count; i++) {
#pragma HLS pipeline
    uint64 hcode[VEC];
    uint64 hlen[VEC];
    uint16 len_vec[VEC];
    uint16 dist_vec[VEC];
    uint16 total_len_vec[VEC];
    uint8 valid_vec[VEC];
    vec_2t len_current, dist_current;
    vec_t valid_current;

    // There will be one more output line if the original size is
    //   a multiple of VEC. Simple add an empty line.
    if (in_size % VEC == 0 && i == vec_batch_count - 1) {
      len_current = 0;
      dist_current = 0;
      valid_current = 0;
    } else {
      if (len.empty() || dist.empty() || valid.empty()) {
        i--;
        continue;
      }
      len.read(len_current);
      dist.read(dist_current);
      valid.read(valid_current);
    }
    vec_2t_to_uint16(len_vec, len_current);
    vec_2t_to_uint16(dist_vec, dist_current);
    vec_t_to_chars(valid_vec, valid_current);

    for (j = 0; j < VEC; j++) {
      if (i * VEC + j == in_size) {
        len_vec[j] = 256;
        dist_vec[j] = 0;
        valid_vec[j] = 1;
      } else if (i * VEC + j > in_size) {
        len_vec[j] = 0;
        dist_vec[j] = 0;
        valid_vec[j] = 0;
      }
      huffman_translate(len_vec[j], dist_vec[j],
          &total_len_vec[j], &hcode[j], &hlen[j]);
      if (valid_vec[j] == 0) {
        hcode[j] = 0;
        hlen[j] = 0;
        total_len_vec[j] = 0;
      }
/*
      cout << "i: " << i*VEC+j
           << " (" << len_vec[j] << ", " << dist_vec[j] << ") ~"
           << hex << hcode[j] << " " << hlen[j] << dec << endl;
*/
    }

    uint16 total_len_out = 0;
    for (j = 0; j < VEC; j++) {
      total_len_out += total_len_vec[j];
    }

    counter += total_len_out;
//    cout << "Total len: " << counter << "; ("
//         << DIV_CEIL(counter, 8) << ")" << endl;

    vec_8t hcode8_out, hlen8_out;
    for (j = 0; j < VEC; j++) {
      hcode8_out((j+1)*64-1, j*64) = hcode[j];
      hlen8_out((j+1)*64-1, j*64) = hlen[j];
    }
    hcode8.write(hcode8_out); hlen8.write(hlen8_out);
    total_len.write(total_len_out);
  }
}

// Perform bit packing on position i of local pack array.
void accumulate_pos(uint32 s_pos[4*VEC], int i, int out_iter,
    uint32 hcode[VEC*4], uint16 *pack_p, uint16 *pack_next_p) {
#pragma HLS inline
  int j;
  uint16 pack, pack_next;
  pack = 0; pack_next = 0;
  uint16 zero16 = 0;
  uint32 zero32 = 0;
  for (j=0; j<4*VEC; j++) {
    bool upper = ((s_pos[j]+1) % (VEC) == i);
    // should write lower part here
    bool lower = (s_pos[j] % (VEC) == i);

    bool use_next = (s_pos[j] / (VEC) > out_iter) ||
        (upper && ((s_pos[j] + 1) / (VEC)) > out_iter);
    uint32 val32 = upper ? (hcode[j] >> 16) : (lower ? hcode[j] : zero32);
    uint16 val = val32(15, 0);
    pack |= use_next ? zero16 : val;
    pack_next |= use_next ? val : zero16;
/*
    if (dump==0) {
        // huffman encoding dump
        if (val) {
          if (upper) {
            fprintf(stderr, "Position %d has upper of data %d -- %08x\n",
                i, j, hcode[j]);
          }
          if (lower) {
            fprintf(stderr, "Position %d has lower of data %d -- %08x\n",
                i, j, hcode[j]);
          } 
        }
        if (use_next && val) {
          fprintf(stderr, "boundary: position %d, code %d\n", i, j);
        }
    }
*/
  }
  *pack_p = pack;
  *pack_next_p = pack_next;
}

void huffman_local_pack(vec_8t hcode_in, vec_8t hlen_in, int old_bit_pos, int out_count,
    vec_2t *work_buf_out, vec_2t *work_buf_next_out) {
#pragma HLS INLINE
    // Starting bit position of each code.
    uint32 pos[4*VEC];
    // Starting short position of each code.
    uint32 s_pos[4*VEC];

    uint32 hcode[4*VEC];
    uint32 hlen[4*VEC];
    uint16 zero16 = 0;
    
    int j;
    vec_2t work_buf = 0, work_buf_next = 0;

    // Compressed and packed data -- ready to send to output. Use double buffering
    // to reduce stall.
    // We know lcode, lextra, dcode, dextra's huffman codes are no more than 16 
    // bits, therefore it's impossible in one iteration to use up both buffers.
    uint16 local_pack[VEC];
#pragma HLS ARRAY_PARTITION variable=local_pack complete
    uint16 local_pack_next[VEC];
#pragma HLS ARRAY_PARTITION variable=local_pack_next complete
    for (j = 0; j < 4*VEC; j++) {
      hcode[j] = (zero16, hcode_in((j+1)*16-1, j*16));
      hlen[j] = (zero16, hlen_in((j+1)*16-1, j*16));
    }

    // Huffman packing.
    // Perform pre-shift just like Altera GZIP example algorithm, but put code
    // into a function to avoid unrolling of large loops.

    // Phase 1. Calculate starting bit pos of each of the 4*VEC codes.
    // Accumulation of bit offsets; reduction possible.
    pos[0] = 0;
    for (j = 1; j < 4*VEC; j++) {
      pos[j] = pos[j-1] + hlen[j-1];
    }

    vec_2t pack_temp = 0;
    vec_4t pack_shifted;
    for (j=0; j<VEC*4; j++) {
      vec_2t temp = hcode[j];
      pack_temp |= temp << pos[j];
    }
    pack_shifted = pack_temp;
    pack_shifted = pack_shifted << (old_bit_pos % (VEC*16));
    work_buf = pack_shifted(VEC*16-1, 0);
    work_buf_next = pack_shifted(VEC*32-1, VEC*16);

/*
    for (j=0; j<4*VEC; j++) {
      pos[j] += old_bit_pos;
      s_pos[j] = pos[j] / 16;
    }

    // Phase 2. Shift each code by an appropriate amount and align to 16-bit
    // boundary.
    for (j=0; j<4*VEC; j++) {
      hcode[j] = hcode[j] << (pos[j] % 16);
    }
    for (j=0; j<VEC; j++) {
      local_pack[j] = 0;
      local_pack_next[j] = 0;
    }

    // Phase 3. For each position in the current and next packing buffer,
    // OR with appropriate data.
    for (j=0; j<VEC; j++) {
      accumulate_pos(s_pos, j, out_count, hcode,
          &local_pack[j],
          &local_pack_next[j]);
    }

    // Now pack current buffers into output double buffer
    *work_buf_out = uint16_to_vec_2t(local_pack);
    *work_buf_next_out = uint16_to_vec_2t(local_pack_next);
*/
    *work_buf_out = work_buf;
    *work_buf_next_out = work_buf_next;
}

void write_huffman_output(stream<uint16> &total_len,
    stream<vec_8t> &hcode8, stream<vec_8t> &hlen8,
    int in_size, stream<vec_2t> &data, stream<int> &size) {
  // Input huffman tree size in bytes

  int tree_size_bits_fixed = 640;
/*
  vec_2t tree_b[512/VEC/2]; // 32
  tree_b[0] = vec_2t("F689489CCC28C9C39E75CB24A0059DED", 16);
  tree_b[1] = vec_2t("E7AC4E57B6BD792B9EEE9869EE985CD2", 16);
  tree_b[2] = vec_2t("999871276CB2DB12894B628D9C3B5A28", 16);
  tree_b[3] = vec_2t("D55775554FFBE1999999999999999999", 16);
  tree_b[4] = vec_2t("53AA61EFFBBD2B43BBB3270E7DCF4CF4", 16);
*/
  uint512 tree_buf[8] = {
uint512("d55775554ffbe1999999999999999999999871276cb2db12894b628d9c3b5a28e7ac4e57b6bd792b9eee9869ee985cd2f689489ccc28c9c39e75cb24a0059ded", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000853aa61effbbd2b43bbb3270e7dcf4cf4", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16),
uint512("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 16)
};

  int i, j, out_count;
  // The number of 64 byte batches we can write to output directly.
  int tree_batch_count = DIV_CEIL(tree_size_bits_fixed, 512);
  // The number of input batches
  int in_batch_count = DIV_CEIL(in_size+1, VEC);

  int bit_pos = 0;
  vec_2t partial_out;

  // Record the number of 2*VEC byte buffers wrote to out_buf.
  out_count = 0;
  int tree_output_lines = tree_size_bits_fixed / (VEC * 16);

  // Read tree into a local buffer
  // A huffman tree cannot exceed 64 * 4 bytes.

  vec_2t tree_b[4*64/(2*VEC)];
#pragma HLS array partition variable=tree_b cyclic factor=4
  for (i=0; i<tree_batch_count; i++)
  {
     int j;
#pragma HLS pipeline
     uint512 tmp512 = tree_buf[i];
     for (j=0; j<64/(2*VEC); j++) {
       tree_b[(64/(2*VEC)*i+j)] =
           tmp512((j+1)*16*VEC-1, j*16*VEC);
     }
  }
/*
  for (i=0; i<512/(2*VEC); i++) {
    uint8 tree_line[2*VEC];
    for (j=0; j<2*VEC; j++) {
      tree_line[j] = tree_b8[i*2*VEC + j];
    }
    tree_b[i] = chars_to_vec_2t(tree_line);
  }
*/

  // Write as much as possible of the huffman tree to the output.
  for (i=0; i<tree_output_lines; i++) {
#pragma HLS pipeline
    cout << "tree_b[i]: " << hex << tree_b[i] << dec << endl;
    data.write(tree_b[i]);
    size.write(-1);
  }

  if (tree_size_bits_fixed > tree_output_lines * VEC * 16) {
    partial_out = tree_b[tree_output_lines];
  } else {
    partial_out = 0;
  }
  out_count = tree_output_lines;
  bit_pos = tree_size_bits_fixed;

  for (i = 0; i < in_batch_count; i++) {
#pragma HLS PIPELINE II=1
    vec_8t hcode8_out, hlen8_out;
    vec_2t work_buf, work_buf_next;

    if (total_len.empty() ||
        hcode8.empty() || hlen8.empty()) {
      i--;
      continue;
    }

    // Load data from channels
    hcode8.read(hcode8_out); hlen8.read(hlen8_out);
    uint16 total_len_current;
    total_len.read(total_len_current);

    huffman_local_pack(hcode8_out, hlen8_out, bit_pos, out_count,
        &work_buf, &work_buf_next);

    // bit pos is the register to carry through the loop so we update immediately
    int new_bit_pos = (int)total_len_current + bit_pos;
    bool reached_next = (new_bit_pos / (VEC*16) > out_count);
    bit_pos = new_bit_pos;

    if (reached_next) {
      out_count++;
    }

    if (reached_next) {
      data.write(work_buf | partial_out);
      size.write(-1);
      partial_out = work_buf_next;
    } else {
      partial_out |= work_buf;
    }
  }

  // Flush partial_out
  if (bit_pos % (VEC*16) != 0) {
    data.write(partial_out);
    size.write(-1);
  }

  data.write(0);
  size.write(DIV_CEIL(bit_pos, 8));
}

void export_data(stream<vec_2t> &data, stream<int> &size, uint512 *out_buf,
    int *out_size) {
  uint512 tmp_out = 0;
  // The number of vec_2t we received.
  int count = 0;
  int out_count = 0;
  while (1) {
#pragma HLS pipeline
    vec_2t data_read;
    int size_read;
    if (data.empty() || size.empty()) {
      continue;
    }
    data.read(data_read);
    size.read(size_read);
    if (size_read == -1) {
      int slot = count % (32/VEC);
      tmp_out((slot+1)*VEC*16-1, slot*VEC*16) = data_read;
      count++;
      if (slot == (32/VEC-1)) {
        out_buf[out_count++] = tmp_out;
        tmp_out = 0;
      }
    } else {
      // Flush uint512 buffer.
      if (count % (32/VEC) != 0) {
        out_buf[out_count] = tmp_out;
      }
      *out_size = size_read;
      break;
    }
  }
}

extern "C" {
void deflate259(uint512 *in_buf, int in_size,
                uint512 *out_buf, int *out_size) {
#pragma HLS INTERFACE m_axi port=in_buf offset=slave bundle=gmem1 depth=1024
#pragma HLS INTERFACE m_axi port=out_buf offset=slave bundle=gmem2 depth=2048
#pragma HLS INTERFACE m_axi port=out_size offset=slave bundle=gmem3 depth=1
//#pragma HLS INTERFACE s_axilite port=in_size bundle=control
//#pragma HLS INTERFACE s_axilite port=tree_size bundle=control
//#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=in_buf bundle=control
#pragma HLS INTERFACE s_axilite port=in_size bundle=control
#pragma HLS INTERFACE s_axilite port=out_buf bundle=control
#pragma HLS INTERFACE s_axilite port=out_size bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  stream<vec_t> literals("literals");
#pragma HLS STREAM variable=literals depth=1
  stream<vec_t> literals_2("literals_2");
#pragma HLS STREAM variable=literals_2 depth=1

  stream<vec_2t> len_raw("len_raw");
#pragma HLS STREAM variable=len_raw depth=1
  stream<vec_2t> dist_raw("dist_raw");
#pragma HLS STREAM variable=dist_raw depth=1
  stream<vec_2t> len("len");
#pragma HLS STREAM variable=len depth=1
  stream<vec_2t> dist("dist");
#pragma HLS STREAM variable=dist depth=1
  stream<vec_t> valid("valid");
#pragma HLS STREAM variable=valid depth=1

  stream<vec_2t> hcode0("hcode0");
#pragma HLS STREAM variable=hcode0 depth=1
  stream<vec_2t> hcode1("hcode1");
#pragma HLS STREAM variable=hcode1 depth=1
  stream<vec_2t> hcode2("hcode2");
#pragma HLS STREAM variable=hcode2 depth=1
  stream<vec_2t> hcode3("hcode3");
#pragma HLS STREAM variable=hcode3 depth=1

  stream<vec_2t> hlen0("hlen0");
#pragma HLS STREAM variable=hlen0 depth=1
  stream<vec_2t> hlen1("hlen1");
#pragma HLS STREAM variable=hlen1 depth=1
  stream<vec_2t> hlen2("hlen2");
#pragma HLS STREAM variable=hlen2 depth=1
  stream<vec_2t> hlen3("hlen3");
#pragma HLS STREAM variable=hlen3 depth=1

  stream<vec_8t> hcode8("hcode8");
#pragma HLS STREAM variable=hcode8 depth=1
  stream<vec_8t> hlen8("hlen8");
#pragma HLS STREAM variable=hlen8 depth=1

  stream<vec_2t> data("data");
#pragma HLS STREAM variable=data depth=1
  stream<int> size("size");
#pragma HLS STREAM variable=size depth=1
  stream<uint16> total_len("total_len");
#pragma HLS STREAM variable=total_len depth=1

#pragma HLS DATAFLOW
  int in_size_local = in_size;

  feed(in_buf, in_size_local, literals);
//  mock_lz77(literals, len, dist, valid, in_size_local);

  hash_match(literals, in_size_local, literals_2,
      len_raw, dist_raw);

  match_selection(literals_2, len_raw, dist_raw, in_size_local,
      len, dist, valid);


  parallel_huffman_encode(len, dist, valid, in_size_local, total_len,
      hcode8, hlen8);

  write_huffman_output(total_len,
      hcode8, hlen8, in_size_local, data, size);

  export_data(data, size, out_buf, out_size);
}
}
