#include <stdio.h>
#include <stdlib.h>
#include "ap_cint.h"

#define N 127
#define NT 64

// Use a function to avoid compiler optimizing the data access away.
void prepare_input(char* in_char, char* tree_char) {
  int i;
  for (i=0; i<N; i++) {
    in_char[i] = 1;
   // printf("in_char[%d] = %02x\n", i, in_char[i]);
  }

  for (i=0; i<NT; i++) {
    tree_char[i] = 2;
   // printf("in_char[%d] = %02x\n", i, tree_char[i]);
  }
}

void deflate259(uint512 *in_buf, int in_size, uint512* tree_buf, int tree_size,
                uint512 *out_buf, int *out_size);

int main() {
  int i;
  uint512 in0[100];
  uint512 tree0[100];
  uint512 out0[200];
  uint512 answer[200];
  int out_size;

  char *in_char, *tree_char, *answer_char, *out_char;
  in_char = (char*) in0;
  tree_char = (char*) tree0;
  answer_char = (char*) answer;
  out_char = (char*) out0;

  prepare_input(in_char, tree_char);

  for (i=0; i<NT+N; i++) {
    if (i < NT) {
      answer_char[i] = 2;
    } else {
      answer_char[i] = 1;
    }
  }

  deflate259(in0, N, tree0, 8*NT, out0, &out_size);

  for (i=0; i<N; i++) {
    if (answer_char[i] != out_char[i]) {
      printf("Mismatch at position %d, expected %d, got %d.\n",
          i, answer_char[i], out_char[i]);
      return 1;
    }
  }
  if (out_size != (NT+N)) {
    fprintf(stderr, "Output size mismatch, expected %d, got %d.\n",
        NT+N, out_size);
    return 1;
  }

  return 0;
}
