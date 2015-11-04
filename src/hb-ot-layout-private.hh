/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2012,2013  Google, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_PRIVATE_HH
#define HB_OT_LAYOUT_PRIVATE_HH

#include "hb-private.hh"

#include "hb-font-private.hh"
#include "hb-buffer-private.hh"
#include "hb-set-private.hh"


/* Private API corresponding to hb-ot-layout.h: */

HB_INTERNAL hb_bool_t
hb_ot_layout_table_find_feature (hb_face_t    *face,
				 hb_tag_t      table_tag,
				 hb_tag_t      feature_tag,
				 unsigned int *feature_index);


/*
 * GDEF
 */

typedef enum
{
  /* The following three match LookupFlags::Ignore* numbers. */
  HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH	= 0x02u,
  HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE	= 0x04u,
  HB_OT_LAYOUT_GLYPH_PROPS_MARK		= 0x08u,

  /* The following are used internally; not derived from GDEF. */
  HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED	= 0x10u,
  HB_OT_LAYOUT_GLYPH_PROPS_LIGATED	= 0x20u,
  HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED	= 0x40u,

  HB_OT_LAYOUT_GLYPH_PROPS_PRESERVE     = HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED |
					  HB_OT_LAYOUT_GLYPH_PROPS_LIGATED |
					  HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED
} hb_ot_layout_glyph_class_mask_t;


/*
 * GSUB/GPOS
 */

HB_INTERNAL hb_bool_t
hb_ot_layout_lookup_would_substitute_fast (hb_face_t            *face,
					   unsigned int          lookup_index,
					   const hb_codepoint_t *glyphs,
					   unsigned int          glyphs_length,
					   hb_bool_t             zero_context);


/* Should be called before all the substitute_lookup's are done. */
HB_INTERNAL void
hb_ot_layout_substitute_start (hb_font_t    *font,
			       hb_buffer_t  *buffer);


struct hb_ot_layout_lookup_accelerator_t;

namespace OT {
  struct hb_apply_context_t;
  struct SubstLookup;
}

HB_INTERNAL void
hb_ot_layout_substitute_lookup (OT::hb_apply_context_t *c,
				const OT::SubstLookup &lookup,
				const hb_ot_layout_lookup_accelerator_t &accel);


/* Should be called after all the substitute_lookup's are done */
HB_INTERNAL void
hb_ot_layout_substitute_finish (hb_font_t    *font,
				hb_buffer_t  *buffer);


/* Should be called before all the position_lookup's are done.  Resets positions to zero. */
HB_INTERNAL void
hb_ot_layout_position_start (hb_font_t    *font,
			     hb_buffer_t  *buffer);

/* Should be called after all the position_lookup's are done */
HB_INTERNAL void
hb_ot_layout_position_finish (hb_font_t    *font,
			      hb_buffer_t  *buffer);



/*
 * hb_ot_layout_t
 */

namespace OT {
  struct GDEF;
  struct GSUB;
  struct GPOS;
}

struct hb_ot_layout_lookup_accelerator_t
{
  template <typename TLookup>
  inline void init (const TLookup &lookup)
  {
    digest.init ();
    lookup.add_coverage (&digest);
  }

  inline void fini (void)
  {
  }

  inline bool may_have (hb_codepoint_t g) const {
    return digest.may_have (g);
  }

  private:
  hb_set_digest_t digest;
};

struct hb_ot_layout_t
{
  hb_blob_t *gdef_blob;
  hb_blob_t *gsub_blob;
  hb_blob_t *gpos_blob;

  const struct OT::GDEF *gdef;
  const struct OT::GSUB *gsub;
  const struct OT::GPOS *gpos;

  unsigned int gsub_lookup_count;
  unsigned int gpos_lookup_count;

  hb_ot_layout_lookup_accelerator_t *gsub_accels;
  hb_ot_layout_lookup_accelerator_t *gpos_accels;
};


HB_INTERNAL hb_ot_layout_t *
_hb_ot_layout_create (hb_face_t *face);

HB_INTERNAL void
_hb_ot_layout_destroy (hb_ot_layout_t *layout);


#define hb_ot_layout_from_face(face) ((hb_ot_layout_t *) face->shaper_data.ot)


/*
 * Buffer var routines.
 */

/* buffer var allocations, used during the entire shaping process */
#define unicode_props()		var2.u16[0]

/* buffer var allocations, used during the GSUB/GPOS processing */
#define glyph_props()		var1.u16[0] /* GDEF glyph properties */
#define lig_props()		var1.u8[2] /* GSUB/GPOS ligature tracking */
#define syllable()		var1.u8[3] /* GSUB/GPOS shaping boundaries */


/* loop over syllables */

#define foreach_syllable(buffer, start, end) \
  for (unsigned int \
       _count = buffer->len, \
       start = 0, end = _count ? _next_syllable (buffer, 0) : 0; \
       start < _count; \
       start = end, end = _next_syllable (buffer, start))

static inline unsigned int
_next_syllable (hb_buffer_t *buffer, unsigned int start)
{
  hb_glyph_info_t *info = buffer->info;
  unsigned int count = buffer->len;

  unsigned int syllable = info[start].syllable();
  while (++start < count && syllable == info[start].syllable())
    ;

  return start;
}


/* unicode_props */

/* Design:
 * unicode_props() is a two-byte number.  The low byte includes:
 * - General_Category: 5 bits.
 * - A bit each for:
 *   * Is it Default_Ignorable(); we have a modified Default_Ignorable().
 *   * Is it U+200D ZWJ?
 *   * Is it U+200C ZWNJ?
 *
 * The high-byte has different meanings, switched by the Gen-Cat:
 * - For Mn,Mc,Me: the modified Combining_Class.
 * - For Ws: index of which space character this is, if space fallback
 *   is needed, ie. we don't set this by default, only if asked to.
 *
 * If needed, we can use the ZWJ/ZWNJ to use the high byte as well,
 * freeing two more bits.
 */

enum {
  UPROPS_MASK_ZWJ       = 0x20u,
  UPROPS_MASK_ZWNJ      = 0x40u,
  UPROPS_MASK_IGNORABLE = 0x80u,
  UPROPS_MASK_GEN_CAT   = 0x1Fu
};

enum space_t {
  SPACE = 0,
  SPACE_NBSP,
  SPACE_EN,
  SPACE_EM,
  SPACE_EM_3,
  SPACE_EM_4,
  SPACE_EM_6,
  SPACE_FIGURE,
  SPACE_PUNCTUATION,
  SPACE_THIN,
  SPACE_HAIR,
  SPACE_NARROW,
  SPACE_MEDIUM,
  SPACE_IDEOGRAPHIC,
};

static inline void
_hb_glyph_info_set_unicode_props (hb_glyph_info_t *info, hb_unicode_funcs_t *unicode)
{
  unsigned int u = info->codepoint;
  unsigned int gen_cat = (unsigned int) unicode->general_category (u);
  unsigned int props = gen_cat;

  if (u >= 0x80)
  {
    if (unlikely (unicode->is_default_ignorable (u)))
    {
      props |=  UPROPS_MASK_IGNORABLE;
      if (u == 0x200Cu) props |= UPROPS_MASK_ZWNJ;
      if (u == 0x200Du) props |= UPROPS_MASK_ZWJ;
    }
    else if (unlikely (HB_UNICODE_GENERAL_CATEGORY_IS_NON_ENCLOSING_MARK (gen_cat)))
    {
      /* Only Mn and Mc can have non-zero ccc:
       * http://www.unicode.org/policies/stability_policy.html#Property_Value
       * """
       * Canonical_Combining_Class, General_Category
       * All characters other than those with General_Category property values
       * Spacing_Mark (Mc) and Nonspacing_Mark (Mn) have the Canonical_Combining_Class
       * property value 0.
       * 1.1.5+
       * """
       *
       * Also, all Mn's that are Default_Ignorable, have ccc=0, hence
       * the "else if".
       */
      props |= unicode->modified_combining_class (info->codepoint)<<8;
    }
  }

  info->unicode_props() = props;
}

static inline void
_hb_glyph_info_set_general_category (hb_glyph_info_t *info,
				     hb_unicode_general_category_t gen_cat)
{
  /* Clears top-byte. */
  info->unicode_props() = (unsigned int) gen_cat | (info->unicode_props() & (0xFF & ~UPROPS_MASK_GEN_CAT));
}

static inline hb_unicode_general_category_t
_hb_glyph_info_get_general_category (const hb_glyph_info_t *info)
{
  return (hb_unicode_general_category_t) (info->unicode_props() & UPROPS_MASK_GEN_CAT);
}

static inline bool
_hb_glyph_info_is_unicode_mark (const hb_glyph_info_t *info)
{
  return HB_UNICODE_GENERAL_CATEGORY_IS_MARK (info->unicode_props() & UPROPS_MASK_GEN_CAT);
}
static inline void
_hb_glyph_info_set_modified_combining_class (hb_glyph_info_t *info,
					     unsigned int modified_class)
{
  if (unlikely (!_hb_glyph_info_is_unicode_mark (info)))
    return;
  info->unicode_props() = (modified_class<<8) | (info->unicode_props() & 0xFF);
}
static inline unsigned int
_hb_glyph_info_get_modified_combining_class (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_mark (info) ? info->unicode_props()>>8 : 0;
}

static inline bool
_hb_glyph_info_is_unicode_space (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_get_general_category (info) ==
	 HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR;
}
static inline void
_hb_glyph_info_set_unicode_space_for_char (hb_glyph_info_t *info, hb_codepoint_t u)
{
  if (unlikely (!_hb_glyph_info_is_unicode_space (info)))
    return;

  space_t s;
  switch (u)
  {
    default:      s = SPACE;		break; /* Shouldn't happen. */
    case 0x00A0u: s = SPACE_NBSP;	break;
    case 0x2002u: s = SPACE_EN;		break;
    case 0x2003u: s = SPACE_EM;		break;
    case 0x2004u: s = SPACE_EM_3;	break;
    case 0x2005u: s = SPACE_EM_4;	break;
    case 0x2006u: s = SPACE_EM_6;	break;
    case 0x2007u: s = SPACE_FIGURE;	break;
    case 0x2008u: s = SPACE_PUNCTUATION;break;
    case 0x2009u: s = SPACE_THIN;	break;
    case 0x200Au: s = SPACE_HAIR;	break;
    case 0x202Fu: s = SPACE_NARROW;	break;
    case 0x205Fu: s = SPACE_MEDIUM;	break;
    case 0x3000u: s = SPACE_IDEOGRAPHIC;break;
  }

  info->unicode_props() = (((unsigned int) s)<<8) | (info->unicode_props() & 0xFF);
}
static inline space_t
_hb_glyph_info_get_unicode_space (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_space (info) ? (space_t) (info->unicode_props()>>8) : SPACE;
}

static inline bool _hb_glyph_info_ligated (const hb_glyph_info_t *info);

static inline hb_bool_t
_hb_glyph_info_is_default_ignorable (const hb_glyph_info_t *info)
{
  return (info->unicode_props() & UPROPS_MASK_IGNORABLE) && !_hb_glyph_info_ligated (info);
}

static inline hb_bool_t
_hb_glyph_info_is_zwnj (const hb_glyph_info_t *info)
{
  return !!(info->unicode_props() & UPROPS_MASK_ZWNJ);
}

static inline hb_bool_t
_hb_glyph_info_is_zwj (const hb_glyph_info_t *info)
{
  return !!(info->unicode_props() & UPROPS_MASK_ZWJ);
}

static inline void
_hb_glyph_info_flip_joiners (hb_glyph_info_t *info)
{
  info->unicode_props() ^= UPROPS_MASK_ZWNJ | UPROPS_MASK_ZWJ;
}

/* lig_props: aka lig_id / lig_comp
 *
 * When a ligature is formed:
 *
 *   - The ligature glyph and any marks in between all the same newly allocated
 *     lig_id,
 *   - The ligature glyph will get lig_num_comps set to the number of components
 *   - The marks get lig_comp > 0, reflecting which component of the ligature
 *     they were applied to.
 *   - This is used in GPOS to attach marks to the right component of a ligature
 *     in MarkLigPos,
 *   - Note that when marks are ligated together, much of the above is skipped
 *     and the current lig_id reused.
 *
 * When a multiple-substitution is done:
 *
 *   - All resulting glyphs will have lig_id = 0,
 *   - The resulting glyphs will have lig_comp = 0, 1, 2, ... respectively.
 *   - This is used in GPOS to attach marks to the first component of a
 *     multiple substitution in MarkBasePos.
 *
 * The numbers are also used in GPOS to do mark-to-mark positioning only
 * to marks that belong to the same component of the same ligature.
 */

static inline void
_hb_glyph_info_clear_lig_props (hb_glyph_info_t *info)
{
  info->lig_props() = 0;
}

#define IS_LIG_BASE 0x10

static inline void
_hb_glyph_info_set_lig_props_for_ligature (hb_glyph_info_t *info,
					   unsigned int lig_id,
					   unsigned int lig_num_comps)
{
  info->lig_props() = (lig_id << 5) | IS_LIG_BASE | (lig_num_comps & 0x0F);
}

static inline void
_hb_glyph_info_set_lig_props_for_mark (hb_glyph_info_t *info,
				       unsigned int lig_id,
				       unsigned int lig_comp)
{
  info->lig_props() = (lig_id << 5) | (lig_comp & 0x0F);
}

static inline void
_hb_glyph_info_set_lig_props_for_component (hb_glyph_info_t *info, unsigned int comp)
{
  _hb_glyph_info_set_lig_props_for_mark (info, 0, comp);
}

static inline unsigned int
_hb_glyph_info_get_lig_id (const hb_glyph_info_t *info)
{
  return info->lig_props() >> 5;
}

static inline bool
_hb_glyph_info_ligated_internal (const hb_glyph_info_t *info)
{
  return !!(info->lig_props() & IS_LIG_BASE);
}

static inline unsigned int
_hb_glyph_info_get_lig_comp (const hb_glyph_info_t *info)
{
  if (_hb_glyph_info_ligated_internal (info))
    return 0;
  else
    return info->lig_props() & 0x0F;
}

static inline unsigned int
_hb_glyph_info_get_lig_num_comps (const hb_glyph_info_t *info)
{
  if ((info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE) &&
      _hb_glyph_info_ligated_internal (info))
    return info->lig_props() & 0x0F;
  else
    return 1;
}

static inline uint8_t
_hb_allocate_lig_id (hb_buffer_t *buffer) {
  uint8_t lig_id = buffer->next_serial () & 0x07;
  if (unlikely (!lig_id))
    lig_id = _hb_allocate_lig_id (buffer); /* in case of overflow */
  return lig_id;
}

/* glyph_props: */

static inline void
_hb_glyph_info_set_glyph_props (hb_glyph_info_t *info, unsigned int props)
{
  info->glyph_props() = props;
}

static inline unsigned int
_hb_glyph_info_get_glyph_props (const hb_glyph_info_t *info)
{
  return info->glyph_props();
}

static inline bool
_hb_glyph_info_is_base_glyph (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH);
}

static inline bool
_hb_glyph_info_is_ligature (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE);
}

static inline bool
_hb_glyph_info_is_mark (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_MARK);
}

static inline bool
_hb_glyph_info_substituted (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED);
}

static inline bool
_hb_glyph_info_ligated (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATED);
}

static inline bool
_hb_glyph_info_multiplied (const hb_glyph_info_t *info)
{
  return !!(info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED);
}

static inline bool
_hb_glyph_info_ligated_and_didnt_multiply (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_ligated (info) && !_hb_glyph_info_multiplied (info);
}

static inline void
_hb_glyph_info_clear_ligated_and_multiplied (hb_glyph_info_t *info)
{
  info->glyph_props() &= ~(HB_OT_LAYOUT_GLYPH_PROPS_LIGATED |
			   HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED);
}

static inline void
_hb_glyph_info_clear_substituted_and_ligated_and_multiplied (hb_glyph_info_t *info)
{
  info->glyph_props() &= ~(HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED |
			   HB_OT_LAYOUT_GLYPH_PROPS_LIGATED |
			   HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED);
}


/* Allocation / deallocation. */

static inline void
_hb_buffer_allocate_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_deallocate_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_DEALLOCATE_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_assert_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ASSERT_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_allocate_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, glyph_props);
  HB_BUFFER_ALLOCATE_VAR (buffer, lig_props);
  HB_BUFFER_ALLOCATE_VAR (buffer, syllable);
}

static inline void
_hb_buffer_deallocate_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_DEALLOCATE_VAR (buffer, syllable);
  HB_BUFFER_DEALLOCATE_VAR (buffer, lig_props);
  HB_BUFFER_DEALLOCATE_VAR (buffer, glyph_props);
}

static inline void
_hb_buffer_assert_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ASSERT_VAR (buffer, glyph_props);
  HB_BUFFER_ASSERT_VAR (buffer, lig_props);
  HB_BUFFER_ASSERT_VAR (buffer, syllable);
}

/* Make sure no one directly touches our props... */
#undef unicode_props0
#undef unicode_props1
#undef lig_props
#undef glyph_props


#endif /* HB_OT_LAYOUT_PRIVATE_HH */
