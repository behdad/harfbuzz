/*
 * Copyright © 2012  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_SHAPE_COMPLEX_ARABIC_FALLBACK_HH
#define HB_OT_SHAPE_COMPLEX_ARABIC_FALLBACK_HH

#include "hb.hh"

#include "hb-ot-cmap-table.hh"
#include "hb-ot-shape.hh"
#include "hb-ot-shape-complex-arabic-pua.hh"
#include "hb-ot-layout-gsub-table.hh"


/* Features ordered the same as the entries in shaping_table rows,
 * followed by rlig.  Don't change. */
static const hb_tag_t arabic_fallback_features[] =
{
  HB_TAG('i','n','i','t'),
  HB_TAG('m','e','d','i'),
  HB_TAG('f','i','n','a'),
  HB_TAG('i','s','o','l'),
  HB_TAG('r','l','i','g'),
};

static OT::SubstLookup *
synthesize_lookup_single (const hb_ot_shape_plan_t *plan HB_UNUSED,
			  hb_font_t *font,
			  unsigned int feature_index,
			  const uint16_t table[][4],
			  hb_codepoint_t table_first,
			  hb_codepoint_t table_last,
			  OT::HBGlyphID *glyphs,
			  OT::HBGlyphID *substitutes,
			  char *buf,
			  size_t buf_len)
{
  unsigned int num_glyphs = 0;

  /* Populate arrays */
  for (hb_codepoint_t u = table_first; u < table_last + 1; u++)
  {
    hb_codepoint_t s = table[u - table_first][feature_index];
    hb_codepoint_t u_glyph, s_glyph;

    if (!s ||
	!hb_font_get_glyph (font, u, 0, &u_glyph) ||
	!hb_font_get_glyph (font, s, 0, &s_glyph) ||
	u_glyph == s_glyph ||
	u_glyph > 0xFFFFu || s_glyph > 0xFFFFu)
      continue;

    glyphs[num_glyphs] = u_glyph;
    substitutes[num_glyphs] = s_glyph;

    num_glyphs++;
  }

  if (!num_glyphs)
    return nullptr;

  /* Bubble-sort or something equally good!
   * May not be good-enough for presidential candidate interviews, but good-enough for us... */
  hb_stable_sort (&glyphs[0], num_glyphs,
		  (int(*)(const OT::HBUINT16*, const OT::HBUINT16 *)) OT::HBGlyphID::cmp,
		  &substitutes[0]);


  hb_serialize_context_t c (buf, buf_len);
  OT::SubstLookup *lookup = c.start_serialize<OT::SubstLookup> ();
  bool ret = lookup->serialize_single (&c,
				       OT::LookupFlag::IgnoreMarks,
				       hb_sorted_array (glyphs, num_glyphs),
				       hb_array (substitutes, num_glyphs));
  c.end_serialize ();

  return ret && !c.in_error () ? c.copy<OT::SubstLookup> () : nullptr;
}

static OT::SubstLookup *
synthesize_lookup_ligature (const hb_ot_shape_plan_t *plan HB_UNUSED,
			    hb_font_t *font,
			    OT::HBGlyphID *first_glyphs,
			    unsigned int *first_glyphs_indirection,
			    unsigned int *ligature_per_first_glyph_count_list,
			    OT::HBGlyphID *ligature_list,
			    unsigned int *component_count_list,
			    OT::HBGlyphID *component_list,
			    const struct ligature_set_t *table,
			    unsigned int table_len,
			    char *buf,
			    size_t buf_len,
			    unsigned int lookupflag)
{
  unsigned int num_first_glyphs = 0;
  unsigned int num_ligatures = 0;

  /* Populate arrays */

  /* Sort out the first-glyphs */
  for (unsigned int first_glyph_idx = 0; first_glyph_idx < table_len; first_glyph_idx++)
  {
    hb_codepoint_t first_u = table[first_glyph_idx].first;
    hb_codepoint_t first_glyph;
    if (!hb_font_get_glyph (font, first_u, 0, &first_glyph))
      continue;
    first_glyphs[num_first_glyphs] = first_glyph;
    ligature_per_first_glyph_count_list[num_first_glyphs] = 0;
    first_glyphs_indirection[num_first_glyphs] = first_glyph_idx;
    num_first_glyphs++;
  }
  hb_stable_sort (&first_glyphs[0], num_first_glyphs,
		  (int(*)(const OT::HBUINT16*, const OT::HBUINT16 *)) OT::HBGlyphID::cmp,
		  &first_glyphs_indirection[0]);

  /* Now that the first-glyphs are sorted, walk again, populate ligatures. */
  for (unsigned int i = 0; i < num_first_glyphs; i++)
  {
    unsigned int first_glyph_idx = first_glyphs_indirection[i];

    for (unsigned int second_glyph_idx = 0; second_glyph_idx < ARRAY_LENGTH (table[0].ligatures); second_glyph_idx++)
    {
      hb_codepoint_t second_u   = table[first_glyph_idx].ligatures[second_glyph_idx].second;
      hb_codepoint_t ligature_u = table[first_glyph_idx].ligatures[second_glyph_idx].ligature;
      hb_codepoint_t second_glyph, ligature_glyph;
      if (!second_u ||
	  !hb_font_get_glyph (font, second_u,   0, &second_glyph) ||
	  !hb_font_get_glyph (font, ligature_u, 0, &ligature_glyph))
	continue;

      ligature_per_first_glyph_count_list[i]++;

      ligature_list[num_ligatures] = ligature_glyph;
      component_count_list[num_ligatures] = 2;
      component_list[num_ligatures] = second_glyph;
      num_ligatures++;
    }
  }

  if (!num_ligatures)
    return nullptr;


  hb_serialize_context_t c (buf, buf_len);
  OT::SubstLookup *lookup = c.start_serialize<OT::SubstLookup> ();
  bool ret = lookup->serialize_ligature (&c,
					 lookupflag,
					 hb_sorted_array (first_glyphs, num_first_glyphs),
					 hb_array (ligature_per_first_glyph_count_list, num_first_glyphs),
					 hb_array (ligature_list, num_ligatures),
					 hb_array (component_count_list, num_ligatures),
					 hb_array (component_list, num_ligatures));
  c.end_serialize ();
  /* TODO sanitize the results? */

  return ret && !c.in_error () ? c.copy<OT::SubstLookup> () : nullptr;
}


static OT::SubstLookup *
arabic_fallback_synthesize_lookup_single (const hb_ot_shape_plan_t *plan HB_UNUSED,
					  hb_font_t *font,
					  unsigned int feature_index)
{
  OT::HBGlyphID glyphs[SHAPING_TABLE_LAST - SHAPING_TABLE_FIRST + 1];
  OT::HBGlyphID substitutes[SHAPING_TABLE_LAST - SHAPING_TABLE_FIRST + 1];
  /* Each glyph takes four bytes max, and there's some overhead. */
  char buf[(SHAPING_TABLE_LAST - SHAPING_TABLE_FIRST + 1) * 4 + 128];

  return synthesize_lookup_single (plan,
				   font,
				   feature_index,
				   shaping_table,
				   SHAPING_TABLE_FIRST,
				   SHAPING_TABLE_LAST,
				   glyphs,
				   substitutes,
				   buf,
				   sizeof (buf));
}

static OT::SubstLookup *
arabic_fallback_synthesize_lookup_ligature (const hb_ot_shape_plan_t *plan HB_UNUSED,
					    hb_font_t *font)
{
  OT::HBGlyphID first_glyphs[ARRAY_LENGTH_CONST (ligature_table)];
  unsigned int first_glyphs_indirection[ARRAY_LENGTH_CONST (ligature_table)];
  unsigned int ligature_per_first_glyph_count_list[ARRAY_LENGTH_CONST (first_glyphs)];

  /* We know that all our ligatures are 2-component */
  OT::HBGlyphID ligature_list[ARRAY_LENGTH_CONST (first_glyphs) * ARRAY_LENGTH_CONST(ligature_table[0].ligatures)];
  unsigned int component_count_list[ARRAY_LENGTH_CONST (ligature_list)];
  OT::HBGlyphID component_list[ARRAY_LENGTH_CONST (ligature_list) * 1/* One extra component per ligature */];

  /* 16 bytes per ligature ought to be enough... */
  char buf[ARRAY_LENGTH_CONST (ligature_list) * 16 + 128];

  return synthesize_lookup_ligature (plan,
				     font,
				     first_glyphs,
				     first_glyphs_indirection,
				     ligature_per_first_glyph_count_list,
				     ligature_list,
				     component_count_list,
				     component_list,
				     ligature_table,
				     ARRAY_LENGTH_CONST (ligature_table),
				     buf,
				     sizeof (buf),
				     OT::LookupFlag::IgnoreMarks);
}

static OT::SubstLookup *
arabic_fallback_synthesize_lookup (const hb_ot_shape_plan_t *plan,
				   hb_font_t *font,
				   unsigned int feature_index)
{
  if (feature_index < 4)
    return arabic_fallback_synthesize_lookup_single (plan, font, feature_index);
  else
    return arabic_fallback_synthesize_lookup_ligature (plan, font);
}

#define ARABIC_FALLBACK_MAX_LOOKUPS 5

struct arabic_fallback_plan_t
{
  unsigned int num_lookups;
  bool free_lookups;

  hb_mask_t mask_array[ARABIC_FALLBACK_MAX_LOOKUPS];
  OT::SubstLookup *lookup_array[ARABIC_FALLBACK_MAX_LOOKUPS];
  OT::hb_ot_layout_lookup_accelerator_t accel_array[ARABIC_FALLBACK_MAX_LOOKUPS];
};

static OT::SubstLookup *
arabic_fallback_pua1_synthesize_lookup_single (const hb_ot_shape_plan_t *plan HB_UNUSED,
					       hb_font_t *font,
					       unsigned int feature_index)
{
  OT::HBGlyphID glyphs[SHAPING_TABLE_PUA1_LEN];
  OT::HBGlyphID substitutes[SHAPING_TABLE_PUA1_LEN];
  /* Each glyph takes four bytes max, and there's some overhead. */
  char buf[SHAPING_TABLE_PUA1_LEN * 4 + 128];

  return synthesize_lookup_single (plan,
				   font,
				   feature_index,
				   shaping_table_pua1,
				   SHAPING_TABLE_PUA1_FIRST,
				   SHAPING_TABLE_PUA1_LAST,
				   glyphs,
				   substitutes,
				   buf,
				   sizeof (buf));
}

static OT::SubstLookup *
arabic_fallback_pua2_synthesize_lookup_single (const hb_ot_shape_plan_t *plan HB_UNUSED,
					       hb_font_t *font,
					       unsigned int feature_index)
{
  OT::HBGlyphID glyphs[SHAPING_TABLE_PUA2_LEN];
  OT::HBGlyphID substitutes[SHAPING_TABLE_PUA2_LEN];
  /* Each glyph takes four bytes max, and there's some overhead. */
  char buf[SHAPING_TABLE_PUA2_LEN * 4 + 128];

  return synthesize_lookup_single (plan,
				   font,
				   feature_index,
				   shaping_table_pua2,
				   SHAPING_TABLE_PUA2_FIRST,
				   SHAPING_TABLE_PUA2_LAST,
				   glyphs,
				   substitutes,
				   buf,
				   sizeof (buf));
}

static OT::SubstLookup *
arabic_fallback_pua1_synthesize_lookup_ligature (const hb_ot_shape_plan_t *plan HB_UNUSED,
						 hb_font_t *font)
{
  OT::HBGlyphID first_glyphs[ARRAY_LENGTH_CONST (ligature_table_pua1)];
  unsigned int first_glyphs_indirection[ARRAY_LENGTH_CONST (ligature_table_pua1)];
  unsigned int ligature_per_first_glyph_count_list[ARRAY_LENGTH_CONST (first_glyphs)];

  /* We know that all our ligatures are 2-component */
  OT::HBGlyphID ligature_list[ARRAY_LENGTH_CONST (first_glyphs) * ARRAY_LENGTH_CONST(ligature_table_pua1[0].ligatures)];
  unsigned int component_count_list[ARRAY_LENGTH_CONST (ligature_list)];
  OT::HBGlyphID component_list[ARRAY_LENGTH_CONST (ligature_list) * 1/* One extra component per ligature */];

  /* 16 bytes per ligature ought to be enough... */
  char buf[ARRAY_LENGTH_CONST (ligature_list) * 16 + 128];

  return synthesize_lookup_ligature (plan,
				     font,
				     first_glyphs,
				     first_glyphs_indirection,
				     ligature_per_first_glyph_count_list,
				     ligature_list,
				     component_count_list,
				     component_list,
				     ligature_table_pua1,
				     ARRAY_LENGTH_CONST (ligature_table_pua1),
				     buf,
				     sizeof (buf),
				     0);
}

static OT::SubstLookup *
arabic_fallback_pua2_synthesize_lookup_ligature (const hb_ot_shape_plan_t *plan HB_UNUSED,
						 hb_font_t *font)
{
  OT::HBGlyphID first_glyphs[ARRAY_LENGTH_CONST (ligature_table_pua2)];
  unsigned int first_glyphs_indirection[ARRAY_LENGTH_CONST (ligature_table_pua2)];
  unsigned int ligature_per_first_glyph_count_list[ARRAY_LENGTH_CONST (first_glyphs)];

  /* We know that all our ligatures are 2-component */
  OT::HBGlyphID ligature_list[ARRAY_LENGTH_CONST (first_glyphs) * ARRAY_LENGTH_CONST(ligature_table_pua2[0].ligatures)];
  unsigned int component_count_list[ARRAY_LENGTH_CONST (ligature_list)];
  OT::HBGlyphID component_list[ARRAY_LENGTH_CONST (ligature_list) * 1/* One extra component per ligature */];

  /* 16 bytes per ligature ought to be enough... */
  char buf[ARRAY_LENGTH_CONST (ligature_list) * 16 + 128];

  return synthesize_lookup_ligature (plan,
				     font,
				     first_glyphs,
				     first_glyphs_indirection,
				     ligature_per_first_glyph_count_list,
				     ligature_list,
				     component_count_list,
				     component_list,
				     ligature_table_pua2,
				     ARRAY_LENGTH_CONST (ligature_table_pua2),
				     buf,
				     sizeof (buf),
				     0);
}

static OT::SubstLookup *
arabic_fallback_pua_synthesize_lookup (const hb_ot_shape_plan_t *plan,
					  hb_font_t *font,
					  unsigned int feature_index)
{
  switch ((unsigned) font->face->table.OS2->get_font_page ()) {
  case OT::OS2::font_page_t::FONT_PAGE_SIMP_ARABIC:
  {
    if (feature_index < 4)
      return arabic_fallback_pua1_synthesize_lookup_single (plan, font, feature_index);
    else
      return arabic_fallback_pua1_synthesize_lookup_ligature (plan, font);
  }
  case OT::OS2::font_page_t::FONT_PAGE_TRAD_ARABIC:
  {
    if (feature_index < 4)
      return arabic_fallback_pua2_synthesize_lookup_single (plan, font, feature_index);
    else
      return arabic_fallback_pua2_synthesize_lookup_ligature (plan, font);
  }
  default:
    return nullptr;
  }
}

#if defined(_WIN32) && !defined(HB_NO_WIN1256)
#define HB_WITH_WIN1256
#endif

#ifdef HB_WITH_WIN1256
#include "hb-ot-shape-complex-arabic-win1256.hh"
#endif

struct ManifestLookup
{
  public:
  OT::Tag tag;
  OT::Offset16To<OT::SubstLookup> lookupOffset;
  public:
  DEFINE_SIZE_STATIC (6);
};
typedef OT::Array16Of<ManifestLookup> Manifest;

static bool
arabic_fallback_plan_init_win1256 (arabic_fallback_plan_t *fallback_plan HB_UNUSED,
				   const hb_ot_shape_plan_t *plan HB_UNUSED,
				   hb_font_t *font HB_UNUSED)
{
#ifdef HB_WITH_WIN1256
  /* Does this font look like it's Windows-1256-encoded? */
  hb_codepoint_t g;
  if (!(hb_font_get_glyph (font, 0x0627u, 0, &g) && g == 199 /* ALEF */ &&
	hb_font_get_glyph (font, 0x0644u, 0, &g) && g == 225 /* LAM */ &&
	hb_font_get_glyph (font, 0x0649u, 0, &g) && g == 236 /* ALEF MAKSURA */ &&
	hb_font_get_glyph (font, 0x064Au, 0, &g) && g == 237 /* YEH */ &&
	hb_font_get_glyph (font, 0x0652u, 0, &g) && g == 250 /* SUKUN */))
    return false;

  const Manifest &manifest = reinterpret_cast<const Manifest&> (arabic_win1256_gsub_lookups.manifest);
  static_assert (sizeof (arabic_win1256_gsub_lookups.manifestData) ==
		 ARABIC_FALLBACK_MAX_LOOKUPS * sizeof (ManifestLookup), "");
  /* TODO sanitize the table? */

  unsigned j = 0;
  unsigned int count = manifest.len;
  for (unsigned int i = 0; i < count; i++)
  {
    fallback_plan->mask_array[j] = plan->map.get_1_mask (manifest[i].tag);
    if (fallback_plan->mask_array[j])
    {
      fallback_plan->lookup_array[j] = const_cast<OT::SubstLookup*> (&(&manifest+manifest[i].lookupOffset));
      if (fallback_plan->lookup_array[j])
      {
	fallback_plan->accel_array[j].init (*fallback_plan->lookup_array[j]);
	j++;
      }
    }
  }

  fallback_plan->num_lookups = j;
  fallback_plan->free_lookups = false;

  return j > 0;
#else
  return false;
#endif
}

static bool
arabic_fallback_plan_init_pua (arabic_fallback_plan_t *fallback_plan,
			       const hb_ot_shape_plan_t *plan,
			       hb_font_t *font)
{
  switch ((unsigned) font->face->table.OS2->get_font_page ()) {
  case OT::OS2::font_page_t::FONT_PAGE_SIMP_ARABIC:
  case OT::OS2::font_page_t::FONT_PAGE_TRAD_ARABIC:
    break;
  default:
    return false;
  }

  static_assert ((ARRAY_LENGTH_CONST(arabic_fallback_features) <= ARABIC_FALLBACK_MAX_LOOKUPS), "");
  unsigned int j = 0;
  for (unsigned int i = 0; i < ARRAY_LENGTH(arabic_fallback_features) ; i++)
  {
    fallback_plan->mask_array[j] = plan->map.get_1_mask (arabic_fallback_features[i]);
    if (fallback_plan->mask_array[j])
    {
      fallback_plan->lookup_array[j] = arabic_fallback_pua_synthesize_lookup (plan, font, i);
      if (fallback_plan->lookup_array[j])
      {
	fallback_plan->accel_array[j].init (*fallback_plan->lookup_array[j]);
	j++;
      }
    }
  }

  fallback_plan->num_lookups = j;
  fallback_plan->free_lookups = true;

  return j > 0;
}

static bool
arabic_fallback_plan_init_unicode (arabic_fallback_plan_t *fallback_plan,
				   const hb_ot_shape_plan_t *plan,
				   hb_font_t *font)
{
  static_assert ((ARRAY_LENGTH_CONST(arabic_fallback_features) <= ARABIC_FALLBACK_MAX_LOOKUPS), "");
  unsigned int j = 0;
  for (unsigned int i = 0; i < ARRAY_LENGTH(arabic_fallback_features) ; i++)
  {
    fallback_plan->mask_array[j] = plan->map.get_1_mask (arabic_fallback_features[i]);
    if (fallback_plan->mask_array[j])
    {
      fallback_plan->lookup_array[j] = arabic_fallback_synthesize_lookup (plan, font, i);
      if (fallback_plan->lookup_array[j])
      {
	fallback_plan->accel_array[j].init (*fallback_plan->lookup_array[j]);
	j++;
      }
    }
  }

  fallback_plan->num_lookups = j;
  fallback_plan->free_lookups = true;

  return j > 0;
}

static arabic_fallback_plan_t *
arabic_fallback_plan_create (const hb_ot_shape_plan_t *plan,
			     hb_font_t *font)
{
  arabic_fallback_plan_t *fallback_plan = (arabic_fallback_plan_t *) hb_calloc (1, sizeof (arabic_fallback_plan_t));
  if (unlikely (!fallback_plan))
    return const_cast<arabic_fallback_plan_t *> (&Null (arabic_fallback_plan_t));

  fallback_plan->num_lookups = 0;
  fallback_plan->free_lookups = false;

  /* Try synthesizing GSUB table using legacy PUA mapping,
   * in case the font has cmap entries for the PUA characters. */
  if (arabic_fallback_plan_init_pua (fallback_plan, plan, font))
    return fallback_plan;

  /* Try synthesizing GSUB table using Unicode Arabic Presentation Forms,
   * in case the font has cmap entries for the presentation-forms characters. */
  if (arabic_fallback_plan_init_unicode (fallback_plan, plan, font))
    return fallback_plan;

  /* See if this looks like a Windows-1256-encoded font.  If it does, use a
   * hand-coded GSUB table. */
  if (arabic_fallback_plan_init_win1256 (fallback_plan, plan, font))
    return fallback_plan;

  assert (fallback_plan->num_lookups == 0);
  hb_free (fallback_plan);
  return const_cast<arabic_fallback_plan_t *> (&Null (arabic_fallback_plan_t));
}

static void
arabic_fallback_plan_destroy (arabic_fallback_plan_t *fallback_plan)
{
  if (!fallback_plan || fallback_plan->num_lookups == 0)
    return;

  for (unsigned int i = 0; i < fallback_plan->num_lookups; i++)
    if (fallback_plan->lookup_array[i])
    {
      fallback_plan->accel_array[i].fini ();
      if (fallback_plan->free_lookups)
	hb_free (fallback_plan->lookup_array[i]);
    }

  hb_free (fallback_plan);
}

static void
arabic_fallback_plan_shape (arabic_fallback_plan_t *fallback_plan,
			    hb_font_t *font,
			    hb_buffer_t *buffer)
{
  OT::hb_ot_apply_context_t c (0, font, buffer);
  for (unsigned int i = 0; i < fallback_plan->num_lookups; i++)
    if (fallback_plan->lookup_array[i]) {
      c.set_lookup_mask (fallback_plan->mask_array[i]);
      hb_ot_layout_substitute_lookup (&c,
				      *fallback_plan->lookup_array[i],
				      fallback_plan->accel_array[i]);
    }
}


#endif /* HB_OT_SHAPE_COMPLEX_ARABIC_FALLBACK_HH */
