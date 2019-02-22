#include "wandio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lzo/lzo1x.h>
#include <lzo/lzoconf.h>

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct lzo_t {

	io_t *parent;
	enum err_t err;

	lzo_bytep in;
	lzo_uint new_len;
};

extern io_source_t lzo_source;

#define DATA(io) ((struct lzo_t *)((io)->data))

io_t *lzo_open(io_t *parent) {
	io_t *io;
	if (!parent) {
		return NULL;
	}

	io = malloc(sizeof(io_t));
	io->source = &lzo_source;
	io->data = malloc(sizeof(struct lzo_t));

	DATA(io)->parent = parent;
	DATA(io)->err = ERR_OK;

	if (lzo_init() != LZO_E_OK) {
                fprintf(stderr, "lzo failed to init\n");
                /* return something? */
        }

	DATA(io)->in = (lzo_bytep) malloc(WANDIO_BUFFER_SIZE);

	if (DATA(io)->in == NULL) {
		fprintf(stderr, "Unable to allocate memory\n");
		/* return something? */
	}

	return io;
}

static int64_t lzo_read(io_t *io, void *buffer, int64_t len) {
	int r;

	if (DATA(io)->err == ERR_EOF) {
		return 0;
	}
	if (DATA(io)->err == ERR_ERROR) {
		return -1;
	}

	/* read in some data */
	int bytes = wandio_read(DATA(io)->parent,
		DATA(io)->in,
		WANDIO_BUFFER_SIZE);

	/* check for EOF */
	if (bytes == 0) {
		DATA(io)->err = ERR_EOF;
		return 0;
	}

	/* check for error */
	if (bytes < 0) {
		DATA(io)->err = ERR_ERROR;
		return -1;
	}

	int i;
	unsigned char *p = (unsigned char *)DATA(io)->in;
	fprintf(stderr, "data to decompress: ");
	for (i=0;i<bytes;i++) {
		fprintf(stderr, "%02x ", p[i]);
	}
	fprintf(stderr, "\n");

	/* decompress the data */
	r = lzo1x_decompress_safe(DATA(io)->in, bytes, buffer, &DATA(io)->new_len, NULL);
	if (r == LZO_E_OK) {

	} else {
		printf("internal error - decompression failed: %d\n", r);
		return 1;
	}

	fprintf(stderr, "new len: %lu\n", DATA(io)->new_len);
        unsigned char *d = (unsigned char *)buffer;
        fprintf(stderr, "data decompressed: ");
        for (i=0;i<5;i++) {
                fprintf(stderr, "%02x ", d[i]);
        }
        fprintf(stderr, "\n");

	return DATA(io)->new_len;
}

static void lzo_close(io_t *io) {
	wandio_destroy(DATA(io)->parent);
	//lzo_free(DATA(io)->out);
	free(io->data);
	free(io);
}

io_source_t lzo_source = {
	"lzo",
	lzo_read,
	NULL, 		/* peek */
	NULL,		/* tell */
	NULL,		/* seek */
	lzo_close
};
