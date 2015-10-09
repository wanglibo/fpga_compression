#include "constant.h"

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

#if VEC>8
  uint16 red16[16]; uint16 red16[16];
#pragma HLS ARRAY_PARTITION variable=red16 complete
#pragma HLS ARRAY_PARTITION variable=red16i complete
#endif

#if VEC==8
  // Here we break the abstraction of VEC.
  for (i=0; i<8; i++) {
    red8[i] = input[i]; red8i[i] = i;
  }
#elif VEC==16
  for (i=0; i<16; i++) {
    red16[i] = input[i]; red16i[i] = i;
  }
#endif

#if VEC>8
  for (i=0; i<8; i++) {
    uint16 l = red16[2*i], r = red16[2*i+1];
    uint16 li = red16i[2*i], ri = red16i[2*i+1];
    if (l >= r) {
      red8[i] = l; red8i[i] = li;
    } else {
      red8[i] = r; red8i[i] = ri;
    }
  }
#endif
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

#if VEC>8
  uint16 red16[16]; uint16 red16[16];
#pragma HLS ARRAY_PARTITION variable=red16 complete
#pragma HLS ARRAY_PARTITION variable=red16i complete
#endif

#if VEC==8
  // Here we break the abstraction of VEC.
  for (i=0; i<8; i++) {
    red8[i] = input[i]; red8i[i] = i;
  }
#elif VEC==16
  for (i=0; i<16; i++) {
    red16[i] = input[i]; red16i[i] = i;
  }
#endif

#if VEC>8
  for (i=0; i<8; i++) {
    uint16 l = red16[2*i], r = red16[2*i+1];
    uint16 li = red16i[2*i], ri = red16i[2*i+1];
    if (l <= r) {
      red8[i] = l; red8i[i] = li;
    } else {
      red8[i] = r; red8i[i] = ri;
    }
  }
#endif
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

/*
// Min with min index
void min_reduction(uint16 input[VEC], uint16 *value, uint16 *index) {
  int i;
#if VEC==8
  min_reduction_8(input, value, index);
#endif
}
// Max with min index
void max_reduction(uint16 input[VEC], uint16 *value, uint16 *index) {
  int i;
#if VEC==8
  max_reduction_8(input, value, index);
#endif
}
*/
