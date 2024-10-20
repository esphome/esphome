/*
 * Copyright (C) 2013-2015 Willy Tarreau <w@1wt.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE /* for F_SETPIPE_SZ */
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <fcntl.h>
#include "slz.h"
#if defined(__linux__)
#include <linux/fs.h>
#endif


/* some platforms do not provide PAGE_SIZE */
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

/* display the message and exit with the code */
__attribute__((noreturn)) void die(int code, const char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(code);
}

__attribute__((noreturn)) void usage(const char *name, int code)
{
	die(code,
	    "Usage: %s [option]* [file]\n"
	    "\n"
	    "The following arguments are supported :\n"
	    "  -0         disable compression, only uses output format\n"
	    "  -1         compress faster\n"
	    "  -2         compress better\n"
	    "  -3 .. -9   compress even better [default]\n"
	    "  -b <size>  only use <size> bytes from the input file\n"
	    "  -B         use buffered mode instead of mmap (uses less memory)\n"
	    "  -c         send output to stdout [default]\n"
	    "  -f         force sending output to a terminal\n"
	    "  -h         display this help\n"
	    "  -l <loops> loop <loops> times over the same file\n"
	    "  -n         does nothing, just for gzip compatibility\n"
	    "  -t         test mode: do not emit anything\n"
	    "  -v         increase verbosity\n"
	    "\n"
	    "  -D         use raw Deflate output format (RFC1951)\n"
	    "  -G         use Gzip output format (RFC1952) [default]\n"
	    "  -Z         use Zlib output format (RFC1950)\n"
	    "\n"
	    "If no file is specified, stdin will be used instead.\n"
	    "\n"
	    ,name);
}


int main(int argc, char **argv)
{
	const char *name = argv[0];
	struct stat statbuf;
	struct slz_stream strm;
	unsigned char *outbuf;
	unsigned char *buffer;
	off_t toread = -1;
	off_t tocompress = 0;
	off_t ofs;
	size_t outblen;
	size_t outbsize;
	size_t block_size;
	size_t mapsize = 0;
	unsigned long long totin = 0;
	unsigned long long totout = 0;
	int buffer_mode = 0;
	int loops = 1;
	int console = 1;
	int level   = 3;
	int verbose = 0;
	int test    = 0;
	int format  = SLZ_FMT_GZIP;
	int force   = 0;
	int fd = 0;
	int error = 0;

	argv++;
	argc--;

	while (argc > 0) {
		if (**argv != '-')
			break;

		if (argv[0][0] == '-' && argv[0][1] >= '0' && argv[0][1] <= '9')
			level = argv[0][1] - '0';

		else if (strcmp(argv[0], "-b") == 0) {
			if (argc < 2)
				usage(name, 1);
			toread = strtoll(argv[1], NULL, 0);
			argv++;
			argc--;
		}

		else if (strcmp(argv[0], "-c") == 0)
			console = 1;

		else if (strcmp(argv[0], "-f") == 0)
			force = 1;

		else if (strcmp(argv[0], "-h") == 0)
			usage(name, 0);

		else if (strcmp(argv[0], "-l") == 0) {
			if (argc < 2)
				usage(name, 1);
			loops = atoi(argv[1]);
			argv++;
			argc--;
		}

		else if (strcmp(argv[0], "-n") == 0)
			/* just for gzip compatibility */ ;

		else if (strcmp(argv[0], "-B") == 0)
			buffer_mode = 1;

		else if (strcmp(argv[0], "-t") == 0)
			test = 1;

		else if (strcmp(argv[0], "-v") == 0)
			verbose++;

		else if (strcmp(argv[0], "-D") == 0)
			format = SLZ_FMT_DEFLATE;

		else if (strcmp(argv[0], "-G") == 0)
			format = SLZ_FMT_GZIP;

		else if (strcmp(argv[0], "-Z") == 0)
			format = SLZ_FMT_ZLIB;

		else
			usage(name, 1);

		argv++;
		argc--;
	}

	if (argc > 0) {
		fd = open(argv[0], O_RDONLY);
		if (fd == -1) {
			perror("open()");
			exit(1);
		}
	}

	if (isatty(1) && !test && !force)
		die(1, "Use -f if you really want to send compressed data to a terminal, or -h for help.\n");

	slz_make_crc_table();
	slz_prepare_dist_table();

	block_size = 32768;
	if (level > 1)
		block_size *= 4; // 128 kB
	if (level > 2)
		block_size *= 8; // 1 MB

	outbsize = 2 * block_size; // allows to pack more than one full output at each round
	outbuf = calloc(1, outbsize + 4096);
	if (!outbuf) {
		perror("calloc");
		exit(1);
	}

	/* principle : we'll determine the input file size and try to map the
	 * file at once. If it works we have a single input buffer of the whole
	 * file size. If it fails we'll bound the input buffer to the buffer size
	 * and read the input in smaller blocks.
	 */
	if (toread < 0) {
		if (fstat(fd, &statbuf) == -1) {
			toread = 0;
		}
		else {
			toread = statbuf.st_size;

#if defined(BLKGETSIZE64)
			/* block devices are reported as size zero */
			if (statbuf.st_mode & S_IFBLK) {
				if (ioctl(fd, BLKGETSIZE64, &toread) == -1)
					toread = 0;
			}
#endif

#if defined(F_GETPIPE_SZ) && defined(F_SETPIPE_SZ)
			/* attempt to optimize the pipe size if needed and possible */
			if (S_ISFIFO(statbuf.st_mode)) {
				int size = fcntl(fd, F_GETPIPE_SZ);
				if (size > 0 && size < block_size)
					fcntl(fd, F_SETPIPE_SZ, block_size);
			}
#endif
		}
	}

	/* try to increase output pipe size to avoid blocking on writes */
#if defined(F_GETPIPE_SZ) && defined(F_SETPIPE_SZ)
	if (fstat(1, &statbuf) != -1) {
		/* attempt to optimize the pipe size if needed and possible */
		if (S_ISFIFO(statbuf.st_mode)) {
			int size = fcntl(1, F_GETPIPE_SZ);
			if (size > 0 && size < block_size)
				fcntl(1, F_SETPIPE_SZ, block_size);
		}
	}
#endif

	if (toread && !buffer_mode) {
		/* we know the size to map, let's do it */
		mapsize = (toread + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		buffer = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
		if (buffer == MAP_FAILED)
			mapsize = 0;
		else {
#if defined(MADV_DONTDUMP)
			madvise((void *)buffer, mapsize, MADV_DONTDUMP);
#endif
#if defined(MADV_SEQUENTIAL)
			madvise((void *)buffer, mapsize, MADV_SEQUENTIAL);
#endif
		}
	}

	if (!mapsize) {
		/* no mmap() done, read the size of a default block */
		buffer_mode = 1;
		mapsize = block_size;
		if (toread && toread < mapsize)
			mapsize = toread;

		buffer = calloc(1, mapsize);
		if (!buffer) {
			perror("calloc");
			exit(1);
		}

		if (toread && toread <= mapsize) {
			/* file-at-once, read it now */
			toread = read(fd, buffer, toread);
			if (toread < 0) {
				perror("read");
				exit(2);
			}
		}

		if (loops > 1 && !toread) {
			loops = 1;
			fprintf(stderr, "Warning: disabling loops on non-regular file\n");
		}
	}

	while (loops--) {
		slz_init(&strm, !!level, format);

		outblen = ofs = 0;
		do {
			int more = !toread || (toread - ofs) > block_size;
			unsigned char *start;

			if (toread && toread <= mapsize) {
				/* We use the memory-mapped file so the next
				 * block starts at the buffer + file offset. We
				 * read by blocks of <block_size> bytes at one
				 * except the last one.
				 */
				tocompress = more ? block_size : toread - ofs;
				start = buffer + ofs;
			}
			else {
				/* we'll try to fill at least half a buffer with
				 * input data, it ensures we compress reasonably
				 * well without having to wait too much for the
				 * sender when it's a pipe.
				 */
				ssize_t ret;
				tocompress = 0;

				do {
					ret = read(fd, buffer + tocompress, more ? block_size - tocompress : toread - ofs - tocompress);
					if (ret <= 0) {
						if (ret < 0) {
							perror("read");
							exit(2);
						}
						break;
					}
					tocompress += ret;
				} while (tocompress < block_size / 3);

				if (!tocompress) // done
					break;

				start = buffer;
			}
			outblen += slz_encode(&strm, outbuf + outblen, start, tocompress, more);

#if defined(MADV_DONTNEED)
			if (!buffer_mode && (((ofs + tocompress) ^ ofs) & 2097152)) {
				/* we've crossed a MB boundary, let's release the previous MB */
				madvise((void *)(buffer + (ofs & -2097152)), 2097152, MADV_DONTNEED);
			}
#endif
			if (outblen + block_size > outbsize) {
				/* not enough space left, need to flush */
				if (console && !test && !error)
					if (write(1, outbuf, outblen) < 0)
						error = 1;
				totout += outblen;
				outblen = 0;
			}
			ofs += tocompress;
		} while (!toread || ofs < toread);

		outblen += slz_finish(&strm, outbuf + outblen);
		totin += ofs;
		totout += outblen;
		if (console && !test && !error)
			if (write(1, outbuf, outblen) < 0)
				error = 1;

		if (loops && (!toread || toread > mapsize)) {
			/* this is a seeked file, let's rewind it now */
			lseek(fd, 0, SEEK_SET);
		}
	}
	if (verbose)
		fprintf(stderr, "totin=%llu totout=%llu ratio=%.2f%% crc32=%08x\n",
		        totin, totout, totout * 100.0 / totin, strm.crc32);

	return error;
}
