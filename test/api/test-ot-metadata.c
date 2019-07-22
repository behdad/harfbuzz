/*
 * Copyright © 2019  Ebrahim Byagowi
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

#include <hb-ot.h>

/* Unit tests for hb-ot-metadata.h */

static void
test_ot_metadata_reference_entry (void)
{
  hb_face_t *face = hb_test_open_font_file ("fonts/meta.ttf");
  hb_blob_t *dlng = hb_ot_metadata_reference_entry (face, HB_OT_METADATA_DESIGN_LANGUAGES);
  g_assert_cmpint (hb_blob_get_length (dlng), ==, 8);
  g_assert_cmpmem (hb_blob_get_data (dlng, NULL), 8, "ar,de,fa", 8);
  hb_blob_destroy (dlng);
  hb_blob_t *fslf = hb_ot_metadata_reference_entry (face, (hb_ot_metadata_t) HB_TAG ('f','s','l','f'));
  g_assert_cmpint (hb_blob_get_length (fslf), ==, 12);
  hb_blob_destroy (fslf);
  hb_blob_t *nacl = hb_ot_metadata_reference_entry (face, (hb_ot_metadata_t) HB_TAG ('n','a','c','l'));
  g_assert_cmpint (hb_blob_get_length (nacl), ==, 0);
  hb_blob_destroy (nacl);
  hb_blob_t *slng = hb_ot_metadata_reference_entry (face, HB_OT_METADATA_SUPPORTED_LANGUAGES);
  g_assert_cmpint (hb_blob_get_length (slng), ==, 11);
  g_assert_cmpmem (hb_blob_get_data (slng, NULL), 11, "ar,de,en,fa", 11);
  hb_blob_destroy (slng);
  hb_face_destroy (face);
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);
  hb_test_add (test_ot_metadata_reference_entry);
  return hb_test_run ();
}
