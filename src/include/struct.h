#ifndef __STRUCT_H
#define __STRUCT_H

#define FREE_DEF(name)  void (*name)(void *mem)
#define FREEP_DEF(name) void (*name)(void **mem)
#define FREE_EITHER(obj, mem)                                                  \
    ((obj)->free ? (obj)->free(mem) : (obj)->freep ? (obj)->freep(&(mem)) : 0)


#endif /* __STRUCT_H */
