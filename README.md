# Threaded File Compression

This is a very basic program that can compress and decompress a file with a configurable amount of threads 4096 bytes at a time. 

## Code Structure
`deflate_file()` - The main function for compressing a given file.  It works by reading 4096 bytes from the input file and it then assigns it to a worker thread.  Then the main thread will query each worker thread to see if there is any compressed data to be dumped, making the output is in the correct order.

`def()` - Handles the compression of each 4096 chunk.

`inflate_file()` - Reads the compressed file and writes the decompressed data to the filename + '.uc'
