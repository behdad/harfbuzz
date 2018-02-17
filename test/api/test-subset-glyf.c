/*
 * Copyright © 2018  Google, Inc.
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
 *
 * Google Author(s): Garret Rieger
 */

#include <stdbool.h>

#include "hb-test.h"
#include "hb-subset-test.h"

/* Unit tests for hb-subset-glyf.h */

static void
test_subset_glyf (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Roboto-Regular.abc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Roboto-Regular.ac.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 97);
  hb_set_add (codepoints, 99);
  hb_face_t *face_abc_subset = hb_subset_test_create_subset (face_abc, codepoints);
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('l','o','c', 'a'));
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('m','a','x', 'p'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}

static void
test_subset_glyf_with_components (void)
{
  hb_face_t *face_components = hb_subset_test_open_font ("fonts/Roboto-Regular.components.ttf");
  hb_face_t *face_subset = hb_subset_test_open_font ("fonts/Roboto-Regular.components.subset.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 0x1fc);
  hb_face_t *face_generated_subset = hb_subset_test_create_subset (face_components, codepoints);
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('l','o','c', 'a'));
  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('m','a','x', 'p'));

  hb_face_destroy (face_generated_subset);
  hb_face_destroy (face_subset);
  hb_face_destroy (face_components);
}

static void
test_subset_glyf_noop (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font("fonts/Roboto-Regular.abc.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 97);
  hb_set_add (codepoints, 98);
  hb_set_add (codepoints, 99);
  hb_face_t *face_abc_subset = hb_subset_test_create_subset (face_abc, codepoints);
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_abc, face_abc_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_abc, face_abc_subset, HB_TAG ('l','o','c', 'a'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
}

// TODO(grieger): test for long loca generation.

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_subset_glyf);
  hb_test_add (test_subset_glyf_with_components);
  hb_test_add (test_subset_glyf_noop);

  return hb_test_run();
}
