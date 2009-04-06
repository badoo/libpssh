#include "xsys.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *xmalloc(size_t size)
{
    void *buf = malloc(size);
    assert(buf != NULL);
    return buf;
}

void *xrealloc(void *ptr, size_t size)
{
    void *buf = realloc(ptr, size);
    assert(buf != NULL);
    return buf;
}

ssize_t xwrite(int fd, const void *buf, size_t count)
{
    ssize_t wr;
    ssize_t off = 0;

    while (count > 0) {
        wr = write(fd, (char *)buf+off, count);
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

char *xstrdup(const char *s)
{
    char *ret = strdup(s);
    assert(ret != NULL);
    return ret;
}
