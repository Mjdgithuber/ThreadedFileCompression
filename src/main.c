#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "zlib/zlib.h"

#define CHUNK_SIZE 4096

/* ZLib 'hack' for OS compatibility */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

typedef unsigned char BYTE;

/* Protos */
int def(BYTE* buffer_in, unsigned int buff_in_sz, BYTE* buffer_out, unsigned int buff_out_sz, unsigned int* output_sz);
int inf(BYTE* buffer_in, unsigned int buff_in_sz, BYTE* buffer_out, unsigned int buff_out_sz, unsigned int* output_sz);
int fpeek(FILE *stream);
long int file_size(FILE* fp);

void* compression(void* comp_info);






int fpeek(FILE *stream) {
	int c = fgetc(stream);
	ungetc(c, stream);
	return c;
}

/* Compress bytes from buffer source to buffer dest.
 *    def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
 *       allocated for processing, Z_STREAM_ERROR if an invalid compression
 *          level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
 *             version of the library linked do not match. 
 * Params:
 * buffer_in  - A buffer of uncompressed bytes
 * buff_in_sz - # of bytes to compress (must be multiple of cache line size)
 * buffer_out - A buffer to write compressed data to
 * output_sz  - Place to store # of compressed bytes */
int def(BYTE* buffer_in, unsigned int buff_in_sz, BYTE* buffer_out, unsigned int buff_out_sz, unsigned int* output_sz) {
    int ret;
    unsigned have;
    z_stream strm;

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_BEST_COMPRESSION); /* Z_DEFAULT_COMPRESSION */
    if (ret != Z_OK)
        return ret;

    strm.avail_in = buff_in_sz; /* # of avail bytes */
    strm.next_in  = buffer_in;  /* ptr to first byte of data */
    
    /*printf("Uncompressed Size: %d, first char: %c\n", buff_in_sz, *buffer_in);*/

    strm.avail_out = buff_out_sz; /* size of output buff */
    strm.next_out  = buffer_out;      /* ptr to first byte of o buff */
	
    ret = deflate(&strm, Z_FINISH); /* no bad return value */
    assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
	
    have = buff_out_sz - strm.avail_out;
    printf("Compressed Size: %d\n", have);
	
    /* 1 byte needed for 0 byte filler metadata */
    if(strm.avail_out == 0) {
	(void)deflateEnd(&strm);
	printf("Not enough room in output buffer!\n");
	return -1;
    }


    assert(strm.avail_in == 0); /* all input will be used */
    (*output_sz) = have;

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
} 

/* * Do a basic check to ensure block size is multiple of cachline
 * * Basic check to ensure that uncompressed size is >= than compressed space
 *
 *
 */
int inf(BYTE* buffer_in, unsigned int buff_in_sz, BYTE* buffer_out, unsigned int buff_out_sz, unsigned int* output_sz)
{

// inf(ib, 208, ob, 1000, &sz);


    int ret;
    unsigned have;
    z_stream strm;

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    strm.avail_in = buff_in_sz;
    strm.next_in  = buffer_in;

    strm.avail_out = buff_out_sz;
    strm.next_out  = buffer_out;
    
    ret = inflate(&strm, Z_NO_FLUSH);
    switch (ret) {
    case Z_NEED_DICT:
	ret = Z_DATA_ERROR;     /* and fall through */
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
    	(void)inflateEnd(&strm);
        return ret;
    }

    have = buff_out_sz - strm.avail_out;
    printf("Uncompressed Size: %d, Compressed Size: %d\n", have, buff_in_sz);
 	
    int i;
    for(i = 0; i < 350; i++) {
	printf("%c", buffer_out[i]);
	}  
	printf("\n");
	
	printf("strm.avail_in = %u\n", strm.avail_in);

    /*printf("Have: %d, avaible out: %d, left: %d\n", have, strm.avail_out, strm.avail_in);*/
    assert(strm.avail_in == 0);   
    (*output_sz) = have;

    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

long int file_size(FILE* fp) {
	long int ret;
	fseek(fp, 0, SEEK_END);
	ret = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return ret;
}
typedef struct {
	pthread_t thread;
	char alive; /* 0 - idle, 1 - running, 2 - need to be reset */

	BYTE* input_buf;
	BYTE* output_buf;
	int output_size; /* in bytes */
	int block_id; /* to maintain order */
	int input_size;
} worker_t;

void* compression(void* thread) {
	worker_t* worker = (worker_t*) thread;
	
	def(worker->input_buf, worker->input_size, worker->output_buf, CHUNK_SIZE * 1.2, &worker->output_size);

	worker->alive = 2;
	return NULL;
}

void emma(const char* input_fn, const char* output_fn, int n_workers) {
	FILE *i_fp, *o_fp;
	int i;
	worker_t* workers = calloc(n_workers, sizeof(worker_t));

	i_fp = fopen(input_fn, "r");
	o_fp = fopen(output_fn, "w");

	/* init workers */
	for(i = 0; i < n_workers; i++) {
		workers[i].input_buf = malloc(CHUNK_SIZE);
		workers[i].output_buf = malloc(CHUNK_SIZE * 1.2);
	}

	printf("Starting compression!\n");

	unsigned int read = CHUNK_SIZE;
	int read_id = 0;
	int write_id = 0;
	for(;;) {
		/* attempt to find idle threads */
		if(read == CHUNK_SIZE) {
			for(i = 0; i < n_workers; i++) {
				if(!workers[i].alive) {
					/* read input file into worker */
					read = fread(workers[i].input_buf, 1, CHUNK_SIZE, i_fp);
					if(!read) break;
					printf("Read %u bytes and assigned to thread %u!\n", read, i);
					workers[i].block_id = read_id++;
					workers[i].input_size = read;
					workers[i].alive = 1;

					/* start thread */
					pthread_create(&workers[i].thread, NULL, compression, &workers[i]);
				}
			}
		} else {
			/* break if input file is read and all threads are done */
			for(i = 0; i < n_workers; i++)
				if(workers[i].alive) goto end_loop;
			break;
			end_loop:;
		}

		/* check if any workers have data to write to the file */
		for(i = 0; i < n_workers; i++) {
			if(workers[i].alive == 2 && workers[i].block_id == write_id) {
				/* dump thread data and set it to idle */
				fwrite(workers[i].output_buf, workers[i].output_size, 1, o_fp);
				workers[i].alive = 0;

				/* ensure thread has completed */
				pthread_join(workers[i].thread, NULL);
				write_id++;
			}
		}
				
	}

	printf("Compression Finished! Cleaning up.\n");	

	fclose(i_fp);
	fclose(o_fp);

	/* free workers */
	for(i = 0; i < n_workers; i++) {
		free(workers[i].input_buf);
		free(workers[i].output_buf);
	}
	free(workers);
}

#define CHUNK 16384
int infx(FILE *source, FILE *dest) {
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    /* allocate inflate state */
    printf("Here!\n");
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
	printf("Emma!\n");
        strm.avail_in = fread(in, 1, CHUNK, source);
	printf("Read in %d\n", strm.avail_in);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
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
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);
	printf("Code %d  LEFT: %d\n", ret == Z_STREAM_END, strm.avail_in);
    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int defx2(FILE *source, FILE *dest)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
 *            compression if all of source has been read in */
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        assert(strm.avail_in == 0);     /* all input will be used */

	printf("Emma DONEOK!\n");
    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

int defx(FILE *source, FILE *dest)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
 *            compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
		printf("Error!\n");
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */
    printf("Emma! OK\n");
    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

int infx_mod(FILE *source, FILE *dest) {
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK*2];
    unsigned char out[CHUNK];
    /* allocate inflate state */
    printf("Here!\n");
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
	printf("Emma!\n");
        if(strm.avail_in == 0){
		strm.avail_in = fread(in, 1, CHUNK, source);
		have = 0;
	}
	printf("Read in %d\n", strm.avail_in);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
		printf("HERE!!!\n");
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
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }

	    if(ret == Z_STREAM_END) {
		int left = strm.avail_in;
		unsigned char* in_p = strm.next_in;
		(void)inflateEnd(&strm);
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = left;
		strm.next_in = in_p;
		printf("In offset: %d left to read: %d\n", (int)(in_p - in), strm.avail_in);
		ret = inflateInit(&strm);
	    }

        } while (strm.avail_out == 0 || strm.avail_in != 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END || strm.avail_in != 0);
	printf("Code %d  LEFT: %d\n", ret == Z_STREAM_END, strm.avail_in);
    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int main(int argc, char** argv) {
	//char output_fn[100];

	//emma("test_data.txt", "comp.zl", 2);

	/*FILE* fp, *fpo;
	fp = fopen("test2.txt", "r");//"test_data.txt"
	fpo = fopen("output.txt", "w");
	defx2(fp, fpo);*/

	FILE* fp, *fpo;
	fp = fopen("output.txt", "r");
	fpo = fopen("output.txt.uc", "w");
	infx_mod(fp, fpo);
	//infx(fp, fpo);
	//infx(fp, fpo);

	

	/*FILE* fp;
	fp = fopen("comp.zl", "r");
	BYTE* ib = malloc(208);
	BYTE* ob = malloc(1000);
	fread(ib, 186, 1, fp);
	int sz = 0;
	inf(ib, 186*2, ob, 1000, &sz);
	printf("Inf size %d\n", sz);
	fclose(fp);

	free(ib);
	free(ob);*/


	return 0;
}

