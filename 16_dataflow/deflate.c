void deflate259(int *in_buf, int* out_buf, unsigned avail_in) {
#pragma HLS INTERFACE m_axi port=in_buf offset=slave bundle=gmem1 depth=100
#pragma HLS INTERFACE m_axi port=out_buf offset=slave bundle=gmem1 depth=100
#pragma HLS INTERFACE s_axilite port=avail_in bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
  unsigned i;
  for (i=0; i<avail_in; i++) {
    out_buf[i] = in_buf[i] * 2 + 1;
  }
}
