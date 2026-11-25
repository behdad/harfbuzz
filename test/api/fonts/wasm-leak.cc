/* The Wasm modules of wasm-leak.ttf and of the fonts named after it,
 * which test-wasm.c shapes with.  generate-wasm-api-font.py compiles
 * them, each with what is said of it here defined.
 *
 * wasm-leak.ttf frees nothing of what Harfbuzz copies for it.  Modules
 * made with the Rust toolkit did not for years, and are in fonts.
 *
 * OWN (wasm-leak-own.ttf): the same, and has Harfbuzz free arrays of its
 * own besides.  With SPIN as well (wasm-leak-spin.ttf) it does not stop
 * when it is refused a copy, and runs on.
 *
 * KEEP (wasm-keep.ttf) frees all but a table, which it copies once and
 * holds on to, as a module may.  With WITHOUT as well
 * (wasm-keep-without.ttf) it does without the copy of the buffer when
 * it is refused one, and answers with one glyph.
 *
 * HOLD (wasm-hold.ttf) holds on to the buffer contents as well, from
 * call to call, and has them copied into the same arrays.
 *
 * ASK is KEEP, and asks at every call for two arrays of that many
 * items, which it does without: 200000 (wasm-ask.ttf), when not one of
 * them fits the room of an instance; 60000 (wasm-ask-two.ttf), when one
 * does and two do not.  With BOTH (wasm-ask-both.ttf) it asks for two
 * arrays of 30000 items twice: the room holds the first two, and the
 * module does without the others.
 *
 * FILL (wasm-fill.ttf) frees nothing, takes 1.2MB of the room for good
 * in the first call of an instance, and tells what features it got. */

#define HB_WASM_INTERFACE(ret_t, name) \
  __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

/* How many times this instance was called. */
static unsigned calls;

#ifdef HOLD
#define KEEP
static buffer_contents_t held = BUFFER_CONTENTS_INIT;
#endif

#ifdef ASK
#define KEEP
#endif

#ifdef SPIN
/* Runs until it is stopped. */
static void
spin ()
{
  volatile unsigned n = 0;
  for (;;)
    n = n + 1;
}
#endif

#ifdef FILL
static buffer_contents_t filler = BUFFER_CONTENTS_INIT;
#endif

#ifdef KEEP
static blob_t table = BLOB_INIT;
#endif

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  calls++;

  /* From here on the buffer holds glyphs. */
  if (!shape_with (font, buffer, features, num_features, "ot"))
    return false;

#ifdef HOLD
  buffer_contents_t &contents = held;
#else
  buffer_contents_t contents = BUFFER_CONTENTS_INIT;
#endif
  if (!buffer_copy_contents (buffer, &contents))
  {
#ifdef SPIN
    spin ();
#endif
#ifdef WITHOUT
    /* Hands back what part of the copy it got, and answers with one
     * glyph of its own, which tells which call of the instance this
     * is. */
    static glyph_info_t one_info[1];
    static glyph_position_t one_pos[1];
    buffer_contents_t one = {1, one_info, one_pos};
    buffer_contents_free (&contents);
    one_pos[0].y_offset = calls;
    return buffer_set_contents (buffer, &one);
#endif
    return false;
  }
  if (!contents.length)
    return false;

#ifdef ASK
#ifdef BOTH
  buffer_contents_t first = BUFFER_CONTENTS_INIT;
  if (!buffer_contents_realloc (&first, ASK))
    return false;
#endif
  /* Refused, and done without. */
  buffer_contents_t more = BUFFER_CONTENTS_INIT;
  if (buffer_contents_realloc (&more, ASK))
    return false;
#ifdef BOTH
  buffer_contents_free (&first);
#endif
#endif

  /* Like the toolkit, does not look at whether it got the outline. */
  glyph_outline_t outline = GLYPH_OUTLINE_INIT;
  font_copy_glyph_outline (font, contents.info[0].codepoint, &outline);
#ifdef SPIN
  if (!outline.n_points)
    spin ();
#endif

#ifdef KEEP
  if (!table.length &&
      !face_copy_table (font_get_face (font), TAG ('m','a','x','p'), &table))
    return false;
#endif

#ifdef FILL
  /* Two arrays of 600kB, if there is room for them. */
  if (!filler.length)
    buffer_contents_realloc (&filler, 30000);

  /* The value of the last feature. */
  contents.pos[0].y_advance = num_features ? features[num_features - 1].value : 0;
#endif

  /* Tells the test what it got, and which call of the instance this is. */
  contents.pos[0].x_offset = outline.n_points;
  contents.pos[0].y_offset = calls;
  bool_t ret = buffer_set_contents (buffer, &contents);

#ifdef KEEP
  glyph_outline_free (&outline);
#ifndef HOLD
  buffer_contents_free (&contents);
#endif
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
