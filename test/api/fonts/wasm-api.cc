/* The module in wasm-api.ttf, which test-wasm.c shapes with.
 * generate-wasm-api-font.py builds the font from this file.
 *
 * The first character of the text selects what the module does.  It
 * answers by setting the buffer to a few values, one per glyph. */

#define HB_WASM_INTERFACE(ret_t, name) __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

#define CAPACITY 64

static glyph_info_t infos[CAPACITY];
static glyph_position_t positions[CAPACITY];
static char table[256];
static int coords[CAPACITY];
static glyph_outline_point_t points[256];
static uint32_t contours[16];

/* The buffer of the previous call. */
static buffer_t *last_buffer;

static bool_t
answer (buffer_t *buffer, const uint32_t *values, unsigned count)
{
  for (unsigned i = 0; i < count; i++)
  {
    infos[i].codepoint = values[i];
    infos[i].mask = 0;
    infos[i].cluster = 0;
    infos[i].var1 = infos[i].var2 = 0;
    positions[i].x_advance = positions[i].y_advance = 0;
    positions[i].x_offset = positions[i].y_offset = 0;
    positions[i].var = 0;
  }
  buffer_contents_t contents = {count, infos, positions};
  return buffer_set_contents (buffer, &contents);
}

bool_t
shape (void *shape_plan,
       font_t *font,
       buffer_t *buffer,
       const feature_t *features,
       uint32_t num_features)
{
  buffer_t *previous = last_buffer;
  last_buffer = buffer;

  uint32_t values[8];

  /* Each array passed to a copy function here is larger than what
   * goes into it. */
  buffer_contents_t contents = {CAPACITY, infos, positions};
  if (!buffer_copy_contents (buffer, &contents))
    return false;

  switch (infos[0].codepoint)
  {
    case 'a':
    {
      /* The lengths that the copy functions report. */
      blob_t blob = {sizeof (table), table};
      coords_t location = {CAPACITY, coords};
      glyph_outline_t outline = {256, points, 16, contours};
      codepoint_t a = font_get_glyph (font, 'a', 0);
      codepoint_t b = font_get_glyph (font, 'b', 0);
      if (!face_copy_table (font_get_face (font), TAG ('m','a','x','p'), &blob) ||
	  !font_copy_coords (font, &location) ||
	  !font_copy_glyph_outline (font, a, &outline))
	return false;
      values[0] = contents.length;
      values[1] = blob.length;
      values[2] = location.length;
      values[3] = outline.n_points;
      values[4] = outline.n_contours;

      /* One structure for outlines of different sizes, in arrays that
       * the host allocates. */
      glyph_outline_t reused = GLYPH_OUTLINE_INIT;
      values[5] = 0;
      for (unsigned i = 0; i < 20000; i++)
	if (!font_copy_glyph_outline (font, i & 1 ? a : b, &reused))
	  values[5]++;
      glyph_outline_free (&reused);

      return answer (buffer, values, 6);
    }

    case 'b':
      /* What the handle of the previous call's buffer names now. */
      values[0] = previous ? buffer_get_direction (previous) : (uint32_t) -1;
      return answer (buffer, values, 1);

    case 'c':
    {
      /* Faces and fonts of our own, and a handle that names nothing. */
      font_t *second = font_create (font_get_face (font));
      blob_t blob = {sizeof (table), table};
      font_t *third = font_create (face_create (&blob, 0));
      values[0] = shape_with (font, (buffer_t *) 0x7ffffff0, 0, 0, "ot");
      values[1] = face_get_upem (font_get_face (second));
      values[2] = third != 0;
      return answer (buffer, values, 3);
    }
  }

  return false;
}
