#ifndef __STRUCT_H
#define __STRUCT_H

typedef void (*free_fn)(void *mem);
typedef void (*freep_fn)(void **mem);
#define FREE_EITHER(obj, mem)                                                  \
    ((obj)->free ? (obj)->free(mem) : (obj)->freep ? (obj)->freep(&(mem)) : 0)
#define FREE_EITHER_NP(obj, mem)                                               \
    ((obj).free ? (obj).free(mem) : (obj).freep ? (obj).freep(&(mem)) : 0)

#endif /* __STRUCT_H */
