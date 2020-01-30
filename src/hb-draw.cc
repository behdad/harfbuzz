/*
 * Copyright © 2019-2020  Ebrahim Byagowi
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

#include "hb.hh"

#ifndef HB_NO_DRAW

#include "hb-draw.hh"
#include "hb-ot.h"
#include "hb-ot-glyf-table.hh"
#include "hb-ot-cff1-table.hh"
#include "hb-ot-cff2-table.hh"

/**
 * hb_draw_funcs_set_move_to_func:
 * @funcs: decompose functions object
 * @move_to: move-to callback
 *
 * Sets move-to callback to the decompose functions object.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_set_move_to_func (hb_draw_funcs_t        *funcs,
				hb_draw_move_to_func_t  move_to)
{
  if (unlikely (hb_object_is_immutable (funcs))) return;
  funcs->move_to = move_to;
}

/**
 * hb_draw_funcs_set_line_to_func:
 * @funcs: decompose functions object
 * @line_to: line-to callback
 *
 * Sets line-to callback to the decompose functions object.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_set_line_to_func (hb_draw_funcs_t        *funcs,
				hb_draw_line_to_func_t  line_to)
{
  if (unlikely (hb_object_is_immutable (funcs))) return;
  funcs->line_to = line_to;
}

/**
 * hb_draw_funcs_set_quadratic_to_func:
 * @funcs: decompose functions object
 * @move_to: quadratic-to callback
 *
 * Sets quadratic-to callback to the decompose functions object.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_set_quadratic_to_func (hb_draw_funcs_t             *funcs,
				     hb_draw_quadratic_to_func_t  quadratic_to)
{
  if (unlikely (hb_object_is_immutable (funcs))) return;
  funcs->quadratic_to = quadratic_to;
}

/**
 * hb_draw_funcs_set_cubic_to_func:
 * @funcs: decompose functions
 * @cubic_to: cubic-to callback
 *
 * Sets cubic-to callback to the decompose functions object.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_set_cubic_to_func (hb_draw_funcs_t         *funcs,
				 hb_draw_cubic_to_func_t  cubic_to)
{
  if (unlikely (hb_object_is_immutable (funcs))) return;
  funcs->cubic_to = cubic_to;
}

/**
 * hb_draw_funcs_set_close_path_func:
 * @funcs: decompose functions object
 * @close_path: close-path callback
 *
 * Sets close-path callback to the decompose functions object.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_set_close_path_func (hb_draw_funcs_t           *funcs,
				   hb_draw_close_path_func_t  close_path)
{
  if (unlikely (hb_object_is_immutable (funcs))) return;
  funcs->close_path = close_path;
}

static void
_move_to_nil (const hb_draw_funcs_t *funcs HB_UNUSED,
	      hb_position_t to_x HB_UNUSED, hb_position_t to_y HB_UNUSED,
	      void *user_data HB_UNUSED) {}

static void
_line_to_nil (const hb_draw_funcs_t *funcs HB_UNUSED, hb_position_t from_x HB_UNUSED, hb_position_t from_y HB_UNUSED,
	      hb_position_t to_x HB_UNUSED, hb_position_t to_y HB_UNUSED, void *user_data HB_UNUSED) {}

static void
_quadratic_to_cubic (const hb_draw_funcs_t *funcs, hb_position_t from_x, hb_position_t from_y,
		     hb_position_t control_x, hb_position_t control_y,
		     hb_position_t to_x, hb_position_t to_y, void *user_data HB_UNUSED)
{
   /* based on https://github.com/fonttools/fonttools/blob/a37dab3/Lib/fontTools/pens/basePen.py#L218 */
   hb_position_t mid1_x = roundf ((float) from_x + 0.6666666667f * (control_x - from_x));
   hb_position_t mid1_y = roundf ((float) from_y + 0.6666666667f * (control_y - from_y));
   hb_position_t mid2_x = roundf ((float) to_x + 0.6666666667f * (control_x - to_x));
   hb_position_t mid2_y = roundf ((float) to_y + 0.6666666667f * (control_y - to_y));
   funcs->cubic_to (funcs, from_x, from_y, mid1_x, mid1_y, mid2_x, mid2_y,
		    to_x, to_y, user_data);
}

static void
_cubic_to_nil (const hb_draw_funcs_t *funcs HB_UNUSED,
	       hb_position_t from_x HB_UNUSED, hb_position_t from_y HB_UNUSED,
	       hb_position_t control1_x HB_UNUSED, hb_position_t control1_y HB_UNUSED,
	       hb_position_t control2_x HB_UNUSED, hb_position_t control2_y HB_UNUSED,
	       hb_position_t to_x HB_UNUSED, hb_position_t to_y HB_UNUSED,
	       void *user_data HB_UNUSED) {}

static void
_close_path_nil (const hb_draw_funcs_t *funcs HB_UNUSED, void *user_data HB_UNUSED) {}

/**
 * hb_draw_funcs_create:
 *
 * Creates a new decompose callbacks object.
 *
 * Since: REPLACEME
 **/
hb_draw_funcs_t *
hb_draw_funcs_create ()
{
  hb_draw_funcs_t *funcs;
  if (unlikely (!(funcs = hb_object_create<hb_draw_funcs_t> ())))
    return const_cast<hb_draw_funcs_t *> (&Null (hb_draw_funcs_t));

  funcs->move_to = (hb_draw_move_to_func_t) _move_to_nil;
  funcs->line_to = (hb_draw_line_to_func_t) _line_to_nil;
  funcs->quadratic_to = (hb_draw_quadratic_to_func_t) _quadratic_to_cubic;
  funcs->cubic_to = (hb_draw_cubic_to_func_t) _cubic_to_nil;
  funcs->close_path = (hb_draw_close_path_func_t) _close_path_nil;
  return funcs;
}

/**
 * hb_draw_funcs_reference:
 * @funcs: decompose functions
 *
 * Add to callbacks object refcount.
 *
 * Returns: The same object.
 * Since: REPLACEME
 **/
hb_draw_funcs_t *
hb_draw_funcs_reference (hb_draw_funcs_t *funcs)
{
  return hb_object_reference (funcs);
}

/**
 * hb_draw_funcs_destroy:
 * @funcs: decompose functions
 *
 * Decreases refcount of callbacks object and deletes the object if it reaches
 * to zero.
 *
 * Since: REPLACEME
 **/
void
hb_draw_funcs_destroy (hb_draw_funcs_t *funcs)
{
  if (!hb_object_destroy (funcs)) return;

  free (funcs);
}

/**
 * hb_draw:
 * @font: a font object
 * @glyph: a glyph id
 * @funcs: decompose callbacks object
 * @user_data: parameter you like be passed to the callbacks when are called
 *
 * Decomposes a glyph.
 *
 * Returns: Whether the font had the glyph and the operation completed successfully.
 * Since: REPLACEME
 **/
hb_bool_t
hb_font_draw_glyph (hb_font_t *font, hb_codepoint_t glyph,
		    const hb_draw_funcs_t *funcs,
		    void *user_data)
{
  if (unlikely (funcs == &Null (hb_draw_funcs_t) ||
		glyph >= font->face->get_num_glyphs ()))
    return false;

  if (font->face->table.glyf->get_path (font, glyph, funcs, user_data)) return true;
#ifndef HB_NO_CFF
  if (font->face->table.cff1->get_path (font, glyph, funcs, user_data)) return true;
  if (font->face->table.cff2->get_path (font, glyph, funcs, user_data)) return true;
#endif

  return false;
}

#endif
