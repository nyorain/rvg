#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define NK_ASSERT(expr) assert(expr)

struct nk_vec2 {
	float x;
	float y;
};

struct nk_recti {short x,y,w,h;};

typedef union {
	void *ptr;
	int id;
} nk_handle;

typedef void*(*nk_plugin_alloc)(nk_handle, void *old, size_t);
typedef void (*nk_plugin_free)(nk_handle, void *old);

struct nk_allocator {
    nk_handle userdata;
    nk_plugin_alloc alloc;
    nk_plugin_free free;
};


static struct nk_vec2
nk_vec2(float x, float y)
{
    struct nk_vec2 ret;
    ret.x = x; ret.y = y;
    return ret;
}

static int nk_ifloorf(float x) { return (int) floorf(x); }
static int nk_iceilf(float x) { return (int) ceilf(x); }

#define nk_zero_struct(s) memset(&s, 0, sizeof(s))

#ifdef __cplusplus
template<typename T> struct nk_alignof;
template<typename T, int size_diff> struct nk_helper{enum {value = size_diff};};
template<typename T> struct nk_helper<T,0>{enum {value = nk_alignof<T>::value};};
template<typename T> struct nk_alignof{struct Big {T x; char c;}; enum {
    diff = sizeof(Big) - sizeof(T), value = nk_helper<Big, diff>::value};};
#define NK_ALIGNOF(t) (nk_alignof<t>::value)
#elif defined(_MSC_VER)
#define NK_ALIGNOF(t) (__alignof(t))
#else
#define NK_ALIGNOF(t) ((char*)(&((struct {char c; t _h;}*)0)->_h) - (char*)0)
#endif

/* Pointer to Integer type conversion for pointer alignment */
#if defined(__PTRDIFF_TYPE__) /* This case should work for GCC*/
# define NK_UINT_TO_PTR(x) ((void*)(__PTRDIFF_TYPE__)(x))
# define NK_PTR_TO_UINT(x) ((size_t)(__PTRDIFF_TYPE__)(x))
#elif !defined(__GNUC__) /* works for compilers other than LLVM */
# define NK_UINT_TO_PTR(x) ((void*)&((char*)0)[x])
# define NK_PTR_TO_UINT(x) ((size_t)(((char*)x)-(char*)0))
#elif defined(NK_USE_FIXED_TYPES) /* used if we have <stdint.h> */
# define NK_UINT_TO_PTR(x) ((void*)(uintptr_t)(x))
# define NK_PTR_TO_UINT(x) ((uintptr_t)(x))
#else /* generates warning but works */
# define NK_UINT_TO_PTR(x) ((void*)(x))
# define NK_PTR_TO_UINT(x) ((size_t)(x))
#endif

#define NK_ALIGN_PTR(x, mask)\
    (NK_UINT_TO_PTR((NK_PTR_TO_UINT((uint8_t*)(x) + (mask-1)) & ~(mask-1))))
#define NK_ALIGN_PTR_BACK(x, mask)\
    (NK_UINT_TO_PTR((NK_PTR_TO_UINT((uint8_t*)(x)) & ~(mask-1))))

#define NK_MIN(a,b) ((a) < (b) ? (a) : (b))
#define NK_MAX(a,b) ((a) < (b) ? (b) : (a))
#define NK_CLAMP(i,v,x) (NK_MAX(NK_MIN(v,x), i))
