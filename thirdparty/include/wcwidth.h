#ifndef __WCWIDTH_H
#define __WCWIDTH_H

#include <stdint.h>
#include <stddef.h>

int mk_wcwidth(uint32_t);
int mk_wcswidth(const uint32_t *, size_t);
int mk_wcwidth_cjk(uint32_t);
int mk_wcswidth_cjk(const uint32_t *, size_t);

#endif /* __WCWIDTH_H */
