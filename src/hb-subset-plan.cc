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
 * Google Author(s): Garret Rieger, Roderick Sheeter
 */

#include "hb-subset-private.hh"

#include "hb-subset-plan.hh"
#include "hb-ot-cmap-table.hh"

HB_INTERNAL int
_hb_codepoint_t_cmp (const void *l, const void *r) {
  return *((hb_codepoint_t *) l) - *((hb_codepoint_t *) r);
}

hb_bool_t
hb_subset_plan_new_gid_for_old_id (hb_subset_plan_t *plan,
                                   hb_codepoint_t old_gid,
                                   hb_codepoint_t *new_gid)
{
  // the index in old_gids is the new gid; only up to codepoints.len are valid
  for (unsigned int i = 0; i < plan->gids_to_retain_sorted.len; i++) {
    if (plan->gids_to_retain_sorted[i] == old_gid) {
      *new_gid = i;
      return true;
    }
  }
  return false;
}

HB_INTERNAL void
_populate_codepoints (hb_set_t *input_codepoints,
                      hb_prealloced_array_t<hb_codepoint_t>& plan_codepoints)
{
  plan_codepoints.alloc (hb_set_get_population (input_codepoints));
  hb_codepoint_t cp = -1;
  while (hb_set_next (input_codepoints, &cp)) {
    hb_codepoint_t *wr = plan_codepoints.push();
    *wr = cp;
  }
  plan_codepoints.qsort (_hb_codepoint_t_cmp);
}

HB_INTERNAL void
_populate_gids_to_retain (hb_face_t *face,
                          hb_prealloced_array_t<hb_codepoint_t>& codepoints,
                          hb_prealloced_array_t<hb_codepoint_t>& old_gids,
                          hb_prealloced_array_t<hb_codepoint_t>& old_gids_sorted)
{
  OT::cmap::accelerator_t cmap;
  cmap.init (face);

  hb_prealloced_array_t<unsigned int> bad_indices;

  old_gids.alloc (codepoints.len);
  bool has_zero = false;
  for (unsigned int i = 0; i < codepoints.len; i++) {
    hb_codepoint_t gid;
    if (!cmap.get_nominal_glyph (codepoints[i], &gid)) {
      gid = -1;
      *(bad_indices.push ()) = i;
    }
    if (gid == 0) {
      has_zero = true;
    }
    *(old_gids.push ()) = gid;
  }

  while (bad_indices.len > 0) {
    unsigned int i = bad_indices[bad_indices.len - 1];
    bad_indices.pop ();
    DEBUG_MSG(SUBSET, nullptr, "Drop U+%04X; no gid", codepoints[i]);
    codepoints.remove (i);
    old_gids.remove (i);
  }

  // Populate a second glyph id array that is sorted by glyph id
  // and is gauranteed to contain 0.
  old_gids_sorted.alloc (old_gids.len + (has_zero ? 0 : 1));
  for (unsigned int i = 0; i < old_gids.len; i++) {
    *(old_gids_sorted.push ()) = old_gids[i];
  }
  if (!has_zero)
    *(old_gids_sorted.push ()) = 0;
  old_gids_sorted.qsort (_hb_codepoint_t_cmp);

  for (unsigned int i = 0; i < codepoints.len; i++) {
      DEBUG_MSG(SUBSET, nullptr, " U+%04X, old_gid %d, new_gid %d", codepoints[i], old_gids[i], i);
  }

  // TODO(Q1) expand with glyphs that make up complex glyphs
  // TODO expand with glyphs reached by G*
  //
  cmap.fini ();
}

/**
 * hb_subset_plan_create:
 * Computes a plan for subsetting the supplied face according
 * to a provide profile and input. The plan describes
 * which tables and glyphs should be retained.
 *
 * Return value: New subset plan.
 *
 * Since: 1.7.5
 **/
hb_subset_plan_t *
hb_subset_plan_create (hb_face_t           *face,
                       hb_subset_profile_t *profile,
                       hb_subset_input_t   *input)
{
  hb_subset_plan_t *plan = hb_object_create<hb_subset_plan_t> ();

  plan->codepoints.init();
  plan->gids_to_retain.init();
  plan->gids_to_retain_sorted.init();

  _populate_codepoints (input->codepoints, plan->codepoints);
  _populate_gids_to_retain (face,
                            plan->codepoints,
                            plan->gids_to_retain,
                            plan->gids_to_retain_sorted);
  return plan;
}

/**
 * hb_subset_plan_destroy:
 *
 * Since: 1.7.5
 **/
void
hb_subset_plan_destroy (hb_subset_plan_t *plan)
{
  if (!hb_object_destroy (plan)) return;

  plan->codepoints.finish ();
  plan->gids_to_retain.finish ();
  plan->gids_to_retain_sorted.finish ();

  free (plan);
}
