/*
 * Copyright © 2026  Behdad Esfahbod
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include "hb-test.h"

/* The module in fonts/wasm-api.ttf answers with a few values, one per
 * glyph.  Its source is fonts/wasm-api.cc. */

static hb_font_t *
open_font (void)
{
  hb_face_t *face = hb_test_open_font_file ("fonts/wasm-api.ttf");
  hb_font_t *font = hb_font_create (face);
  hb_face_destroy (face);
  return font;
}

static hb_buffer_t *
shape_text (hb_font_t *font, const char *text)
{
  const char *shapers[] = {"wasm", NULL};
  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, text, -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));
  return buffer;
}

static unsigned
value (hb_buffer_t *buffer, unsigned i)
{
  unsigned length;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &length);
  g_assert_cmpuint (i, <, length);
  return info[i].codepoint;
}


/* Count points and contours the way the outline handed to a module does. */

typedef struct {
  unsigned points;
  unsigned contours;
} outline_size_t;

static void
count_one (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
	   hb_draw_state_t *st HB_UNUSED,
	   float to_x HB_UNUSED, float to_y HB_UNUSED,
	   void *user_data HB_UNUSED)
{ ((outline_size_t *) draw_data)->points += 1; }

static void
count_quadratic (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
		 hb_draw_state_t *st HB_UNUSED,
		 float control_x HB_UNUSED, float control_y HB_UNUSED,
		 float to_x HB_UNUSED, float to_y HB_UNUSED,
		 void *user_data HB_UNUSED)
{ ((outline_size_t *) draw_data)->points += 2; }

static void
count_cubic (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
	     hb_draw_state_t *st HB_UNUSED,
	     float control1_x HB_UNUSED, float control1_y HB_UNUSED,
	     float control2_x HB_UNUSED, float control2_y HB_UNUSED,
	     float to_x HB_UNUSED, float to_y HB_UNUSED,
	     void *user_data HB_UNUSED)
{ ((outline_size_t *) draw_data)->points += 3; }

static void
count_close (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
	     hb_draw_state_t *st HB_UNUSED,
	     void *user_data HB_UNUSED)
{ ((outline_size_t *) draw_data)->contours += 1; }

static outline_size_t
outline_size (hb_font_t *font, hb_codepoint_t unicode)
{
  outline_size_t size = {0, 0};
  hb_codepoint_t glyph = 0;
  hb_draw_funcs_t *funcs = hb_draw_funcs_create ();
  hb_draw_funcs_set_move_to_func (funcs, count_one, NULL, NULL);
  hb_draw_funcs_set_line_to_func (funcs, count_one, NULL, NULL);
  hb_draw_funcs_set_quadratic_to_func (funcs, count_quadratic, NULL, NULL);
  hb_draw_funcs_set_cubic_to_func (funcs, count_cubic, NULL, NULL);
  hb_draw_funcs_set_close_path_func (funcs, count_close, NULL, NULL);
  g_assert_true (hb_font_get_nominal_glyph (font, unicode, &glyph));
  hb_font_draw_glyph (font, glyph, funcs, &size);
  hb_draw_funcs_destroy (funcs);
  return size;
}

static void
test_wasm_copy_lengths (void)
{
  hb_font_t *font = open_font ();
  hb_blob_t *maxp = hb_face_reference_table (hb_font_get_face (font),
					     HB_TAG ('m','a','x','p'));
  outline_size_t size = outline_size (font, 'a');
  hb_buffer_t *buffer = shape_text (font, "abc");

  /* The module passed arrays larger than needed, and reads back how
   * much was copied into each. */
  g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 6);
  g_assert_cmpuint (value (buffer, 0), ==, 3);
  g_assert_cmpuint (value (buffer, 1), ==, hb_blob_get_length (maxp));
  g_assert_cmpuint (value (buffer, 2), ==, 0);
  g_assert_cmpuint (size.points, >, 0);
  g_assert_cmpuint (value (buffer, 3), ==, size.points);
  g_assert_cmpuint (value (buffer, 4), ==, size.contours);

  /* It then reused one outline structure for two glyphs in turn, and
   * counted the copies that failed. */
  g_assert_cmpuint (value (buffer, 5), ==, 0);

  hb_buffer_destroy (buffer);
  hb_blob_destroy (maxp);
  hb_font_destroy (font);
}

static void
test_wasm_handles_end_with_the_call (void)
{
  hb_font_t *font = open_font ();
  hb_buffer_t *first, *second;

  first = shape_text (font, "b");
  g_assert_cmpuint (value (first, 0), ==, (unsigned) -1);

  /* The module passes the handle it had for the first buffer.  That
   * buffer is still alive and has a direction; the handle is not to
   * name it any more. */
  second = shape_text (font, "b");
  g_assert_cmpuint (value (second, 0), ==, HB_DIRECTION_INVALID);

  hb_buffer_destroy (first);
  hb_buffer_destroy (second);
  hb_font_destroy (font);
}

static int face_destroyed;

static void
note_face_destroyed (void *data HB_UNUSED)
{ face_destroyed++; }

static void
test_wasm_created_objects (void)
{
  static hb_user_data_key_t key;
  hb_face_t *face = hb_test_open_font_file ("fonts/wasm-api.ttf");
  hb_font_t *font = hb_font_create (face);
  hb_buffer_t *buffer;

  hb_face_set_user_data (face, &key, &key, note_face_destroyed, TRUE);
  hb_face_destroy (face);

  /* The module creates a font on our face, and a face and a font of
   * its own.  It also passes shape_with() a handle that names no
   * buffer. */
  buffer = shape_text (font, "c");
  g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 3);
  g_assert_cmpuint (value (buffer, 0), ==, 0);
  g_assert_cmpuint (value (buffer, 1), ==, hb_face_get_upem (hb_font_get_face (font)));
  g_assert_cmpuint (value (buffer, 2), ==, 1);
  hb_buffer_destroy (buffer);

  /* The handle resolved to the empty buffer, which is to stay as it was. */
  g_assert_cmpint (hb_buffer_get_content_type (hb_buffer_get_empty ()), ==,
		   HB_BUFFER_CONTENT_TYPE_INVALID);

  /* Nothing the module created keeps our face alive. */
  g_assert_cmpint (face_destroyed, ==, 0);
  hb_font_destroy (font);
  g_assert_cmpint (face_destroyed, ==, 1);
}

static void
test_wasm_handles_belong_to_the_instance (void)
{
  hb_font_t *font1 = open_font ();
  hb_font_t *font2 = open_font ();
  hb_buffer_t *buffer1, *buffer2, *buffer3;

  /* Two faces, so two instances of the module.  Each numbers its
   * handles by itself, so both see the same numbers, which name
   * different objects. */
  buffer1 = shape_text (font1, "d");
  buffer2 = shape_text (font2, "d");
  g_assert_cmpuint (hb_buffer_get_length (buffer1), ==, 4);
  for (unsigned i = 0; i < 4; i++)
  {
    g_assert_cmpuint (value (buffer1, i), !=, 0);
    g_assert_cmpuint (value (buffer1, i), ==, value (buffer2, i));
  }

  /* The face keeps its handle from shape_plan_create() on... */
  g_assert_cmpuint (value (buffer1, 0), ==, value (buffer1, 1));
  buffer3 = shape_text (font1, "d");
  g_assert_cmpuint (value (buffer3, 0), ==, value (buffer1, 0));
  /* ...while the font and the buffer get new ones in every call. */
  g_assert_cmpuint (value (buffer3, 2), !=, value (buffer1, 2));
  g_assert_cmpuint (value (buffer3, 3), !=, value (buffer1, 3));
  g_assert_cmpuint (value (buffer3, 2), !=, value (buffer1, 3));

  hb_buffer_destroy (buffer1);
  hb_buffer_destroy (buffer2);
  hb_buffer_destroy (buffer3);
  hb_font_destroy (font1);
  hb_font_destroy (font2);
}

static void
test_wasm_handle_limit (void)
{
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer = shape_text (font, "e");

  /* The module asked for a thousand fonts.  It got some, was refused
   * the rest, and the ones it got work. */
  g_assert_cmpuint (value (buffer, 0), >=, 16);
  g_assert_cmpuint (value (buffer, 0), <, 1000);
  g_assert_cmpuint (value (buffer, 1), ==, hb_face_get_upem (hb_font_get_face (font)));

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}

static void
test_wasm_destroy (void)
{
  hb_font_t *font = open_font ();
  hb_codepoint_t a = 0;
  unsigned upem = hb_face_get_upem (hb_font_get_face (font));
  unsigned empty_upem = hb_face_get_upem (hb_face_get_empty ());

  g_assert_true (hb_font_get_nominal_glyph (font, 'a', &a));
  g_assert_cmpuint (upem, !=, empty_upem);

  for (unsigned call = 1; call <= 3; call++)
  {
    hb_buffer_t *buffer = shape_text (font, "f");
    g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 8);

    /* The module destroyed all it created, so its instance is kept:
     * it has counted every call so far. */
    g_assert_cmpuint (value (buffer, 0), ==, call);

    /* Its font worked, and the handle named nothing once destroyed. */
    g_assert_cmpuint (value (buffer, 1), ==, a);
    g_assert_cmpuint (value (buffer, 2), ==, 0);

    /* So for its face.  The font on that face still had it, under a
     * new handle, and kept it alive until that handle went too. */
    g_assert_cmpuint (value (buffer, 3), ==, empty_upem);
    g_assert_cmpuint (value (buffer, 4), ==, 1);
    g_assert_cmpuint (value (buffer, 5), !=, 0);

    /* What it was given is not its to destroy. */
    g_assert_cmpuint (value (buffer, 6), ==, a);
    g_assert_cmpuint (value (buffer, 7), ==, upem);

    hb_buffer_destroy (buffer);
  }

  hb_font_destroy (font);

  /* A module that keeps a font gets a new instance for each call. */
  font = open_font ();
  for (unsigned call = 1; call <= 3; call++)
  {
    hb_buffer_t *buffer = shape_text (font, "g");
    g_assert_cmpuint (value (buffer, 0), ==, 1);
    g_assert_cmpuint (value (buffer, 1), ==, 1);
    hb_buffer_destroy (buffer);
  }
  hb_font_destroy (font);
}

static void
test_wasm_start_function (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_face_t *face = hb_test_open_font_file ("fonts/wasm-start.ttf");
  hb_font_t *font = hb_font_create (face);
  hb_buffer_t *buffer = hb_buffer_create ();

  /* The module called into us while its instance was being made, and
   * got nothing for it. */
  hb_buffer_add_utf8 (buffer, "abc", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
#ifdef HB_TEST_WASM_BUDGET
  /* With the limits, a module that the runtime runs by itself is not
   * run at all. */
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
#else
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));
#endif

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
  hb_face_destroy (face);
}

static hb_bool_t
shapes (const char *font_file)
{
  const char *shapers[] = {"wasm", NULL};
  hb_face_t *face = hb_test_open_font_file (font_file);
  hb_font_t *font = hb_font_create (face);
  hb_buffer_t *buffer = hb_buffer_create ();
  hb_bool_t ret;

  hb_buffer_add_utf8 (buffer, "abc", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  ret = hb_shape_full (font, buffer, NULL, 0, shapers);

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
  hb_face_destroy (face);
  return ret;
}

static void
test_wasm_refused_modules (void)
{
  /* A module may import a function that nothing provides, as long as
   * it does not call it.  Modules made with wasm-bindgen do. */
  g_assert_true (shapes ("fonts/wasm-import.ttf"));

  /* Each of these modules has the runtime run something that we cannot
   * limit: a function of the runtime's own C library, the module's own
   * allocator, a start function.  With the limits they are not run.
   * A runtime built without a C library does not provide the first,
   * and the module's call to it fails. */
#ifdef HB_TEST_WASM_BUDGET
  g_assert_false (shapes ("fonts/wasm-import-libc.ttf"));
  g_assert_false (shapes ("fonts/wasm-allocator.ttf"));
  g_assert_false (shapes ("fonts/wasm-start-section.ttf"));
#else
  g_assert_true (shapes ("fonts/wasm-allocator.ttf"));
  g_assert_true (shapes ("fonts/wasm-start-section.ttf"));
#endif
}

static void
test_wasm_instance_kept (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_face_t *face = hb_test_open_font_file ("fonts/wasm-fixed-memory.ttf");
  hb_font_t *font = hb_font_create (face);
  hb_buffer_t *buffer = hb_buffer_create ();
  char text[2001];

  hb_buffer_add_utf8 (buffer, "abc", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));

  /* The module's memory cannot grow past what we allow it, so a longer
   * buffer goes to the instance it already has.  The module checks. */
  memset (text, 'a', sizeof (text) - 1);
  text[sizeof (text) - 1] = '\0';
  hb_buffer_reset (buffer);
  hb_buffer_add_utf8 (buffer, text, -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
  hb_face_destroy (face);
}

static void
test_wasm_module_allocator (void)
{
  /* These modules export malloc() and free(), which the runtime then
   * runs for what we allocate in the module. */
#ifdef HB_TEST_WASM_BUDGET
  g_assert_false (shapes ("fonts/wasm-allocator-grow.ttf"));
  g_assert_false (shapes ("fonts/wasm-allocator-reenter.ttf"));
#else
  /* One enlarges the module's memory at every allocation, which can
   * move it, and checks what we wrote into it after. */
  g_assert_true (shapes ("fonts/wasm-allocator-grow.ttf"));

  /* The other calls back into us from its allocator, in the middle of
   * our copying the buffer, and is stopped. */
  g_assert_false (shapes ("fonts/wasm-allocator-reenter.ttf"));
#endif
}

static int advances_asked;

static hb_position_t
count_h_advance (hb_font_t *font, void *font_data HB_UNUSED,
		 hb_codepoint_t glyph, void *user_data HB_UNUSED)
{
  advances_asked++;
  return hb_font_get_glyph_h_advance (hb_font_get_parent (font), glyph);
}

static void
test_wasm_failed_call (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *parent = open_font ();
  hb_font_t *font = hb_font_create_sub_font (parent);
  hb_font_funcs_t *funcs = hb_font_funcs_create ();
  hb_buffer_t *buffer;

  hb_font_funcs_set_glyph_h_advance_func (funcs, count_h_advance, NULL, NULL);
  hb_font_set_funcs (font, funcs, NULL, NULL);
  hb_font_funcs_destroy (funcs);
  advances_asked = 0;

  /* The module asks for an advance and fails, with the buffer as it
   * came.  Its instance is new, and a new one would only fail again:
   * the module is called once. */
  buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, "l", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_assert_cmpint (advances_asked, ==, 1);
  hb_buffer_destroy (buffer);

  /* The next call is to a new instance, which is then kept: the module
   * counts the calls to its instance. */
  buffer = shape_text (font, "f");
  g_assert_cmpuint (value (buffer, 0), ==, 1);
  hb_buffer_destroy (buffer);
  buffer = shape_text (font, "f");
  g_assert_cmpuint (value (buffer, 0), ==, 2);
  hb_buffer_destroy (buffer);

  /* A call that fails in an instance that has served others is made
   * once more, in a new instance. */
  buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, "l", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_assert_cmpint (advances_asked, ==, 3);
  hb_buffer_destroy (buffer);

  /* Neither of the two is kept. */
  buffer = shape_text (font, "f");
  g_assert_cmpuint (value (buffer, 0), ==, 1);
  hb_buffer_destroy (buffer);

  hb_font_destroy (font);
  hb_font_destroy (parent);
}

static void
test_wasm_coords_free (void)
{
  static const int coords[2] = {1 << 14, -(1 << 13)};
  hb_face_t *face = hb_test_open_font_file ("fonts/wasm-coords.ttf");
  hb_font_t *font = hb_font_create (face);
  hb_buffer_t *buffer;

  hb_face_destroy (face);
  hb_font_set_var_coords_normalized (font, coords, 2);

  /* The module had the font's coordinates copied and freed them, many
   * times over, and counted what went wrong. */
  buffer = shape_text (font, "m");
  g_assert_cmpuint (value (buffer, 0), ==, 0);
  g_assert_cmpuint (value (buffer, 1), ==, 2);

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}

/* Opens a font that counts, in advances_asked, the advances asked of
 * it: shaping a text asks for as many each time it is done. */
static hb_font_t *
open_counting_font (const char *font_file)
{
  hb_face_t *face = hb_test_open_font_file (font_file);
  hb_font_t *parent = hb_font_create (face);
  hb_font_t *font = hb_font_create_sub_font (parent);
  hb_font_funcs_t *funcs = hb_font_funcs_create ();

  hb_font_funcs_set_glyph_h_advance_func (funcs, count_h_advance, NULL, NULL);
  hb_font_set_funcs (font, funcs, NULL, NULL);
  hb_font_funcs_destroy (funcs);
  hb_font_destroy (parent);
  hb_face_destroy (face);
  return font;
}

/* Shapes the same text many times with one font.  Returns how many
 * instances of the module that took; the most calls that one of them
 * got; and how many times the module shaped the text, which is more
 * than the calls we made if some were done over. */
static unsigned
shape_repeatedly (const char *font_file, unsigned length, unsigned calls,
		  unsigned *most_calls, unsigned *tries)
{
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *font = open_counting_font (font_file);
  hb_buffer_t *buffer = hb_buffer_create ();
  outline_size_t size = outline_size (hb_font_get_parent (font), 'a');
  char *text = (char *) g_malloc (length + 1);
  unsigned instances = 0;
  int per_try = 0;

  g_assert_cmpuint (size.points, >, 0);
  memset (text, 'a', length);
  text[length] = '\0';
  *most_calls = 0;
  advances_asked = 0;

  for (unsigned call = 0; call < calls; call++)
  {
    hb_glyph_position_t *pos;
    unsigned glyphs;

    hb_buffer_reset (buffer);
    hb_buffer_add_utf8 (buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties (buffer);
    g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));

    /* The module got the outline it asked for, every time.  It also
     * tells which call of its instance this was. */
    pos = hb_buffer_get_glyph_positions (buffer, &glyphs);
    g_assert_cmpuint (glyphs, ==, length);
    g_assert_cmpuint (pos[0].x_offset, ==, size.points);
    instances += pos[0].y_offset == 1;
    *most_calls = MAX (*most_calls, (unsigned) pos[0].y_offset);

    /* The first call is to a new instance, and is not done over. */
    if (!call)
      per_try = advances_asked;
  }
  *tries = per_try ? (unsigned) (advances_asked / per_try) : calls;

  g_free (text);
  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
  return instances;
}

static hb_buffer_t *
shape_as (hb_font_t *font, unsigned length)
{
  const char *shapers[] = {"wasm", NULL};
  hb_buffer_t *buffer = hb_buffer_create ();
  char *text = (char *) g_malloc (length + 1);

  memset (text, 'a', length);
  text[length] = '\0';
  hb_buffer_add_utf8 (buffer, text, -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_free (text);
  return buffer;
}

static void
test_wasm_module_that_frees_nothing (void)
{
  unsigned instances, most_calls, tries;
  hb_font_t *font;

  /* What we copy for a module goes into room that its instance has for
   * that, and this module leaves it all there.  It is to work all the
   * same: when a copy finds no room, the call is done over in a new
   * instance.  And not in a new one for every call. */
  instances = shape_repeatedly ("fonts/wasm-leak.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, >, 1);
  g_assert_cmpuint (instances, <, 50);
  g_assert_cmpuint (most_calls, >, 4);

  /* Each instance but the first began with a call that was done over. */
  g_assert_cmpuint (tries, ==, 200 + instances - 1);

  /* With a text of one character it is the outline that finds no room
   * first, and the module does not look at whether it got one: the
   * call succeeds, and is done over all the same. */
  instances = shape_repeatedly ("fonts/wasm-leak.ttf", 1, 8000, &most_calls, &tries);
  g_assert_cmpuint (instances, >, 1);
  g_assert_cmpuint (instances, <, 50);
  g_assert_cmpuint (tries, ==, 8000 + instances - 1);

  /* A module that has us free arrays of its own as well, as many as we
   * allocated for it. */
  instances = shape_repeatedly ("fonts/wasm-leak-own.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, >, 1);
  g_assert_cmpuint (instances, <, 50);
  g_assert_cmpuint (tries, ==, 200 + instances - 1);

  instances = shape_repeatedly ("fonts/wasm-leak-own.ttf", 2, 8000, &most_calls, &tries);
  g_assert_cmpuint (instances, >, 1);
  g_assert_cmpuint (instances, <, 50);
  g_assert_cmpuint (tries, ==, 8000 + instances - 1);

  /* A module that holds on to something of ours keeps its instance:
   * a table... */
  instances = shape_repeatedly ("fonts/wasm-keep.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 1);
  g_assert_cmpuint (most_calls, ==, 200);
  g_assert_cmpuint (tries, ==, 200);

  /* ...or most of the room there is. */
  instances = shape_repeatedly ("fonts/wasm-hold.ttf", 30000, 20, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 1);
  g_assert_cmpuint (most_calls, ==, 20);
  g_assert_cmpuint (tries, ==, 20);

  /* So does one that asks at every call for more than an instance can
   * ever hold, and does without: two arrays of which not one fits the
   * room... */
  instances = shape_repeatedly ("fonts/wasm-ask.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 1);
  g_assert_cmpuint (tries, ==, 200);

  /* ...or of which one does, and two do not. */
  instances = shape_repeatedly ("fonts/wasm-ask-two.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 1);
  g_assert_cmpuint (tries, ==, 200);

  /* The copy of a buffer is two such arrays.  For a text of 60,000
   * characters the room holds one of them and not both, and this
   * module does without the copy: it answers with one glyph, which
   * tells which call of its instance that was. */
  font = open_counting_font ("fonts/wasm-keep-without.ttf");
  hb_buffer_destroy (shape_as (font, 2000));
  for (int call = 2; call < 8; call++)
  {
    hb_buffer_t *buffer = shape_as (font, 60000);
    g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 1);
    g_assert_cmpint (hb_buffer_get_glyph_positions (buffer, NULL)[0].y_offset, ==, call);
    hb_buffer_destroy (buffer);
  }
  hb_font_destroy (font);

  /* Not one that asks twice for arrays that the room holds once, and
   * does without the second two: for all we know it left the first
   * two where they are.  An instance in which a copy found no room is
   * not kept, and the next call is made in a new one, once. */
  instances = shape_repeatedly ("fonts/wasm-ask-both.ttf", 2000, 200, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 200);
  g_assert_cmpuint (tries, ==, 200);

#ifdef HB_TEST_WASM_BUDGET
  /* A module that frees nothing and, when it is refused a copy, runs on
   * until it is stopped.  That call is done over too: it is for want
   * of the copy that it ran out of instructions. */
  instances = shape_repeatedly ("fonts/wasm-leak-spin.ttf", 1, 4000, &most_calls, &tries);
  g_assert_cmpuint (instances, >, 1);
  g_assert_cmpuint (instances, <, 50);
  g_assert_cmpuint (tries, ==, 4000 + instances - 1);
#endif

#ifndef HB_TEST_WASM_BUDGET
  /* Without the limits a module may have an allocator of its own, which
   * the runtime then runs for what we copy.  This one's is small, is
   * emptied at every call, and gets nothing back.  Its instance is kept
   * too, for more than we could have copied into the room that we asked
   * the runtime for. */
  instances = shape_repeatedly ("fonts/wasm-allocator-arena.ttf", 2000, 100, &most_calls, &tries);
  g_assert_cmpuint (instances, ==, 1);
  g_assert_cmpuint (most_calls, ==, 100);
#endif
}

static void
assert_same_glyphs (hb_buffer_t *buffer, hb_buffer_t *buffer2)
{
  unsigned length, length2;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &length);
  hb_glyph_info_t *info2 = hb_buffer_get_glyph_infos (buffer2, &length2);

  g_assert_cmpuint (length, ==, length2);
  for (unsigned i = 0; i < length; i++)
  {
    g_assert_cmpuint (info[i].codepoint, ==, info2[i].codepoint);
    g_assert_cmpuint (info[i].cluster, ==, info2[i].cluster);
  }
  g_assert_cmpmem (hb_buffer_get_glyph_positions (buffer, NULL),
		   length * sizeof (hb_glyph_position_t),
		   hb_buffer_get_glyph_positions (buffer2, NULL),
		   length2 * sizeof (hb_glyph_position_t));
}

/* Shapes a long text after a number of short ones, and the same text
 * with another font of the same file.  Returns how many times the
 * module shaped the long text after the short ones. */
static unsigned
long_text_after_short (const char *font_file, unsigned short_calls)
{
  hb_font_t *font = open_counting_font (font_file);
  hb_font_t *font2 = open_counting_font (font_file);
  hb_buffer_t *after, *alone;
  int asked, asked2;

  for (unsigned call = 0; call < short_calls; call++)
    hb_buffer_destroy (shape_as (font, 2000));

  advances_asked = 0;
  after = shape_as (font, 30000);
  asked = advances_asked;
  advances_asked = 0;
  alone = shape_as (font2, 30000);
  asked2 = advances_asked;
  g_assert_cmpint (asked2, >, 0);
  g_assert_cmpint (asked % asked2, ==, 0);

  /* It comes out as in a new instance. */
  g_assert_cmpuint (hb_buffer_get_length (alone), ==, 30000);
  assert_same_glyphs (after, alone);

  hb_buffer_destroy (after);
  hb_buffer_destroy (alone);
  hb_font_destroy (font);
  hb_font_destroy (font2);
  return asked / asked2;
}

static void
test_wasm_long_text_after_short (void)
{
  /* The module frees nothing.  After thirteen calls its instance has
   * room for more of the same, but not for a text that a new instance
   * has room for.  The module has shaped the buffer by the time it
   * finds out; the call is done over all the same, so the text is
   * shaped twice. */
  g_assert_cmpuint (long_text_after_short ("fonts/wasm-leak.ttf", 13), ==, 2);
  g_assert_cmpuint (long_text_after_short ("fonts/wasm-leak-own.ttf", 13), ==, 2);
}

static void
test_wasm_features_without_room (void)
{
  enum { NUM_FEATURES = 90000 };
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *font = open_counting_font ("fonts/wasm-fill.ttf");
  hb_feature_t *features = (hb_feature_t *) g_malloc (NUM_FEATURES * sizeof (hb_feature_t));
  hb_buffer_t *buffer = hb_buffer_create ();
  int asked;

  for (unsigned i = 0; i < NUM_FEATURES; i++)
  {
    features[i].tag = HB_TAG ('t','e','s','t');
    features[i].value = 7;
    features[i].start = HB_FEATURE_GLOBAL_START;
    features[i].end = HB_FEATURE_GLOBAL_END;
  }

  /* In the first call of an instance the module takes 1.2MB of the
   * room, and holds on to them.  It tells the value of the last feature
   * it was given: none here. */
  advances_asked = 0;
  hb_buffer_add_utf8 (buffer, "a", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_assert_cmpint (hb_buffer_get_glyph_positions (buffer, NULL)[0].y_advance, ==, 0);
  asked = advances_asked;
  g_assert_cmpint (asked, >, 0);

  /* What is left has no room for 1.4MB of features.  The module is not
   * called with features that it cannot read: the call is made in a
   * new instance, where they fit, and nowhere else. */
  advances_asked = 0;
  hb_buffer_reset (buffer);
  hb_buffer_add_utf8 (buffer, "a", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_true (hb_shape_full (font, buffer, features, NUM_FEATURES, shapers));
  g_assert_cmpint (hb_buffer_get_glyph_positions (buffer, NULL)[0].y_advance, ==, 7);
  g_assert_cmpint (hb_buffer_get_glyph_positions (buffer, NULL)[0].y_offset, ==, 1);
  g_assert_cmpint (advances_asked, ==, asked);

  hb_buffer_destroy (buffer);
  g_free (features);
  hb_font_destroy (font);
}

static void
test_wasm_buffer_as_it_came (void)
{
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer;

  /* The module counts the calls to its instance. */
  buffer = shape_text (font, "f");
  g_assert_cmpuint (value (buffer, 0), ==, 1);
  hb_buffer_destroy (buffer);

  /* In that instance it changes the buffer, leaves a position in it,
   * and then fails.  The call is done over in a new instance, which
   * tells what it was given: the text, not what the first try left,
   * and no position. */
  buffer = shape_text (font, "nxy");
  g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 8);
  g_assert_cmpuint (value (buffer, 7), ==, 0);
  g_assert_cmpuint (value (buffer, 0), ==, 3);
  g_assert_cmpuint (value (buffer, 1), ==, 'n');
  g_assert_cmpuint (value (buffer, 2), ==, 0);
  g_assert_cmpuint (value (buffer, 3), ==, 'x');
  g_assert_cmpuint (value (buffer, 4), ==, 1);
  g_assert_cmpuint (value (buffer, 5), ==, 'y');
  g_assert_cmpuint (value (buffer, 6), ==, 2);
  hb_buffer_destroy (buffer);

  hb_font_destroy (font);
}

static void
test_wasm_positions_of_the_caller (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer = hb_buffer_create ();
  hb_glyph_position_t *pos;

  /* A caller that wrote positions into the buffer before shaping.  The
   * module is given none: a call that is done over could not be given
   * them again. */
  hb_buffer_add_utf8 (buffer, "o", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  pos = hb_buffer_get_glyph_positions (buffer, NULL);
  pos[0].x_advance = 1234;
  pos[0].y_offset = 5678;
  g_assert_true (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_assert_cmpuint (value (buffer, 0), ==, 0);
  g_assert_cmpuint (value (buffer, 1), ==, 0);

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}


#ifdef HB_TEST_WASM_BUDGET

static void
test_wasm_memory_limit (void)
{
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer = shape_text (font, "h");

  /* The module asked for 256MiB in steps and was refused before that. */
  g_assert_cmpuint (value (buffer, 1), >, 0);
  g_assert_cmpuint (value (buffer, 1), <, 4096);
  hb_buffer_destroy (buffer);

  /* It made faces of 4MiB of data until it was refused that too. */
  buffer = shape_text (font, "i");
  g_assert_cmpuint (value (buffer, 0), >, 0);
  g_assert_cmpuint (value (buffer, 0), <, 64);
  hb_buffer_destroy (buffer);

  hb_font_destroy (font);
}

static void
test_wasm_instruction_limit (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer = hb_buffer_create ();

  /* The module runs more than it may, and is stopped. */
  hb_buffer_add_utf8 (buffer, "j", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
  hb_buffer_destroy (buffer);

  /* The font works again right after. */
  buffer = shape_text (font, "abc");
  g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 6);
  hb_buffer_destroy (buffer);
  hb_font_destroy (font);

  /* In an instance that has served a call, too, the module is stopped
   * once: a call that ran out of instructions is not made again.  The
   * module asks for an advance before it starts. */
  font = open_counting_font ("fonts/wasm-api.ttf");
  buffer = shape_text (font, "f");
  g_assert_cmpuint (value (buffer, 0), ==, 1);
  hb_buffer_destroy (buffer);
  advances_asked = 0;
  buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, "j", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
  g_assert_cmpint (advances_asked, ==, 1);
  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}

static void
test_wasm_host_work_limit (void)
{
  const char *shapers[] = {"wasm", NULL};
  hb_font_t *font = open_font ();
  hb_buffer_t *buffer = hb_buffer_create ();

  /* The module stays within its instructions, and is stopped for what
   * it has us do. */
  hb_buffer_add_utf8 (buffer, "k", -1, 0, -1);
  hb_buffer_guess_segment_properties (buffer);
  g_assert_false (hb_shape_full (font, buffer, NULL, 0, shapers));
  hb_buffer_destroy (buffer);

  buffer = shape_text (font, "abc");
  g_assert_cmpuint (hb_buffer_get_length (buffer), ==, 6);
  hb_buffer_destroy (buffer);

  hb_font_destroy (font);
}

#endif

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_wasm_copy_lengths);
  hb_test_add (test_wasm_handles_end_with_the_call);
  hb_test_add (test_wasm_created_objects);
  hb_test_add (test_wasm_handles_belong_to_the_instance);
  hb_test_add (test_wasm_handle_limit);
  hb_test_add (test_wasm_destroy);
  hb_test_add (test_wasm_start_function);
  hb_test_add (test_wasm_refused_modules);
  hb_test_add (test_wasm_instance_kept);
  hb_test_add (test_wasm_module_allocator);
  hb_test_add (test_wasm_module_that_frees_nothing);
  hb_test_add (test_wasm_long_text_after_short);
  hb_test_add (test_wasm_features_without_room);
  hb_test_add (test_wasm_buffer_as_it_came);
  hb_test_add (test_wasm_positions_of_the_caller);
  hb_test_add (test_wasm_failed_call);
  hb_test_add (test_wasm_coords_free);
#ifdef HB_TEST_WASM_BUDGET
  hb_test_add (test_wasm_memory_limit);
  hb_test_add (test_wasm_instruction_limit);
  hb_test_add (test_wasm_host_work_limit);
#endif

  return hb_test_run ();
}
