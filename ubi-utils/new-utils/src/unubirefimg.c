/*
 * An utility to reformat the image file generated by mkfs.ubifs
 *
 * Authors:   Yurong Tan (Nancy)
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


#define PROGRAM_VERSION "1.1"
#define PROGRAM_NAME    "ubirefimg"
#define UBI_LEB_SIZE  516096

/*
 * sourcefile usually generated by mkfs.ubifs.
 * usage: #ubirefimg sourcefile outputfile
 */
int main(int argc, char * const argv[])
{
	int err, ifd, ofd, i;
	struct stat st;
	unsigned char *buf=NULL;

	buf = malloc(UBI_LEB_SIZE);
	if(buf==NULL){
		printf("no mem\n");
		goto out_free;
	}

	err = stat(argv[1], &st);
	if (err < 0) {
		printf("stat failed on \"%s\"", argv[1]);
		goto out_free;
	}

	ifd = open(argv[1],  O_RDONLY);
	if (ifd == -1) {
		printf("cannot open \"%s\"", argv[1]);
		goto out_close;
	}

	ofd = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (ofd == -1) {
		printf("cannot create \"%s\"", argv[2]);
		goto out_close;
	}

	i = 0;
	for (;;) {
		unsigned int lnum;
		err = read(ifd, &lnum, sizeof(unsigned int));
		if (err == 0) {
			break;
		} else if (err != sizeof(unsigned int)) {
			printf("read error\n");
			goto out_close1;
		};

		// Write skipped unmapped LEBs
		if (i != lnum)
			memset(buf, 0xff, UBI_LEB_SIZE);
		while (i++ != lnum) {
			err = write(ofd, buf, UBI_LEB_SIZE);
			if (err != UBI_LEB_SIZE) {
				printf("write error\n");
				goto out_close1;
			}
		}

		// Write LEB's data
		err = read(ifd, buf, UBI_LEB_SIZE);
		if (err != UBI_LEB_SIZE) {
			printf("read error\n");
			goto out_close1;
		}

		err = write(ofd, buf, UBI_LEB_SIZE);
		if (err != UBI_LEB_SIZE) {
			printf("write error\n");
			goto out_close1;
		}
	}

	free(buf);
	close(ifd);
	close(ofd);
	return 0;

out_close1:
	close(ofd);
out_close:
	close(ifd);
out_free:
	free(buf);
	return -1;
}

