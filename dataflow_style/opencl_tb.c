#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <CL/opencl.h>

#include <sys/time.h>
#include <time.h>

#define INPUT_MAX 64*1024
#define TREE_MAX 512

////////////////////////////////////////////////////////////////////////////
//
//  Helper functions
//
////////////////////////////////////////////////////////////////////////////
typedef struct timespec timespec;
timespec diff(timespec start, timespec end);
timespec sum(timespec t1, timespec t2);
void printTimeSpec(timespec t);
timespec tic( );
void toc( timespec* start_time );
double get_current_msec( );

timespec diff(timespec start, timespec end)
{
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

timespec sum(timespec t1, timespec t2) {
  timespec temp;
  if (t1.tv_nsec + t2.tv_nsec >= 1000000000) {
    temp.tv_sec = t1.tv_sec + t2.tv_sec + 1;
    temp.tv_nsec = t1.tv_nsec + t2.tv_nsec - 1000000000;
  } else {
    temp.tv_sec = t1.tv_sec + t2.tv_sec;
    temp.tv_nsec = t1.tv_nsec + t2.tv_nsec;
  }
  return temp;
}

void printTimeSpec(timespec t) {
  printf("elapsed time: %d.%09d\n", (int)t.tv_sec, (int)t.tv_nsec);
}

void printTimeSpec_wrbuf(timespec t) {
  printf("write DRAM time: %d.%09d\n", (int)t.tv_sec, (int)t.tv_nsec);
}

timespec tic( )
{
  timespec start_time;
  clock_gettime(CLOCK_REALTIME, &start_time);
  return start_time;
}

void toc( timespec* start_time )
{
  timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  printTimeSpec( diff( *start_time, current_time ) );
  *start_time = current_time;
}

void toc_wrbuf( timespec* start_time )
{
  timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  printTimeSpec_wrbuf( diff( *start_time, current_time ) );
  *start_time = current_time;
}

double get_current_msec( )
{
  timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  return current_time.tv_nsec/1e6+current_time.tv_sec*1e3;
}


int openReadOrDie(char* filePath, char* buf, int maxSize) {
  FILE *fp = fopen(filePath, "r");
  if (!fp) {
    fprintf(stderr, "Failed to open file %s for read!\n");
    exit(1);
  }
  int readSize = fread(buf, 1, maxSize, fp);
  if (readSize < 0) {
    fprintf(stderr, "Failed to read from file %s!\n");
    exit(1);
  }
  fclose(fp);
  return readSize;
}

int openWriteOrDie(char* filePath, char* buf, int size) {
  FILE *fp = fopen(filePath, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open file %s for write!\n");
    exit(1);
  }
  int writeSize = fwrite(buf, 1, size, fp);
  if (writeSize < 0) {
    fprintf(stderr, "Failed to read from file %s!\n");
    exit(1);
  }
  fclose(fp);
  return writeSize;
}

void* mallocOrDie(size_t size) {
  void* ret = malloc(size);
  if (!ret) {
    fprintf(stderr, "Failed to allocate memory of size %d!\n", size);
    exit(1);
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////
//
//  Testbench
//
////////////////////////////////////////////////////////////////////////////

int deflate259_opencl(char* input, int in_len,
    char* output, int* out_len);

int main(int argc, char **argv)
{
  int ret, i;

  char *inFilePath = "/curr/libo/cs259/deflate_git/fpga_compression/dataflow_style/ex1.sam";
  char *outFilePath = "/curr/libo/cs259/deflate_git/fpga_compression/dataflow_style/ex1.sam.def0";
  char *treeFilePath = "/curr/libo/cs259/deflate_git/fpga_compression/dataflow_style/sam_tree.bin";
  char *goldenFilePath = "/curr/libo/cs259/deflate_git/fpga_compression/dataflow_style/ex1.sam.gold.def0";

  // Prepare inputs
  char *inBuf = (char*)mallocOrDie(INPUT_MAX);
  char *outBuf = (char*)mallocOrDie(INPUT_MAX*2);

  int tmp, inSize, treeByteSize, treeBitSize;
  inSize = openReadOrDie(inFilePath, inBuf, INPUT_MAX);

  // Execute api
  int outSize, goldenSize;
  double start = get_current_msec();
  int status = deflate259_opencl(inBuf, inSize,
      outBuf, &outSize);
  double end = get_current_msec();

  if (status != 0) {
    fprintf(stderr, "Deflate failed!\n");
    exit(1);
  }

  printf("Deflate complete, size after compression: %d. Verifying output...\n", outSize);

  if (openWriteOrDie(outFilePath, outBuf, outSize) == outSize) {
    printf("Dumped compressed output successfully.\n");
  }

  char *goldenBuf = (char*)mallocOrDie(INPUT_MAX*2);
  goldenSize = openReadOrDie(goldenFilePath, goldenBuf, INPUT_MAX*2);

  printf("Output length: expected %d, got %d.\n", goldenSize,
      outSize);

  for (i=0; i<((goldenSize>outSize)?goldenSize:outSize); i++) {
    if (outBuf[i] != goldenBuf[i]) {
      fprintf(stderr, "Byte mismatch at position %d: expected %02x, got %02x.", i, goldenBuf[i], outBuf[i]);
      exit(1);
    }
  }

  double elapsed = end - start;
  printf("======================\n");
  printf("Input size: %d bytes (%f MB)\n", inSize,
      (double)1.0*inSize/1024/1024);
  printf("Output size: %d bytes (%f MB)\n", outSize,
      (double)1.0*outSize/1024/1024);
  printf("Compression ratio: %f\n", (double)1.0*inSize/outSize);
  printf("Elapsed time: %f ms\n", elapsed);
  printf("Throughput: %f MB/s\n",
      (double)1000.0*inSize/1024/1024/elapsed);

  return 0;
}

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

int deflate259_opencl(char* input, int in_len,
    char* output, int* out_len) {
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
  printf("Loaded binary!\n");
  size_t n = n_i;
  // Create the compute program from offline
  program = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, &status, &err);
  if ((!program) || (err!=CL_SUCCESS)) {
    printf("Error: Failed to create compute program from binary %d!\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  printf("Created program from binary!\n");
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

  printf("Built program!\n");
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
  input_arg = clCreateBuffer(context, CL_MEM_READ_ONLY, INPUT_MAX, NULL, NULL);
  in_len_arg = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int), NULL, NULL);
  tree_arg = clCreateBuffer(context, CL_MEM_READ_ONLY, TREE_MAX, NULL, NULL);
  tree_len_arg = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int), NULL, NULL);
  output_arg = clCreateBuffer(context, CL_MEM_WRITE_ONLY, INPUT_MAX*2, NULL, NULL);
  out_len_arg = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int), NULL, NULL);

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
  printf("Written source array input!\n");

  err = clEnqueueWriteBuffer(commands, in_len_arg, CL_TRUE, 0, sizeof(int), &in_len, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array &in_len!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("Written in_len = %d\n", in_len);
/*
  err = clEnqueueWriteBuffer(commands, tree_arg, CL_TRUE, 0, TREE_MAX, tree, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array tree!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("Written source array tree!\n");

  err = clEnqueueWriteBuffer(commands, tree_len_arg, CL_TRUE, 0, sizeof(int), &tree_len, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to write to source array &tree_len!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("Written tree_len = %d\n", tree_len);
*/
  // Set the arguments to our compute kernel
//void deflate259_opencl(unsigned char* input, unsigned in_len, unsigned char* tree,
//  unsigned tree_len, unsigned char* output, unsigned* out_len)
  err = 0;
  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_arg);
  err |= clSetKernelArg(kernel, 1, sizeof(int), &in_len);
  //err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &tree_arg);
  //err |= clSetKernelArg(kernel, 3, sizeof(int), &tree_len);
  err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &output_arg);
  err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &out_len_arg);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel arguments! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  printf("Set arguments. Executing kernel!\n");

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
  printf("Scheduled kernel!\n");
  if (err)
  {
    printf("Error: Failed to execute kernel! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  // Read back the results from the device to verify the output
  //
  cl_event readevent;
  int out_len_b;
  err = clEnqueueReadBuffer( commands, out_len_arg, CL_TRUE, 0, sizeof(int),
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
  if (out_len_b < 0) {
    return EXIT_FAILURE;
  }

  err = clEnqueueReadBuffer( commands, output_arg, CL_TRUE, 0, out_len_b, output, 0, NULL, &readevent );
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output data! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
  clWaitForEvents(1, &readevent);
  printf("Read output buffer!\n");
  return 0;
}

