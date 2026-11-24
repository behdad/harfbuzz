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

/* How many times this instance has been asked to shape. */
static unsigned calls;

/* The face, as shape_plan_create() got it. */
static face_t *plan_face;

void *
shape_plan_create (face_t *face)
{
  plan_face = face;
  return (void *) 1;
}

extern "C" __attribute__((export_name("shape_plan_destroy"))) void
shape_plan_destroy (void *shape_plan)
{
  /* This can run while the face is being destroyed. */
  font_create (plan_face);
}

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
  calls++;

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

    case 'd':
      /* The handles themselves. */
      values[0] = (uint32_t) (uintptr_t) font_get_face (font);
      values[1] = (uint32_t) (uintptr_t) plan_face;
      values[2] = (uint32_t) (uintptr_t) font;
      values[3] = (uint32_t) (uintptr_t) buffer;
      return answer (buffer, values, 4);

    case 'e':
    {
      /* More fonts than an instance may hold. */
      font_t *last = 0;
      values[0] = 0;
      for (unsigned i = 0; i < 1000; i++)
      {
	font_t *created = font_create (font_get_face (font));
	if (!created)
	  continue;
	values[0]++;
	last = created;
      }
      values[1] = face_get_upem (font_get_face (last));
      return answer (buffer, values, 2);
    }

    case 'f':
    {
      /* A font and a face of our own, destroyed before we return. */
      font_t *created = font_create (font_get_face (font));
      values[0] = calls;
      values[1] = font_get_glyph (created, 'a', 0);
      font_destroy (created);
      values[2] = font_get_glyph (created, 'a', 0);

      /* A face can go before a font that uses it; the font then gives
       * a new handle for it, which is ours to destroy as well. */
      blob_t blob = {sizeof (table), table};
      face_t *face = face_create (&blob, 0);
      font_t *on_it = font_create (face);
      face_destroy (face);
      values[3] = face_get_upem (face);
      face_t *again = font_get_face (on_it);
      values[4] = again != 0 && again != face && again == font_get_face (on_it);
      font_destroy (on_it);
      values[5] = face_get_upem (again);
      face_destroy (again);

      /* None of these is ours to destroy. */
      font_destroy (font);
      face_destroy (font_get_face (font));
      font_destroy ((font_t *) buffer);
      face_destroy ((face_t *) 0x7ffffff0);
      values[6] = font_get_glyph (font, 'a', 0);
      values[7] = face_get_upem (font_get_face (font));
      return answer (buffer, values, 8);
    }

    case 'g':
      /* A font of our own that we keep. */
      values[0] = calls;
      values[1] = font_create (font_get_face (font)) != 0;
      return answer (buffer, values, 2);
  }

  return false;
}
