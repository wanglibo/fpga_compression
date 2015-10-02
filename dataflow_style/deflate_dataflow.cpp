// Parallel deflate in Dataflow coding style.
//
// Created by Libo Wang on 10/1/15

#include <stdio.h>
#include <string.h>
#include <hls_stream.h>
#include <ap_utils.h>
#include <ap_int.h>

#include <iostream>
using namespace std;

using namespace hls;

// Design parameters and switchse
#define VEC 8

////////////////////////////////////////////////////////////////////////////
//
//  Macros and definitaions
//
////////////////////////////////////////////////////////////////////////////
#if VEC!=8 && VEC!=16 && VEC!=32
#error "VEC must be 8, 16 or 32!"
#endif

typedef ap_uint<VEC*8> vec_t;
typedef ap_uint<VEC*16> vec_2t;
typedef ap_uint<VEC*32> vec_4t;
typedef ap_uint<8> uint8;
typedef ap_uint<16> uint16;
typedef ap_uint<32> uint32;
typedef ap_uint<64> uint64;
typedef ap_uint<128> uint128;
typedef ap_uint<512> uint512;

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

void mock_lz77(stream<vec_t> &literals, stream<vec_2t> &len, stream<vec_2t> &dist,
    stream<vec_t> &valid, int in_size) {
  int i, j;
  uint8 char_in[VEC];
  uint16 short_out[VEC];
  uint8 valid_out[VEC];
  int vec_batch_count = DIV_CEIL(in_size+1, VEC);
  for (i = 0; i < vec_batch_count; i++) {
#pragma HLS PIPELINE
    vec_t literals_read;
    literals.read(literals_read);
    vec_t_to_chars(char_in, literals_read);
    for (j = 0; j < VEC; j++) {
      if (i * VEC + j < in_size) {
        short_out[j] = char_in[j];
        valid_out[j] = 1;
      } else if (i * VEC + j == in_size) {
        short_out[j] = 3;
        valid_out[j] = 1;
      } else {
        short_out[j] = 0;
        valid_out[j] = 0;
      }
    }
    len.write(uint16_to_vec_2t(short_out));
    dist.write(0);
    valid.write(chars_to_vec_t(valid_out));
  }
}

void huffman_translate(uint16 l, uint16 d, uint8 valid, uint64 *hcode, uint64 *hlen) {
  // TODO: add real huffman translation code.
  uint16 mock_hcode[4];
  uint16 mock_hlen[4];
  int i;
  for (i = 0; i < 4; i++) {
    mock_hcode[i] = 0;
    mock_hlen[i] = 0;
  }
  if (valid) {
    mock_hcode[0] = l;
    mock_hlen[0] = 8;
  }

  *hcode = uint16_to_uint64(mock_hcode);
  *hlen = uint16_to_uint64(mock_hlen);
}

void parallel_huffman_encode(stream<vec_2t> &len, stream<vec_2t> &dist,
    stream<vec_t> &valid, int in_size,
    stream<vec_2t> &hcode0, stream<vec_2t> &hlen0,
    stream<vec_2t> &hcode1, stream<vec_2t> &hlen1,
    stream<vec_2t> &hcode2, stream<vec_2t> &hlen2,
    stream<vec_2t> &hcode3, stream<vec_2t> &hlen3) {
  int i, j;
  int vec_batch_count = DIV_CEIL(in_size+1, VEC);
  for (i = 0; i < vec_batch_count; i++) {
#pragma HLS pipeline
    uint64 hcode[VEC];
    uint64 hlen[VEC];
    uint16 len_vec[VEC];
    uint16 dist_vec[VEC];
    uint8 valid_vec[VEC];
    vec_2t len_current, dist_current;
    vec_t valid_current;
    len.read(len_current);
    dist.read(dist_current);
    valid.read(valid_current);
    vec_2t_to_uint16(len_vec, len_current);
    vec_2t_to_uint16(dist_vec, dist_current);
    vec_t_to_chars(valid_vec, valid_current);
    for (j = 0; j < VEC; j++) {
      huffman_translate(len_vec[j], dist_vec[j], valid_vec[j], &hcode[j], &hlen[j]);
    }
    vec_2t hcode0_out, hlen0_out;
    vec_2t hcode1_out, hlen1_out;
    vec_2t hcode2_out, hlen2_out;
    vec_2t hcode3_out, hlen3_out;
    for (j = 0; j < VEC/4; j++) {
      hcode0_out((j+1)*64-1, j*64) = hcode[j];
      hcode1_out((j+1)*64-1, j*64) = hcode[VEC/4+j];
      hcode2_out((j+1)*64-1, j*64) = hcode[2*VEC/4+j];
      hcode3_out((j+1)*64-1, j*64) = hcode[3*VEC/4+j];
      hlen0_out((j+1)*64-1, j*64) = hlen[j];
      hlen1_out((j+1)*64-1, j*64) = hlen[j+VEC/4];
      hlen2_out((j+1)*64-1, j*64) = hlen[j+2*VEC/4];
      hlen3_out((j+1)*64-1, j*64) = hlen[j+3*VEC/4];
    }
    hcode0.write(hcode0_out); hlen0.write(hlen0_out);
    hcode1.write(hcode1_out); hlen1.write(hlen1_out);
    hcode2.write(hcode2_out); hlen2.write(hlen2_out);
    hcode3.write(hcode3_out); hlen3.write(hlen3_out);
  }
}

// Perform bit packing on position i of local pack array.
void accumulate_pos(uint32 s_pos[4*VEC], int i, int out_iter,
    uint32 hcode[VEC*4], uint16 *pack_p, uint16 *pack_next_p) {
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

void write_huffman_output(uint512 *tree_buf, int tree_size_bits,
    stream<vec_2t> &hcode0, stream<vec_2t> &hlen0,
    stream<vec_2t> &hcode1, stream<vec_2t> &hlen1,
    stream<vec_2t> &hcode2, stream<vec_2t> &hlen2,
    stream<vec_2t> &hcode3, stream<vec_2t> &hlen3,
    int in_size, stream<vec_2t> &data, stream<int> &size) {
  // Input huffman tree size in bytes
  int out_pos = 0;
  int i, j, out_count;
  // The number of 64 byte batches we can write to output directly.
  int tree_batch_count = DIV_CEIL(tree_size_bits, 512);
  // The number of input batches
  int in_batch_count = DIV_CEIL(in_size+1, VEC);

  int bit_pos = 0;
  vec_2t work_buf, work_buf_next, partial_out;

  // Record the number of 2*VEC byte buffers wrote to out_buf.
  out_count = 0;
  int tree_output_lines = tree_size_bits / (VEC * 16);

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

  // Write as much as possible of the huffman tree to the output.
  for (i=0; i<tree_output_lines; i++) {
#pragma HLS pipeline
    data.write(tree_b[i]);
    size.write(-1);
  }

  if (tree_size_bits > tree_output_lines * VEC * 16) {
    partial_out = tree_b[tree_output_lines];
  } else {
    partial_out = 0;
  }
  out_count = tree_output_lines;
  bit_pos = tree_size_bits;

  for (i = 0; i < in_batch_count; i++) {
#pragma HLS PIPELINE II=1
    uint32 hcode[4*VEC];
    uint32 hlen[4*VEC];
    uint16 zero16 = 0;
    vec_2t hcode0_out, hlen0_out;
    vec_2t hcode1_out, hlen1_out;
    vec_2t hcode2_out, hlen2_out;
    vec_2t hcode3_out, hlen3_out;

    // Starting bit position of each code.
    uint32 pos[4*VEC];
    // Starting short position of each code.
    uint32 s_pos[4*VEC];

    // Compressed and packed data -- ready to send to output. Use double buffering
    // to reduce stall.
    // We know lcode, lextra, dcode, dextra's huffman codes are no more than 16 
    // bits, therefore it's impossible in one iteration to use up both buffers.
    uint16 local_pack[VEC];
#pragma HLS ARRAY_PARTITION variable=local_pack complete
    uint16 local_pack_next[VEC];
#pragma HLS ARRAY_PARTITION variable=local_pack_next complete

    // Load data from channels
    hcode0.read(hcode0_out); hlen0.read(hlen0_out);
    hcode1.read(hcode1_out); hlen1.read(hlen1_out);
    hcode2.read(hcode2_out); hlen2.read(hlen2_out);
    hcode3.read(hcode3_out); hlen3.read(hlen3_out);
    for (j = 0; j < VEC; j++) {
      hcode[j] = (zero16, hcode0_out((j+1)*16-1, j*16));
      hcode[j+VEC] = (zero16, hcode1_out((j+1)*16-1, j*16));
      hcode[j+2*VEC] = (zero16, hcode2_out((j+1)*16-1, j*16));
      hcode[j+3*VEC] = (zero16, hcode3_out((j+1)*16-1, j*16));
      hlen[j] = (zero16, hlen0_out((j+1)*16-1, j*16));
      hlen[j+VEC] = (zero16, hlen1_out((j+1)*16-1, j*16));
      hlen[j+2*VEC] = (zero16, hlen2_out((j+1)*16-1, j*16));
      hlen[j+3*VEC] = (zero16, hlen3_out((j+1)*16-1, j*16));
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

    int old_bit_pos = bit_pos;
    int new_bit_pos = pos[4*VEC-1] + hlen[4*VEC-1] + old_bit_pos;
    // bit pos is the register to carry through the loop so we update immediately
    bit_pos = new_bit_pos;

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
    work_buf = uint16_to_vec_2t(local_pack);
    work_buf_next = uint16_to_vec_2t(local_pack_next);

    cout << "I: " << i << "; ";
    cout << "Out count: " << out_count << "; ";
    cout << "Old bit pos: " << old_bit_pos << "; ";
    cout << "New bit pos: " << new_bit_pos << endl;
    cout << "Partial: " << hex << partial_out << dec << endl;
    if (new_bit_pos / (VEC*16) > out_count) {
      data.write(work_buf | partial_out);
      size.write(-1);
      partial_out = work_buf_next;
      out_count++;
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
    data.read(data_read);
    size.read(size_read);
    if (size_read == -1) {
      int slot = count % (32/VEC);
      cout << "Data read: " << hex << data_read << dec << endl;
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
void deflate259(uint512 *in_buf, int in_size, uint512* tree_buf, int tree_size,
                uint512 *out_buf, int *out_size) {
#pragma HLS DATAFLOW
#pragma HLS INTERFACE m_axi port=in_buf offset=slave bundle=gmem1 depth=100
#pragma HLS INTERFACE m_axi port=tree_buf offset=slave bundle=gmem2 depth=10
#pragma HLS INTERFACE m_axi port=out_buf offset=slave bundle=gmem3 depth=110
#pragma HLS INTERFACE m_axi port=out_size offset=slave bundle=gmem4 depth=1
#pragma HLS INTERFACE s_axilite port=in_size bundle=control
#pragma HLS INTERFACE s_axilite port=tree_size bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  stream<vec_t> literals("literals");
#pragma HLS STREAM variable=literals depth=1024

  stream<vec_2t> len("len");
#pragma HLS STREAM variable=len depth=1024
  stream<vec_2t> dist("dist");
#pragma HLS STREAM variable=dist depth=1024
  stream<vec_t> valid("valid");
#pragma HLS STREAM variable=valid depth=1024

  stream<vec_2t> hcode0("hcode0");
#pragma HLS STREAM variable=hcode0 depth=1024
  stream<vec_2t> hcode1("hcode1");
#pragma HLS STREAM variable=hcode1 depth=1024
  stream<vec_2t> hcode2("hcode2");
#pragma HLS STREAM variable=hcode2 depth=1024
  stream<vec_2t> hcode3("hcode3");
#pragma HLS STREAM variable=hcode3 depth=1024

  stream<vec_2t> hlen0("hlen0");
#pragma HLS STREAM variable=hlen0 depth=1024
  stream<vec_2t> hlen1("hlen1");
#pragma HLS STREAM variable=hlen1 depth=1024
  stream<vec_2t> hlen2("hlen2");
#pragma HLS STREAM variable=hlen2 depth=1024
  stream<vec_2t> hlen3("hlen3");
#pragma HLS STREAM variable=hlen3 depth=1024

  stream<vec_2t> data("data");
#pragma HLS STREAM variable=data depth=1024
  stream<int> size("size");
#pragma HLS STREAM variable=size depth=1024

  int in_size_local = in_size;

  feed(in_buf, in_size_local, literals);

  mock_lz77(literals, len, dist, valid, in_size_local);

  parallel_huffman_encode(len, dist, valid, in_size_local,
      hcode0, hlen0, hcode1, hlen1, hcode2, hlen2, hcode3, hlen3);

  write_huffman_output(tree_buf, tree_size,
      hcode0, hlen0, hcode1, hlen1, hcode2, hlen2, hcode3, hlen3,
      in_size_local, data, size);

  export_data(data, size, out_buf, out_size);
}
}
