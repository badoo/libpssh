#ifndef XSYS_H
#define XSYS_H

#include <sys/types.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
ssize_t xwrite(int fd, const void *buf, size_t count);

char *xstrdup(const char *s);

#endif
