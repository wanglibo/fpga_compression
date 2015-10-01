/* huffman_translate.c
 * Created by Libo Wang for CS259 project, 5/28/15
 *
 * Here we build a function to translate (literal, distance) or (L, D)
 * pair into (huffman code, bit length) pair.
 *
 * The first step is to convert (L, D) into (Lcode, Lextra, Dcode, Dextra)
 * tuple. We can perform this based on the table below, taken from RFC
 * 1951 directly.
 *
 * Then we perform huffman lookup.We use a domain specific huffman tree, 
 * generated from experiment SAM file format. 
 *
 * The actual C code for the tree is generated. Actually to make the code
 * HLS friendly, we prefer manually entered or generated, precomputed 
 * static data.
 */


/*
Deutsch                      Informational                     [Page 11]

RFC 1951      DEFLATE Compressed Data Format Specification      May 1996


                 Extra               Extra               Extra
            Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
            ---- ---- ------     ---- ---- -------   ---- ---- -------
             257   0     3       267   1   15,16     277   4   67-82
             258   0     4       268   1   17,18     278   4   83-98
             259   0     5       269   2   19-22     279   4   99-114
             260   0     6       270   2   23-26     280   4  115-130
             261   0     7       271   2   27-30     281   5  131-162
             262   0     8       272   2   31-34     282   5  163-194
             263   0     9       273   3   35-42     283   5  195-226
             264   0    10       274   3   43-50     284   5  227-257
             265   1  11,12      275   3   51-58     285   0    258
             266   1  13,14      276   3   59-66

         The extra bits should be interpreted as a machine integer
         stored with the most-significant bit first, e.g., bits 1110
         represent the value 14.

                  Extra           Extra               Extra
             Code Bits Dist  Code Bits   Dist     Code Bits Distance
             ---- ---- ----  ---- ----  ------    ---- ---- --------
               0   0    1     10   4     33-48    20    9   1025-1536
               1   0    2     11   4     49-64    21    9   1537-2048
               2   0    3     12   5     65-96    22   10   2049-3072
               3   0    4     13   5     97-128   23   10   3073-4096
               4   1   5,6    14   6    129-192   24   11   4097-6144
               5   1   7,8    15   6    193-256   25   11   6145-8192
               6   2   9-12   16   7    257-384   26   12  8193-12288
               7   2  13-16   17   7    385-512   27   12 12289-16384
               8   3  17-24   18   8    513-768   28   13 16385-24576
               9   3  25-32   19   8   769-1024   29   13 24577-32768
*/

// Domain specific huffman tree extracted when compressing SAM file.
//#include "huffman_tree.h"
/*

*/

#include <stdio.h>
#include "constant.h"

#define L_EXTRA 29 // from 257 to 285
#define D_EXTRA 30
#define LMAX 286
#define DMAX 30

static unsigned char l_bound[2*L_EXTRA] = {
0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96, 112,128,160,192,224,255,
0,1,2,3,4,5,6,7,9,11,13,15,19,23,27,31,39,47,55,63,79,95,111,127,159,191,223,254,255
};
static unsigned short d_bound[2*D_EXTRA] = {
1,2,3,4,5,7,9, 13,17,25,33,49,65,97, 129,193,257,385,513,769, 1025,1537,2049,3073,4097,6145,8193, 12289,16385,24577,
1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768
};

static unsigned char l_extra_bits[L_EXTRA] = {
0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};

static unsigned char d_extra_bits[D_EXTRA] = {
0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

unsigned lfreq[LMAX];
unsigned dfreq[DMAX];

#ifdef COUNT_FREQ
void reset_freq()
{
  int i;
  for (i=0; i<LMAX; i++) lfreq[i] = 0;
  for (i=0; i<DMAX; i++) dfreq[i] = 0;
}

void dump_freq()
{
  int i;
  FILE *fp_l, *fp_d;
  fp_l = fopen("lfreq.txt", "w");
  fp_d = fopen("dfreq.txt", "w");
  for (i=0; i<LMAX; i++) {
    fprintf(fp_l, "%d\n", lfreq[i]);
  }
  for (i=0; i<DMAX; i++) {
    fprintf(fp_d, "%d\n", dfreq[i]);
  }
  fclose(fp_l);
  fclose(fp_d);
}
#endif

// Translate literal, distance pair into corresponding huffman encoding and bit count.
// If distance is non zero, l = matching distance - 3; otherwise l is literal.
// Assume "code" field has at least 64 bits available.
void huffman_translate(unsigned short l, unsigned short d, uint128 *out_buf, uint128 *outl_buf)
{
#pragma HLS PIPELINE II=1
  int il, id;
  unsigned char ll, lh;  // l's lower and upper bound for each interval
  unsigned short dl, dh; // d's lower and upper bound for each interval
  unsigned char lres[L_EXTRA];
  unsigned char dres[D_EXTRA];
  unsigned out_buf1[4];
  unsigned outl_buf1[4];

  int lindex=0, dindex=0;

unsigned short ltree[572] = {
13, 2975,		 // 0
13, 7071,		 // 1
13, 1951,		 // 2
13, 6047,		 // 3
13, 3999,		 // 4
13, 8095,		 // 5
13, 95,		 // 6
13, 4191,		 // 7
13, 2143,		 // 8
7, 43,		 // 9
11, 367,		 // 10
13, 6239,		 // 11
13, 1119,		 // 12
13, 5215,		 // 13
13, 3167,		 // 14
13, 7263,		 // 15
13, 607,		 // 16
13, 4703,		 // 17
13, 2655,		 // 18
13, 6751,		 // 19
13, 1631,		 // 20
13, 5727,		 // 21
13, 3679,		 // 22
13, 7775,		 // 23
13, 351,		 // 24
13, 4447,		 // 25
13, 2399,		 // 26
13, 6495,		 // 27
13, 1375,		 // 28
13, 5471,		 // 29
13, 3423,		 // 30
13, 7519,		 // 31
11, 1391,		 // " "
11, 879,		 // "!"
13, 863,		 // """
13, 4959,		 // "#"
11, 1903,		 // "$"
9, 207,		 // "%"
8, 23,		 // "&"
8, 151,		 // "'"
8, 87,		 // "("
8, 215,		 // ")"
8, 55,		 // "*"
7, 107,		 // "+"
8, 183,		 // ","
8, 119,		 // "-"
8, 247,		 // "."
8, 15,		 // "/"
7, 27,		 // "0"
6, 5,		 // "1"
6, 37,		 // "2"
7, 91,		 // "3"
6, 21,		 // "4"
6, 53,		 // "5"
6, 13,		 // "6"
6, 45,		 // "7"
7, 59,		 // "8"
6, 29,		 // "9"
6, 61,		 // ":"
6, 3,		 // ";"
7, 123,		 // "<"
8, 143,		 // "="
11, 239,		 // ">"
11, 1263,		 // "?"
12, 287,		 // "@"
9, 463,		 // "A"
12, 2335,		 // "B"
9, 47,		 // "C"
12, 1311,		 // "D"
12, 3359,		 // "E"
12, 799,		 // "F"
9, 303,		 // "G"
11, 751,		 // "H"
12, 2847,		 // "I"
13, 2911,		 // "J"
13, 7007,		 // "K"
12, 1823,		 // "L"
11, 1775,		 // "M"
10, 111,		 // "N"
12, 3871,		 // "O"
13, 1887,		 // "P"
11, 495,		 // "Q"
13, 5983,		 // "R"
11, 1519,		 // "S"
9, 175,		 // "T"
12, 159,		 // "U"
12, 2207,		 // "V"
13, 3935,		 // "W"
13, 8031,		 // "X"
13, 223,		 // "Y"
13, 4319,		 // "Z"
13, 2271,		 // "["
13, 6367,		 // "\"
13, 1247,		 // "]"
13, 5343,		 // "^"
9, 431,		 // "_"
13, 3295,		 // "`"
11, 1007,		 // "a"
13, 7391,		 // "b"
12, 1183,		 // "c"
12, 3231,		 // "d"
11, 2031,		 // "e"
11, 31,		 // "f"
13, 735,		 // "g"
13, 4831,		 // "h"
11, 1055,		 // "i"
13, 2783,		 // "j"
13, 6879,		 // "k"
12, 671,		 // "l"
12, 2719,		 // "m"
12, 1695,		 // "n"
11, 543,		 // "o"
12, 3743,		 // "p"
11, 1567,		 // "q"
12, 415,		 // "r"
12, 2463,		 // "s"
12, 1439,		 // "t"
13, 1759,		 // "u"
13, 5855,		 // "v"
13, 3807,		 // "w"
12, 3487,		 // "x"
13, 7903,		 // "y"
13, 479,		 // "z"
13, 4575,		 // "{"
13, 2527,		 // "|"
13, 6623,		 // "}"
13, 1503,		 // "~"
13, 5599,		 // 127
13, 3551,		 // 128
13, 7647,		 // 129
13, 991,		 // 130
13, 5087,		 // 131
13, 3039,		 // 132
13, 7135,		 // 133
13, 2015,		 // 134
13, 6111,		 // 135
13, 4063,		 // 136
13, 8159,		 // 137
13, 63,		 // 138
13, 4159,		 // 139
13, 2111,		 // 140
13, 6207,		 // 141
13, 1087,		 // 142
13, 5183,		 // 143
13, 3135,		 // 144
13, 7231,		 // 145
13, 575,		 // 146
13, 4671,		 // 147
13, 2623,		 // 148
13, 6719,		 // 149
13, 1599,		 // 150
13, 5695,		 // 151
13, 3647,		 // 152
13, 7743,		 // 153
13, 319,		 // 154
13, 4415,		 // 155
13, 2367,		 // 156
13, 6463,		 // 157
13, 1343,		 // 158
13, 5439,		 // 159
13, 3391,		 // 160
13, 7487,		 // 161
13, 831,		 // 162
13, 4927,		 // 163
13, 2879,		 // 164
13, 6975,		 // 165
13, 1855,		 // 166
13, 5951,		 // 167
13, 3903,		 // 168
13, 7999,		 // 169
13, 191,		 // 170
13, 4287,		 // 171
13, 2239,		 // 172
13, 6335,		 // 173
13, 1215,		 // 174
13, 5311,		 // 175
13, 3263,		 // 176
13, 7359,		 // 177
13, 703,		 // 178
13, 4799,		 // 179
13, 2751,		 // 180
13, 6847,		 // 181
13, 1727,		 // 182
13, 5823,		 // 183
13, 3775,		 // 184
13, 7871,		 // 185
13, 447,		 // 186
13, 4543,		 // 187
13, 2495,		 // 188
13, 6591,		 // 189
13, 1471,		 // 190
13, 5567,		 // 191
13, 3519,		 // 192
13, 7615,		 // 193
13, 959,		 // 194
13, 5055,		 // 195
13, 3007,		 // 196
13, 7103,		 // 197
13, 1983,		 // 198
13, 6079,		 // 199
13, 4031,		 // 200
13, 8127,		 // 201
13, 127,		 // 202
13, 4223,		 // 203
13, 2175,		 // 204
13, 6271,		 // 205
13, 1151,		 // 206
13, 5247,		 // 207
13, 3199,		 // 208
13, 7295,		 // 209
13, 639,		 // 210
13, 4735,		 // 211
13, 2687,		 // 212
13, 6783,		 // 213
13, 1663,		 // 214
13, 5759,		 // 215
13, 3711,		 // 216
13, 7807,		 // 217
13, 383,		 // 218
13, 4479,		 // 219
13, 2431,		 // 220
13, 6527,		 // 221
13, 1407,		 // 222
13, 5503,		 // 223
13, 3455,		 // 224
13, 7551,		 // 225
13, 895,		 // 226
13, 4991,		 // 227
13, 2943,		 // 228
13, 7039,		 // 229
13, 1919,		 // 230
13, 6015,		 // 231
13, 3967,		 // 232
13, 8063,		 // 233
13, 255,		 // 234
13, 4351,		 // 235
13, 2303,		 // 236
13, 6399,		 // 237
13, 1279,		 // 238
13, 5375,		 // 239
13, 3327,		 // 240
13, 7423,		 // 241
13, 767,		 // 242
13, 4863,		 // 243
13, 2815,		 // 244
13, 6911,		 // 245
13, 1791,		 // 246
13, 5887,		 // 247
13, 3839,		 // 248
13, 7935,		 // 249
13, 511,		 // 250
13, 4607,		 // 251
13, 2559,		 // 252
13, 6655,		 // 253
13, 1535,		 // 254
13, 5631,		 // 255
13, 3583,		 // 256
2, 0,		 // 257
3, 2,		 // 258
4, 6,		 // 259
5, 14,		 // 260
5, 30,		 // 261
5, 1,		 // 262
6, 35,		 // 263
6, 19,		 // 264
5, 17,		 // 265
5, 9,		 // 266
5, 25,		 // 267
7, 7,		 // 268
6, 51,		 // 269
7, 71,		 // 270
7, 39,		 // 271
6, 11,		 // 272
7, 103,		 // 273
8, 79,		 // 274
10, 623,		 // 275
12, 927,		 // 276
13, 7679,		 // 277
13, 1023,		 // 278
13, 5119,		 // 279
13, 3071,		 // 280
13, 7167,		 // 281
13, 2047,		 // 282
13, 6143,		 // 283
13, 4095,		 // 284
13, 8191		 // 285
};

unsigned short dtree[60] = {
7, 15,		 // 0
9, 191,		 // 1
9, 447,		 // 2
9, 127,		 // 3
7, 79,		 // 4
7, 47,		 // 5
7, 111,		 // 6
7, 31,		 // 7
7, 95,		 // 8
9, 383,		 // 9
11, 1023,		 // 10
11, 2047,		 // 11
10, 511,		 // 12
8, 63,		 // 13
3, 0,		 // 14
9, 255,		 // 15
4, 4,		 // 16
4, 12,		 // 17
4, 2,		 // 18
4, 10,		 // 19
4, 6,		 // 20
4, 14,		 // 21
4, 1,		 // 22
4, 9,		 // 23
4, 5,		 // 24
4, 13,		 // 25
4, 3,		 // 26
5, 7,		 // 27
4, 11,		 // 28
5, 23		 // 29
};

  // We first translate the (L,D) pair into the deflate code values and extra bits
  unsigned l_extra, d_extra, l_extra_len, d_extra_len, match;
  unsigned l_code, d_code;

  // Output buffer
  unsigned lcode_outb = 0, dcode_outb = 0, llen_outb = 0, dlen_outb = 0;

  // Compute offset in l_extra regardless, although matching distance may be 0
  for (il = 0; il < L_EXTRA; il++) {
    ll = l_bound[il];
    lh = l_bound[il+L_EXTRA];
    if ((ll <= l) && (l <= lh)) {
      lindex = il; 
    }
  }

  // Compute offset in d_extra regardless
  for (id = 0; id < D_EXTRA; id++) {
    dl = d_bound[id];
    dh = d_bound[id+D_EXTRA];
    if ((dl <= d) && (d <= dh)) {
      dindex = id;  // Caller must guarantee d <= 32768
    }
  }

  if (d == 0) {
    l_code = l; l_extra = 0; l_extra_len = 0; 
    d_code = 0; d_extra = 0; d_extra_len = 0;
  } else {
    l_code = lindex + 257; l_extra_len = l_extra_bits[lindex]; l_extra = l - l_bound[lindex];
    d_code = dindex; d_extra_len = d_extra_bits[dindex]; d_extra = d - d_bound[dindex];
  }

#ifdef COUNT_FREQ
  if (d!=0) {
    dfreq[d_code]++;
  }
  lfreq[l_code]++;
#endif

  // Now we just need to lookup the huffman tree for our code and output them in 
  // the following order: l_code, l_extra, d_code, d_extra
  //
  //    MSB                                                LSB
  //      [d_extra]  |  [d_code]  |  [l_extra]  |  [l_code]
  //
  // Extra bit fields are in big endian order.

  // lcode
  llen_outb = ltree[l_code * 2];
  lcode_outb = ltree[l_code * 2 + 1];

  //if (l_extra_len > 0) {
  //  lcode_outb |= (l_extra << llen_outb);
  //  llen_outb += l_extra_len;
  //}

  // dcode: lookup regardless of matching distance
  dlen_outb = dtree[d_code * 2];
  dcode_outb = dtree[d_code * 2 + 1];

  //if (d_extra_len > 0) {
  //  dcode_outb |= (d_extra << dlen_outb);
  //  dlen_outb += d_extra_len;
  //}

  // If it's a literal, then set dcode length to 0
  if (d == 0) {
    dlen_outb = 0;
    d_extra_len = 0;
  }

  if (llen_outb == 0) lcode_outb = 0;
  if (dlen_outb == 0) dcode_outb = 0;
  if (l_extra_len == 0) l_extra = 0;
  if (d_extra_len == 0) d_extra = 0;
/*
  // huffmand translate output dump
  fprintf(stderr, "Input (L,D) = (%d(%02x), %d)\n", l, l, d);
  fprintf(stderr, "Tuple (Lcode, Llen, Lextra, Lelen, Dcode, Dlen, Dextra, Delen)"
      " = (%x,%d,%x,%d,%x,%d,%x,%d)\n", lcode_outb, llen_outb, l_extra, 
      l_extra_len, dcode_outb, dlen_outb, d_extra, d_extra_len);
*/
  // Promode matching result to 32 bits for convenience of alignment
  out_buf1[0] = lcode_outb;
  out_buf1[1] = l_extra;
  out_buf1[2] = dcode_outb;
  out_buf1[3] = d_extra;

  outl_buf1[0] = llen_outb;
  outl_buf1[1] = l_extra_len;
  outl_buf1[2] = dlen_outb;
  outl_buf1[3] = d_extra_len;

#if 0
  *out_buf = *((uint128*)(out_buf1));
  *outl_buf = *((uint128*)(outl_buf1));
#else
  *out_buf = uint32_to_uint128(out_buf1);
  *outl_buf = uint32_to_uint128(outl_buf1);
#endif
}

/*
void huffman_local_pack(unsigned hcode[4*VEC], unsigned pos[4*VEC],
    unsigned short *local_pack_row, unsigned *bit_len)
{
  unsigned pos[4*VEC];
  unsigned i;
  pos[0] = 0;
  for (i=0; i<4*VEC; i++) {
    pos[i] = pos[i-1] + hlen[i-1];
  }
  *bitlen = pos[4*VEC-1] + hlen[4*VEC-1];
  // Perform pre-shift
  for (i=0; i<4*VEC; i++) {
    hcode[i] = hcode[i] << (pos[i] % 16);
  }
  for (i=0; i<4*VEC; i++) {

  }
}*/
