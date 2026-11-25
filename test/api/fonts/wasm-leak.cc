/* The Wasm modules of wasm-leak.ttf, wasm-leak-own.ttf and
 * wasm-keep.ttf, which test-wasm.c shapes with.
 * generate-wasm-api-font.py compiles them.
 *
 * The first frees nothing of what Harfbuzz copies for it.  Modules made
 * with the Rust toolkit did not for years, and are in fonts.  The second
 * does the same and has Harfbuzz free arrays of its own besides.  The
 * third frees all but a table, which it copies once and holds on to, as
 * a module may. */

#define HB_WASM_INTERFACE(ret_t, name) \
  __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

/* How many times this instance was called. */
static unsigned calls;

#ifdef KEEP
static blob_t table = BLOB_INIT;
#endif

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  calls++;

  /* From here on the buffer holds glyphs, and a call that fails is not
   * made again in another instance. */
  if (!shape_with (font, buffer, features, num_features, "ot"))
    return false;

  buffer_contents_t contents = BUFFER_CONTENTS_INIT;
  if (!buffer_copy_contents (buffer, &contents))
    return false;
  if (!contents.length)
    return false;

  /* Like the toolkit, does not look at whether it got the outline. */
  glyph_outline_t outline = GLYPH_OUTLINE_INIT;
  font_copy_glyph_outline (font, contents.info[0].codepoint, &outline);

#ifdef KEEP
  if (!table.length &&
      !face_copy_table (font_get_face (font), TAG ('m','a','x','p'), &table))
    return false;
#endif

  /* Tells the test what it got, and which call of the instance this is. */
  contents.pos[0].x_offset = outline.n_points;
  contents.pos[0].y_offset = calls;
  bool_t ret = buffer_set_contents (buffer, &contents);

#ifdef KEEP
  glyph_outline_free (&outline);
  buffer_contents_free (&contents);
#endif

#ifdef OWN
  /* As many arrays of our own for Harfbuzz to free as it allocated for
   * us in this call. */
  static glyph_info_t own_info[1];
  static glyph_position_t own_pos[1];
  for (unsigned i = 0; i < 2; i++)
  {
    buffer_contents_t own = {1, own_info, own_pos};
    buffer_contents_free (&own);
  }
#endif

  return ret;
}
