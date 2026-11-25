#include <stdlib.h>
#include <malloc/malloc.h>
#include <stdint.h>
static _Thread_local int armed;
static _Thread_local unsigned long long calls;
void labrc_start_heap_audit(void)
{
    calls = 0;
    armed = 1;
}
unsigned long long labrc_stop_heap_audit(void)
{
    armed = 0;
    return calls;
}
static inline void hit(void)
{
    if (armed)
        ++calls;
}
#define INTERPOSE(new, old)                                                                                  \
    __attribute__((used)) static const struct                                                                \
    {                                                                                                        \
        const void* replacement;                                                                             \
        const void* original;                                                                                \
    } hook_##old __attribute__((section("__DATA,__interpose"))) = {(const void*)(uintptr_t)&new,             \
                                                                   (const void*)(uintptr_t)&old}
static void* my_malloc(size_t n)
{
    hit();
    return malloc(n);
}
INTERPOSE(my_malloc, malloc);
static void* my_calloc(size_t n, size_t m)
{
    hit();
    return calloc(n, m);
}
INTERPOSE(my_calloc, calloc);
static void* my_realloc(void* p, size_t n)
{
    hit();
    return realloc(p, n);
}
INTERPOSE(my_realloc, realloc);
static void my_free(void* p)
{
    hit();
    free(p);
}
INTERPOSE(my_free, free);
static int my_posix_memalign(void** p, size_t a, size_t n)
{
    hit();
    return posix_memalign(p, a, n);
}
INTERPOSE(my_posix_memalign, posix_memalign);
static void* my_zone_malloc(malloc_zone_t* z, size_t n)
{
    hit();
    return malloc_zone_malloc(z, n);
}
INTERPOSE(my_zone_malloc, malloc_zone_malloc);
static void my_zone_free(malloc_zone_t* z, void* p)
{
    hit();
    malloc_zone_free(z, p);
}
INTERPOSE(my_zone_free, malloc_zone_free);
