/*
 * Copyright © 2019 Adobe Inc.
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
 * Adobe Author(s): Michiharu Ariza
 */

#ifndef HB_BIMAP_HH
#define HB_BIMAP_HH

#include "hb.hh"

/* Bi-directional map */
struct hb_bimap_t
{
  hb_bimap_t () { init (); }
  ~hb_bimap_t () { fini (); }

  void init ()
  {
    forw_map.init ();
    back_map.init ();
  }

  void fini ()
  {
    forw_map.fini ();
    back_map.fini ();
  }

  /* returns HB_MAP_VALUE_INVALID if unmapped */
  hb_codepoint_t forward (hb_codepoint_t lhs) const { return forw_map[lhs]; }
  hb_codepoint_t backward (hb_codepoint_t rhs) const { return back_map[rhs]; }

  void set (hb_codepoint_t lhs, hb_codepoint_t rhs)
  {
    forw_map.set (lhs, rhs);
    back_map.set (rhs, lhs);
  }

  /* Create an identity map. */
  bool identity (unsigned int size)
  {
    clear ();
    for (hb_codepoint_t i = 0; i < size; i++) set (i, i);
    return forw_map.successful && back_map.successful;
  }

  void clear ()
  {
    forw_map.clear ();
    back_map.clear ();
  }

  void reset ()
  {
    forw_map.reset ();
    back_map.reset ();
  }

  unsigned int get_population () const { return forw_map.get_population (); }

  protected:
  hb_map_t  forw_map;
  hb_map_t  back_map;
};

/* Inremental bimap: only lhs is given, rhs is incrementally assigned */
struct hb_inc_bimap_t : hb_bimap_t
{
  /* Add a mapping from lhs to rhs with a unique value if lhs is unknown.
   * Return the rhs value as the result.
   */
  hb_codepoint_t add (hb_codepoint_t lhs)
  {
    hb_codepoint_t  rhs = forw_map[lhs];
    if (rhs == HB_MAP_VALUE_INVALID)
    {
      rhs = get_population ();
      set (lhs, rhs);
    }
    return rhs;
  }

  bool has (hb_codepoint_t lhs) const { return forw_map.has (lhs); }

  protected:
  static int cmp_id (const void* a, const void* b)
  { return (int)*(const hb_codepoint_t *)a - (int)*(const hb_codepoint_t *)b; }

  public:
  /* Optional: after finished adding all mappings in a random order,
   * reassign rhs to lhs so that they are in the same order. */
  void sort ()
  {
    hb_codepoint_t  count = get_population ();
    hb_vector_t <hb_codepoint_t> work;
    work.resize (count);

    for (hb_codepoint_t rhs = 0; rhs < count; rhs++)
      work[rhs] = back_map[rhs];
  
    work.qsort (cmp_id);
  
    for (hb_codepoint_t rhs = 0; rhs < count; rhs++)
      set (work[rhs], rhs);
  }
};

#endif /* HB_BIMAP_HH */
