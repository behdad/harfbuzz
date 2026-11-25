/* The Wasm modules of wasm-allocator-grow.ttf,
 * wasm-allocator-reenter.ttf and wasm-allocator-arena.ttf.  All export
 * malloc() and free(), which the runtime then runs for the memory
 * Harfbuzz allocates in the module, in the middle of Harfbuzz's
 * functions.  Such a module is not run when the limits are compiled in;
 * test-wasm.c checks what happens when they are not.
 * generate-wasm-api-font.py compiles this once for each. */

#define HB_WASM_INTERFACE(ret_t, name) \
  __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

#ifdef GROW

/* Every allocation, and every free, enlarges the module's memory by
 * 4MiB or more.  A runtime without hardware bounds checks moves the
 * memory when it grows. */
extern "C" {

__attribute__((export_name("malloc")))
void *
malloc (unsigned long size)
{
  unsigned long pages = (size + 65535) / 65536 + 64;
  unsigned long start = __builtin_wasm_memory_grow (0, pages);
  if (start == (unsigned long) -1)
    return nullptr;
  return (void *) (start * 65536);
}

__attribute__((export_name("free")))
void
free (void *p)
{
  __builtin_wasm_memory_grow (0, 64);
}

}

/* Has Harfbuzz allocate in the module every way it can, and checks
 * what it wrote. */
bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  face_t *face = font_get_face (font);
  bool_t ok = true;

  blob_t blob = BLOB_INIT;
  ok &= face_copy_table (face, TAG ('m','a','x','p'), &blob);
  ok &= blob.length >= 6 && blob.data && blob.data[1] == 1;
  ok &= face_copy_table (face, TAG ('h','e','a','d'), &blob);
  ok &= blob.length == 54 && blob.data;

  buffer_contents_t contents = BUFFER_CONTENTS_INIT;
  ok &= buffer_copy_contents (buffer, &contents);
  ok &= contents.length == 3 && contents.info &&
	contents.info[0].codepoint == 'a';
  ok &= buffer_contents_realloc (&contents, 100);
  ok &= contents.length == 100 && contents.info[2].codepoint == 'c';

  glyph_outline_t outline = GLYPH_OUTLINE_INIT;
  ok &= font_copy_glyph_outline (font, font_get_glyph (font, 'a', 0), &outline);
  ok &= outline.n_points && outline.points &&
	outline.n_contours && outline.contours;

  coords_t coords = {0, 0};
  ok &= font_copy_coords (font, &coords);

  features_t copy = FEATURES_INIT;
  ok &= feature_copy_contents (features, num_features, &copy);
  ok &= copy.length == num_features;

  features_free (&copy);
  ok &= !copy.length && !copy.features;
  glyph_outline_free (&outline);
  ok &= !outline.n_points && !outline.points;
  blob_free (&blob);
  ok &= !blob.length && !blob.data;

  contents.length = 3;
  for (unsigned i = 0; i < 3; i++)
    contents.info[i].codepoint = font_get_glyph (font, contents.info[i].codepoint, 0);
  ok &= buffer_set_contents (buffer, &contents);
  buffer_contents_free (&contents);
  ok &= !contents.length && !contents.info && !contents.pos;

  return ok;
}

#endif

#ifdef REENTER

/* The allocator calls back into Harfbuzz, which is in the middle of
 * copying the buffer into the module: it makes the buffer a thousand
 * times longer, so that Harfbuzz would have to move it. */
static buffer_t *the_buffer;
static glyph_info_t infos[3000];
static glyph_position_t positions[3000];
static char heap[1 << 16];
static unsigned used;
static unsigned calls;

extern "C" {

__attribute__((export_name("malloc")))
void *
malloc (unsigned long size)
{
  if (the_buffer && !calls++)
  {
    buffer_contents_t longer = {3000, infos, positions};
    buffer_set_contents (the_buffer, &longer);
  }
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

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  the_buffer = buffer;
  buffer_contents_t contents = BUFFER_CONTENTS_INIT;
  if (!buffer_copy_contents (buffer, &contents))
    return false;
  contents.length = 3;
  return buffer_set_contents (buffer, &contents);
}

#endif

#ifdef ARENA

/* A small allocator, emptied at every call, which nothing is given
 * back to: the module frees none of what Harfbuzz copies for it. */
static char arena[1 << 18];
static unsigned used;
static unsigned calls;

extern "C" {

__attribute__((export_name("malloc")))
void *
malloc (unsigned long size)
{
  if (size > sizeof arena - used)
    return nullptr;
  void *p = arena + used;
  used += (size + 7) & ~7u;
  return p;
}

__attribute__((export_name("free")))
void
free (void *p)
{
}

}

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  used = 0;
  calls++;

  buffer_contents_t contents = BUFFER_CONTENTS_INIT;
  if (!buffer_copy_contents (buffer, &contents) || !contents.length)
    return false;

  for (unsigned i = 0; i < contents.length; i++)
    contents.info[i].codepoint = font_get_glyph (font, contents.info[i].codepoint, 0);

  glyph_outline_t outline = GLYPH_OUTLINE_INIT;
  font_copy_glyph_outline (font, contents.info[0].codepoint, &outline);

  /* Tells the test what it got, and which call of the instance this is. */
  contents.pos[0].x_offset = outline.n_points;
  contents.pos[0].y_offset = calls;
  return buffer_set_contents (buffer, &contents);
}

#endif
