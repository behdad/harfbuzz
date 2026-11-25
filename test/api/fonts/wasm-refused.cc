/* The Wasm modules of wasm-import.ttf, wasm-import-libc.ttf,
 * wasm-allocator.ttf and wasm-start-section.ttf.  The last three each
 * have one thing about them that keeps a module from being run when
 * the limits are compiled in; test-wasm.c checks that.
 * generate-wasm-api-font.py compiles this once for each. */

#define HB_WASM_INTERFACE(ret_t, name) \
  __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

#ifdef IMPORT
/* A function that nothing provides, as modules made with wasm-bindgen
 * import for their JavaScript glue. */
__attribute__((import_module("other"), import_name("function")))
extern void other_function (void);
#endif

#ifdef IMPORT_LIBC
/* A function of the runtime's own C library, if it has one. */
__attribute__((import_module("env"), import_name("strlen")))
extern unsigned long runtime_strlen (const char *s);
#endif

#ifdef ALLOCATOR
/* The runtime runs these for the allocations Harfbuzz makes in the
 * module. */
static char heap[1 << 12];
static unsigned used;

extern "C" {

__attribute__((export_name("malloc")))
void *
malloc (unsigned long size)
{
  if (size > sizeof heap - used)
    return nullptr;
  void *p = heap + used;
  used += (size + 7) & ~7u;
  return p;
}

__attribute__((export_name("free")))
void
free (void *p)
{
}

}
#endif

#ifdef START
/* generate-wasm-api-font.py makes this the module's start function. */
static unsigned started;

extern "C"
__attribute__((export_name("early")))
void
early (void)
{
  started = 1;
}
#endif

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
#ifdef IMPORT
  /* Never called.  That the module imports it is the point. */
  if (num_features == 0x7FFFFFFF)
    other_function ();
#endif
#ifdef IMPORT_LIBC
  static const char text[] = "abc";
  return runtime_strlen (text) == 3;
#endif
  return true;
}
