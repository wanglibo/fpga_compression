#include <ap_int.h>
typedef ap_uint<512> uint512;

extern "C" {
#include <stdio.h>
void deflate259(uint512 *in_buf, int in_size, uint512* tree_buf, int tree_size,
                uint512 *out_buf, int *out_size) {
#pragma HLS INTERFACE m_axi port=in_buf offset=slave bundle=gmem1 depth=102400
#pragma HLS INTERFACE m_axi port=tree_buf offset=slave bundle=gmem2 depth=100
#pragma HLS INTERFACE m_axi port=out_buf offset=slave bundle=gmem3 depth=204800
#pragma HLS INTERFACE m_axi port=out_size offset=slave bundle=gmem4 depth=1
#pragma HLS INTERFACE s_axilite port=in_buf bundle=control
#pragma HLS INTERFACE s_axilite port=in_size bundle=control
#pragma HLS INTERFACE s_axilite port=tree_buf bundle=control
#pragma HLS INTERFACE s_axilite port=tree_size bundle=control
#pragma HLS INTERFACE s_axilite port=out_buf bundle=control
#pragma HLS INTERFACE s_axilite port=out_size bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  int i;
  int count=0;

  printf("in_size: %d\n", in_size);
  printf("tree_size: %d\n", tree_size);
/*
  for (i=0; i<tree_size/8; i++) {
    out_buf[count++] = tree_buf[i];
  }
  for (i=0; i<in_size; i++) {
    out_buf[count++] = in_buf[i];
  }*/
  out_size[0] = in_size + tree_size/8;
  printf("out_size: %d\n", in_size+tree_size/8);
}
}
