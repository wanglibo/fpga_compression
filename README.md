# fpga_compression

Created by Libo Wang on 9/24/2015. Targeting alphadata board
(part xc7vx690tffg1157-2)

# Implementation plan

- Organize test input.
- Create testbench and invoke zlib. 
- Only include huffman encoding and DATAFLOW.
- Perform hardware execution of huffman encoding compressor.
- Add LZ77 incrementally and test hardware.

