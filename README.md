# Threaded File Compression

This is a very basic program that can compress and decompress a file with a configurable amount of threads 4096 bytes at a time. 

## Code Structure
`deflate_file()` - The main function for compressing a given file.  It works by reading 4096 bytes from the input file and it then assigns it to a worker thread.  Then the main thread will query each worker thread to see if there is any compressed data to be dumped, making the output is in the correct order.

`def()` - Handles the compression of each 4096 chunk.

`inflate_file()` - Reads the compressed file and writes the decompressed data to the filename + '.uc'

## How to Build
1. Clone this repo
2. Navigate to the zlib directory and run `make` followed by `make install` to build zlib
3. Go back to the src directory and run `gcc main.c zlib/libz.a -lpthread -Wall`

## Running
To run the program you need to specify whether you want to compress or decompress a file, this is specified with -c and -d respectively.

### For Compression
`./a.out -c file_to_compress #_of_threads`

### For Decompression
`./a.out -d file_to_decompress.zl`
