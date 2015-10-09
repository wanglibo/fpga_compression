// Created by Libo Wang for testing FPGA deflate implementation. OpenCL
// wrapper created on 6/11/15.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h> // performance analysis
#include <sys/time.h>
#include <stdint.h>
#ifdef SDACCEL_HOST
#include <CL/opencl.h>
#endif

#include "ap_cint.h"
void deflate259(uint512 *in_buf, int in_size,
                uint512 *out_buf, int *out_size);

#define CHUNK 64*1024

#define DO_VERIFY 1

int64_t get_time_us( void ) {
#if 0
  struct timeval tv_date;
  gettimeofday( &tv_date, NULL);
  return ((int64_t) tv_date.tv_sec * 1000000 + (int64_t) tv_date.tv_usec);
#else
  return 0;
#endif
}

#ifdef SDACCEL_HOST
int
load_file_to_memory(const char *filename, char **result)
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
  *result = (char *)malloc(size+1);
  if (size != fread(*result, sizeof(char), size, f)) 
  { 
    free(*result);
    return -2; // -2 means file reading fail 
  } 
  fclose(f);
  (*result)[size] = 0;
  return size;
}

int deflate259_opencl(unsigned char* input, unsigned in_len, unsigned char* tree,
  unsigned tree_len, unsigned char* output, unsigned* out_len)
{
#define SDACCEL_WRAPPER
#ifdef SDACCEL_WRAPPER
  int err;                            // error code returned from api calls

  cl_platform_id platform_id;         // platform id
  cl_device_id device_id;             // compute device id 
  cl_context context;                 // compute context
  cl_command_queue commands;          // compute command queue
  cl_program program;                 // compute program
  cl_kernel kernel;                   // compute kernel
   
  char cl_platform_vendor[1001];
  char cl_platform_name[1001];

  err = clGetPlatformIDs(1,&platform_id,NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to find an OpenCL platform!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  err = clGetPlatformInfo(platform_id,CL_PLATFORM_VENDOR,1000,(void *)cl_platform_vendor,NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("CL_PLATFORM_VENDOR %s\n",cl_platform_vendor);
  err = clGetPlatformInfo(platform_id,CL_PLATFORM_NAME,1000,(void *)cl_platform_name,NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: clGetPlatformInfo(CL_PLATFORM_NAME) failed!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("CL_PLATFORM_NAME %s\n",cl_platform_name);

  // Connect to a compute device
  //
  int fpga = 0;
#if defined (FPGA_DEVICE)
  fpga = 1;
#endif
  err = clGetDeviceIDs(platform_id, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
                       1, &device_id, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to create a device group!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Create a compute context 
  //
  context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
  if (!context)
  {
    printf("Error: Failed to create a compute context!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Create a command commands
  //
  commands = clCreateCommandQueue(context, device_id, 0, &err);
  if (!commands)
  {
    printf("Error: Failed to create a command commands!\n");
    printf("Error: code %i\n",err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  int status;

  // Create Program Objects
  //
  
  // Load binary from disk
  unsigned char *kernelbinary;
  char xclbin[]="deflate1.xclbin";
  printf("loading %s\n", xclbin);
  int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
  if (n_i < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  size_t n = n_i;
  // Create the compute program from offline
  program = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, &status, &err);
  if ((!program) || (err!=CL_SUCCESS)) {
    printf("Error: Failed to create compute program from binary %d!\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Build the program executable
  //
  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    size_t len;
    char buffer[2048];

    printf("Error: Failed to build program executable!\n");
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the compute kernel in the program we wish to run
  //
  kernel = clCreateKernel(program, "deflate259", &err);
  if (!kernel || err != CL_SUCCESS)
  {
    printf("Error: Failed to create compute kernel! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the input and output arrays in device memory for our calculation
//  void deflate259_opencl(unsigned char* input, unsigned in_len, unsigned char* tree,
//    unsigned tree_len, unsigned char* output, unsigned* out_len)
  cl_mem input_arg, in_len_arg, tree_arg, tree_len_arg, output_arg, out_len_arg;
  input_arg = clCreateBuffer(context,  CL_MEM_READ_ONLY, CHUNK, NULL, NULL);
  in_len_arg = clCreateBuffer(context,  CL_MEM_READ_ONLY, sizeof(unsigned), NULL, NULL);
  tree_arg = clCreateBuffer(context,  CL_MEM_READ_ONLY, 512, NULL, NULL);
  tree_len_arg = clCreateBuffer(context,  CL_MEM_READ_ONLY, sizeof(unsigned), NULL, NULL);
  output_arg = clCreateBuffer(context, CL_MEM_WRITE_ONLY, CHUNK*2, NULL, NULL);
  out_len_arg = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(unsigned), NULL, NULL);

  if (!input_arg || !in_len_arg || !tree_arg || !tree_len_arg || !output_arg || !out_len_arg)
  {
    printf("Error: Failed to allocate device memory!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }    

  err = clEnqueueWriteBuffer(commands, input_arg, CL_TRUE, 0, in_len, input, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array input!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  err = clEnqueueWriteBuffer(commands, in_len_arg, CL_TRUE, 0, sizeof(unsigned), &in_len, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array &in_len!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  err = clEnqueueWriteBuffer(commands, tree_arg, CL_TRUE, 0, 512, tree, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array tree!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  err = clEnqueueWriteBuffer(commands, tree_len_arg, CL_TRUE, 0, sizeof(unsigned), &tree_len, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array &tree_len!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Set the arguments to our compute kernel
//void deflate259_opencl(unsigned char* input, unsigned in_len, unsigned char* tree,
//  unsigned tree_len, unsigned char* output, unsigned* out_len)
  err = 0;
  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_arg);
  err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &in_len_arg);
  err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &tree_arg);
  err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &tree_len_arg);
  err |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &output_arg);
  err |= clSetKernelArg(kernel, 5, sizeof(cl_mem), &out_len_arg);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel arguments! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Execute the kernel over the entire range of our 1d input data set
  // using the maximum number of work group items for this device
  //

#ifdef C_KERNEL
  err = clEnqueueTask(commands, kernel, 0, NULL, NULL);
#else
  size_t global[1];
  size_t local[1];
  global[0] = 1;
  local[0] = 1;
  err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL, 
                               (size_t*)&global, (size_t*)&local, 0, NULL, NULL);
#endif
  if (err)
  {
    printf("Error: Failed to execute kernel! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Read back the results from the device to verify the output
  //
  cl_event readevent;
  unsigned out_len_b;
  err = clEnqueueReadBuffer( commands, out_len_arg, CL_TRUE, 0, sizeof(unsigned),
      &out_len_b, 0, NULL, &readevent );
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output length! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  clWaitForEvents(1, &readevent);
  *out_len = out_len_b;

  printf("Read final output length: %d\n", out_len_b);

  err = clEnqueueReadBuffer( commands, output_arg, CL_TRUE, 0, out_len_b, output, 0, NULL, &readevent );
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output data! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  clWaitForEvents(1, &readevent);
#endif
}
#endif // #ifdef SDACCEL_HOST

// Tester
int def259(FILE *source) {
    unsigned char in[CHUNK];
    unsigned char in_res[CHUNK];
    unsigned char out[CHUNK*2]; // In case we have an overflow.
    unsigned char tree[512];
    int in_len, out_len, tree_len, tree_bytes, in_res_len, i;
    int tmp;

    int64_t start, end;
    start = get_time_us();

    tmp = fread(in, 1, CHUNK, source);
    if (tmp < 0) {
        fprintf(stderr, "Failed to read input file.\n");
        return 1;
    }
    in_len = tmp;
    FILE *fp = fopen("sam_tree.bin", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to read tree file.\n");
        return 1;
    }
    // To avoid stream corruption
    for (i=0; i<512; i++) {
      tree[i] = 0;
    }
    tree_bytes = fread(tree, 1, 512, fp);
    fprintf(stderr, "Tree bytes: %d\n", tree_bytes);
    // The last item of tree is how many bits are valid within the last non-empty byte.
    // Value is from 0 to 8.
    tree_len = 8*(tree_bytes-2)+tree[tree_bytes-1];
    fprintf(stderr, "Huffman tree loaded, length = %d bits.\n", tree_len);
#ifdef SDACCEL_HOST
    deflate259_opencl(in, in_len, tree, tree_len, out, &out_len);
#else
{
    int j, k;
    uint512 in_hw[CHUNK/64];
    uint512 out_hw[CHUNK*2/64];
    uint512 tree_hw[512/64];
    uint512 tmp512;
    /*
    for (i = 0; i < CHUNK/64; i++) {
        char_512_t tmp;
        for (j = 0; j < 64; j++)
            tmp.content[j] = in[i*64+j];
        in_hw[i] = tmp.wide;
    }

    for (i = 0; i < 512/64; i++) {
        char_512_t tmp;
        for (j = 0; j < 64; j++)
            tmp.content[j] = tree[i*64+j];
        tree_hw[i] = tmp.wide;
    }*/

    //prepare_input((unsigned char*)in_hw, in, in_len);
    //prepare_input((unsigned char*)tree_hw, tree, tree_len);

    //deflate259(in_hw, in_len, tree_hw, tree_len, out_hw, &out_len);

    //prepare_output((unsigned char*)out_hw, out, out_len);
    deflate259(in, in_len, out, &out_len);
/*    for (i = 0; i < CHUNK*2/64; i++) {
        char_512_t tmp;
        tmp.wide = out_hw[i];
        for (j = 0; j < 64; j++)
            out[i*64+j] = tmp.content[j];
    }*/
}
#endif
    end = get_time_us();

    fprintf(stderr, "Deflate complete, size after compression: %d. Verifying output...\n", out_len);
    FILE *dest = fopen("ex1.sam.def0", "w");
    if ( fwrite(out, 1, out_len, dest) == out_len) {
      fprintf(stderr, "Dumped compressed output successfully.\n");
      fclose(dest);
    }

#if DO_VERIFY==1
    unsigned char out_res[CHUNK*2]; // In case we have an overflow.
    int out_res_len;

    fp = fopen("ex1.sam.gold.def0", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to read golden file.\n");
        return 1;
    }
    out_res_len = fread(out_res, 1, CHUNK*2, fp);

    //out_res_len = 21461;
    fprintf(stderr, "Output length: expected %d, got %d.\n", out_res_len, out_len);

    for (i=0; i<((out_res_len>out_len)?out_res_len:out_len); i++) {
      if (out[i] != out_res[i]) {
        fprintf(stderr, "Byte mismatch at position %d: expected %02x, got %02x.", i, out_res[i], out[i]);
        return 1;
      }
    }

/*
    {
        int st = system("diff --brief ex1.sam.gold.def0 ex1.sam.def0");

        if (st != 0) return Z_DATA_ERROR;
    }
*/
#endif

    // Stop the timer and calculate throughput
#define PERFORMANCE_DUMP
#ifdef PERFORMANCE_DUMP
    float elapsed = (float)(end-start)/1000000.0;
    fprintf(stderr, "======================\n");
    fprintf(stderr, "Input size: %d bytes (%f MB)\n", in_len, (float)1.0*in_len/1024/1024);
    fprintf(stderr, "Output size: %d bytes (%f MB)\n", out_len, (float)1.0*out_len/1024/1024);
    fprintf(stderr, "Compression ratio: %f\n", (float)1.0*in_len/out_len);
    fprintf(stderr, "Elapsed time: %f seconds\n", elapsed);
    fprintf(stderr, "Throughput: %f MB/s\n", (float)1.0*in_len/1024/1024/elapsed);
#endif
    return 0;
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;

    FILE *inFile, *outFile;

    int i;
    int compress = 1;
    int level;

    inFile = fopen("ex1.sam", "r");
    if (!inFile) {
      fprintf(stderr, "Failed to open input file '%s'!\n", argv[1]);
      exit(1);
    }
    ret = def259(inFile);
    if (ret != 0) {
      printf("Test failed !!!\n");
      return 1;
    }
    return 0;
}
