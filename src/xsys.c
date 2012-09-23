/* Copyright (c) 2008-2009 Dmitry Novikov <rushba at eyelinkmedia dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials
 *   provided with the distribution.
 *
 *   Neither the name of the copyright holder nor the names
 *   of any other contributors may be used to endorse or
 *   promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include "xsys.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *xmalloc(size_t size)
{				/* {{{ */
	void *buf = malloc(size);
	assert(buf != NULL);
	return buf;
}

/* }}} */

void *xrealloc(void *ptr, size_t size)
{				/* {{{ */
	void *buf = realloc(ptr, size);
	assert(buf != NULL);
	return buf;
}

/* }}} */

ssize_t xwrite(int fd, const void *buf, size_t count)
{				/* {{{ */
	ssize_t wr;
	ssize_t off = 0;

	while (count > 0) {
		wr = write(fd, (char *)buf + off, count);
		if (wr < 0) {
			if (errno == EPIPE) {
				printf("fd: %d: EPIPE!\n", fd);
				break;
			} else {
				printf("write: %d (%s)\n", errno, strerror(errno));
				return -1;
			}
		}
		off += wr;
		count -= wr;
	}
	return off;
}

/* }}} */

char *xstrdup(const char *s)
{				/* {{{ */
	char *ret = strdup(s);
	assert(ret != NULL);
	return ret;
}

/* }}} */
