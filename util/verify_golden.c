// Modified by Libo Wang for testing fpga algorithm output on 9/29/15.

/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "zlib.h"
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 64*1024
#define OUTPUT_MAX 64*1024*1024

void* mallocOrDie(size_t size) {
  void* ret = malloc(size);
  if (!ret) {
    fprintf(stderr, "Failed to allocate memory of size %d!\n", size);
    exit(1);
  }
  return ret;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf_buf(unsigned char *source, unsigned in_len, unsigned char *dest, 
    unsigned *out_len)
{
    int ret;
    unsigned have;
    z_stream strm;
    //unsigned char in[CHUNK];
    //unsigned char out[CHUNK];

    unsigned char *in = (unsigned char*)mallocOrDie(CHUNK);
    unsigned char *out = (unsigned char*)mallocOrDie(CHUNK);

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    // TODO: take raw input stream
    //ret = inflateInit(&strm);
    ret = inflateInit2(&strm, -MAX_WBITS);
    if (ret != Z_OK)
        return ret;
    unsigned in_pos = 0;
    unsigned out_pos = 0;

    /* decompress until deflate stream ends or end of file */
    do {
        if (in_pos + CHUNK < in_len) {
          strm.avail_in = CHUNK;
        } else {
          strm.avail_in = in_len - in_pos;
        }
        //strm.avail_in = fread(in, 1, CHUNK, source);
        //if (ferror(source)) {
        //    (void)inflateEnd(&strm);
        //    return Z_ERRNO;
        //}
        if (strm.avail_in == 0)
            break;
        memcpy((void*)in, (void*)(source + in_pos), CHUNK);
        in_pos += strm.avail_in;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (have != 0) {
              memcpy((void*)(dest + out_pos), (void*)out, have);
              out_pos += have;
            }
            //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
            //    (void)inflateEnd(&strm);
            //    return Z_ERRNO;
            //}
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    *out_len = out_pos;
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

int load_file_to_memory(const char *filename, char **result)
{
  int size = 0;
  FILE *f = fopen(filename, "rb");
  if (f == NULL) 
  {
    *result = NULL;
    return -1; // -1 means file opening fail 
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  *result = (char *)mallocOrDie(size+1);
  if (size != fread(*result, sizeof(char), size, f)) 
  { 
    free(*result);
    return -2; // -2 means file reading fail 
  } 
  fclose(f);
  (*result)[size] = 0;
  return size;
}

int main(int argc, char** argv) {
    FILE *golden_file, input_file;
    char *golden_buf, *input_buf, *output_buf;
    int golden_size, input_size, test_result, output_size;
    int i;

    if (argc < 3) {
        fprintf(stderr, "Usage: verify_golden [golden_file] [input_file].\n");
        exit(1);
    }

    golden_size = load_file_to_memory(argv[1], &golden_buf);
    if (golden_size < 0) {
        fprintf(stderr, "Cannot open golden file %s!\n", argv[1]);
        exit(1);
    }
    input_size = load_file_to_memory(argv[2], &input_buf);
    if (input_size < 0) {
        fprintf(stderr, "Failed to open input file %s!\n", argv[2]);
        exit(1);
    }

    int output_buf_size = OUTPUT_MAX;
/*
    if (input_size > CHUNK) {
        printf("Input size is larger than limit (%d), output may overflow.\n",
             CHUNK);
    }
*/
    output_buf = (char*)mallocOrDie(output_buf_size);
    output_size = 0;

    test_result = inf_buf(golden_buf, golden_size, output_buf, &output_size);
    if (test_result != Z_OK) {
        zerr(test_result);
        exit(1);
    }

    for (i=0; i<input_size && i<output_size; i++) {
        if (input_buf[i] != output_buf[i]) {
            fprintf(stderr, "Mismatch at pos %d: expected %02x, got %02x.\n",
                i, input_buf[i], output_buf[i]);
            return 1;
        }
    }
    if (input_size > output_size) {
        fprintf(stderr, "Output size (%d) is smaller than input size (%d)!\n",
               output_size, input_size);
        return 1;
    }
    if (input_size < output_size) {
        fprintf(stderr, "Output size (%d) is larger than input size (%d)!\n",
               output_size, input_size);
        return 1;
    }

    printf("Test passed.\n");
    return 0;
}
