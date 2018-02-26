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

#include "hb-open-type-private.hh"
#include "hb-ot-glyf-table.hh"
#include "hb-set.h"
#include "hb-subset-glyf.hh"
#include "hb-subset-plan.hh"

static bool
_calculate_glyf_and_loca_prime_size (const OT::glyf::accelerator_t &glyf,
                                     hb_set_t *gids_to_retain,
                                     bool *use_short_loca, /* OUT */
                                     unsigned int *glyf_size, /* OUT */
                                     unsigned int *loca_size /* OUT */)
{
  unsigned int total = 0;
  unsigned int count = 0;
  hb_codepoint_t glyph_id = HB_SET_VALUE_INVALID;
  while (hb_set_next (gids_to_retain, &glyph_id))
  {
    unsigned int start_offset, end_offset;
    if (unlikely (!glyf.get_offsets (glyph_id, &start_offset, &end_offset)))
      end_offset = start_offset = 0;

    total += end_offset - start_offset;
    count++;
  }

  *glyf_size = total;
  *use_short_loca = (total <= 131070);
  *loca_size = (count + 1)
      * (*use_short_loca ? sizeof(OT::HBUINT16) : sizeof(OT::HBUINT32));

  DEBUG_MSG(SUBSET, nullptr, "preparing to subset glyf: final size %d, loca size %d, using %s loca",
            total,
            *loca_size,
            *use_short_loca ? "short" : "long");
  return true;
}

static bool
_write_loca_entry (unsigned int  id,
                   unsigned int  offset,
                   bool          is_short,
                   void         *loca_prime,
                   unsigned int  loca_size)
{
  unsigned int entry_size = is_short ? sizeof (OT::HBUINT16) : sizeof (OT::HBUINT32);
  if ((id + 1) * entry_size <= loca_size)
  {
    if (is_short) {
      ((OT::HBUINT16*) loca_prime) [id].set (offset / 2);
    } else {
      ((OT::HBUINT32*) loca_prime) [id].set (offset);
    }
    return true;
  }

  // Offset was not written because the write is out of bounds.
  DEBUG_MSG (SUBSET,
             nullptr,
             "WARNING: Attempted to write an out of bounds loca entry at index %d. Loca size is %d.",
             id,
             loca_size);
  return false;
}

static void
_update_components (hb_subset_plan_t * plan,
		    char * glyph_start,
		    unsigned int length)

{
  OT::glyf::CompositeGlyphHeader::Iterator iterator;
  if (OT::glyf::CompositeGlyphHeader::get_iterator (glyph_start,
						    length,
						    &iterator))
  {
    do
    {
      hb_codepoint_t new_gid;
      if (!hb_map_get (plan->dest_gid_by_source_gid,
                       iterator.current->glyphIndex,
                       &new_gid))
      {
        DEBUG_MSG(SUBSET, nullptr, "Missing new gid for %d", new_gid);
        continue;
      }
      ((OT::glyf::CompositeGlyphHeader *) iterator.current)->glyphIndex.set (new_gid);
    } while (iterator.move_to_next());
  }
}

static bool
_write_glyf_and_loca_prime (hb_subset_plan_t              *plan,
			    const OT::glyf::accelerator_t &glyf,
                            const char                    *glyf_data,
                            bool                           use_short_loca,
                            unsigned int                   glyf_prime_size,
                            char                          *glyf_prime_data /* OUT */,
                            unsigned int                   loca_prime_size,
                            char                          *loca_prime_data /* OUT */)
{
  char *glyf_prime_data_next = glyf_prime_data;

  bool success = true;
  hb_codepoint_t glyph_id = HB_SET_VALUE_INVALID;
  unsigned int i = 0;
  while (hb_set_next( plan->gids_to_retain, &glyph_id))
  {
    unsigned int start_offset, end_offset;
    if (unlikely (!glyf.get_offsets (glyph_id, &start_offset, &end_offset)))
      end_offset = start_offset = 0;

    int length = end_offset - start_offset;

    if (glyf_prime_data_next + length > glyf_prime_data + glyf_prime_size)
    {
      DEBUG_MSG (SUBSET,
                 nullptr,
                 "WARNING: Attempted to write an out of bounds glyph entry for gid %d",
                 i);
      return false;
    }
    memcpy (glyf_prime_data_next, glyf_data + start_offset, length);

    success = success && _write_loca_entry (i,
                                            glyf_prime_data_next - glyf_prime_data,
                                            use_short_loca,
                                            loca_prime_data,
                                            loca_prime_size);
    _update_components (plan, glyf_prime_data_next, end_offset - start_offset);

    glyf_prime_data_next += length;
    i++;
  }

  success = success && _write_loca_entry (hb_set_get_population (plan->gids_to_retain),
                                          glyf_prime_data_next - glyf_prime_data,
                                          use_short_loca,
                                          loca_prime_data,
                                          loca_prime_size);
  return success;
}

static bool
_hb_subset_glyf_and_loca (const OT::glyf::accelerator_t  &glyf,
                          const char                     *glyf_data,
                          hb_subset_plan_t               *plan,
                          bool                           *use_short_loca,
                          hb_blob_t                     **glyf_prime /* OUT */,
                          hb_blob_t                     **loca_prime /* OUT */)
{
  // TODO(grieger): Sanity check allocation size for the new table.

  unsigned int glyf_prime_size;
  unsigned int loca_prime_size;

  if (unlikely (!_calculate_glyf_and_loca_prime_size (glyf,
                                                      plan->gids_to_retain,
                                                      use_short_loca,
                                                      &glyf_prime_size,
                                                      &loca_prime_size))) {
    return false;
  }

  char *glyf_prime_data = (char *) malloc (glyf_prime_size);
  char *loca_prime_data = (char *) malloc (loca_prime_size);
  if (unlikely (!_write_glyf_and_loca_prime (plan, glyf, glyf_data,
                                             *use_short_loca,
                                             glyf_prime_size, glyf_prime_data,
                                             loca_prime_size, loca_prime_data))) {
    free (glyf_prime_data);
    free (loca_prime_data);
    return false;
  }

  *glyf_prime = hb_blob_create (glyf_prime_data,
                                glyf_prime_size,
                                HB_MEMORY_MODE_READONLY,
                                glyf_prime_data,
                                free);
  *loca_prime = hb_blob_create (loca_prime_data,
                                loca_prime_size,
                                HB_MEMORY_MODE_READONLY,
                                loca_prime_data,
                                free);
  return true;
}

/**
 * hb_subset_glyf:
 * Subsets the glyph table according to a provided plan.
 *
 * Return value: subsetted glyf table.
 *
 * Since: 1.7.5
 **/
bool
hb_subset_glyf_and_loca (hb_subset_plan_t *plan,
                         bool             *use_short_loca, /* OUT */
                         hb_blob_t       **glyf_prime, /* OUT */
                         hb_blob_t       **loca_prime /* OUT */)
{
  hb_blob_t *glyf_blob = OT::Sanitizer<OT::glyf>().sanitize (plan->source->reference_table (HB_OT_TAG_glyf));
  const char *glyf_data = hb_blob_get_data(glyf_blob, nullptr);

  OT::glyf::accelerator_t glyf;
  glyf.init (plan->source);
  bool result = _hb_subset_glyf_and_loca (glyf,
                                          glyf_data,
                                          plan,
                                          use_short_loca,
                                          glyf_prime,
                                          loca_prime);

  hb_blob_destroy (glyf_blob);
  glyf.fini();

  return result;
}
