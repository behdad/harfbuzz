/*
 * Copyright © 2023  Behdad Esfahbod
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

#ifndef HB_WASM_API_FACE_HH
#define HB_WASM_API_FACE_HH

#include "hb-wasm-api.hh"

namespace hb {
namespace wasm {


HB_WASM_API (ptr_t(face_t), face_create) (HB_WASM_EXEC_ENV
					  ptr_d(blob_t, blob),
					  unsigned int index)
{
  HB_PTR_PARAM (blob_t, blob);
  if (unlikely (!blob))
    return nullref;

  char *data = HB_ARRAY_APP2NATIVE (char, blob->data, blob->length);
  if (unlikely (!data))
    return nullref;

  /* The face keeps a copy of the data, outside the module's own memory.
   * Count it against the memory the module may use. */
  auto *plan = HB_WASM_PLAN;
  if (unlikely (!plan ||
		plan->face_bytes > plan->max_memory ||
		blob->length > plan->max_memory - plan->face_bytes))
    return nullref;
  plan->face_bytes += blob->length;

  hb_blob_t *hb_blob = hb_blob_create(
				      data,
				      blob->length,
				      HB_MEMORY_MODE_DUPLICATE,
				      NULL,
				      NULL);

  hb_face_t *face = hb_face_create(hb_blob, index);
  hb_blob_destroy (hb_blob);

  /* The instance owns the face, and destroys it when it goes. */
  HB_OBJ2REF_OWNED (face);
  if (unlikely (!faceref))
    hb_face_destroy (face);

  return faceref;
}

HB_WASM_API (bool_t, face_copy_table) (HB_WASM_EXEC_ENV
				       ptr_d(face_t, face),
				       tag_t table_tag,
				       ptr_d(blob_t, blob))
{
  HB_REF2OBJ (face);
  HB_PTR_PARAM (blob_t, blob);
  if (unlikely (!blob))
    return false;

  hb_unique_ptr_t<hb_blob_t> hb_blob (hb_face_reference_table (face, table_tag));

  unsigned length;
  const char *hb_data = hb_blob_get_data (hb_blob, &length);

  if (length <= blob->length)
  {
    char *data = HB_ARRAY_APP2NATIVE (char, blob->data, length);

    if (unlikely (!data))
    {
      blob->length = 0;
      return false;
    }

    hb_memcpy (data, hb_data, length);
    blob->length = length;

    return true;
  }

  module_free (blob->data);

  blob->length = length;
  blob->data = wasm_runtime_module_dup_data (module_inst, hb_data, length);

  if (blob->length && !blob->data)
  {
    blob->length = 0;
    return false;
  }

  return true;
}

HB_WASM_API (unsigned, face_get_upem) (HB_WASM_EXEC_ENV
				       ptr_d(face_t, face))
{
  HB_REF2OBJ (face);

  return hb_face_get_upem (face);
}

HB_WASM_API (void, face_destroy) (HB_WASM_EXEC_ENV
				  ptr_d(face_t, face))
{
  auto *plan = HB_WASM_PLAN;
  if (likely (plan))
    plan->destroy_ref (faceptr, hb_wasm_ref_type_face);
}


}}

#endif /* HB_WASM_API_FACE_HH */
