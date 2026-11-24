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

#ifndef HB_WASM_API_HH
#define HB_WASM_API_HH

#include "hb.hh"

#include "hb-limits.hh"

#include <wasm_export.h>

#define HB_WASM_BEGIN_DECLS namespace hb { namespace wasm {
#define HB_WASM_END_DECLS }}

#define HB_WASM_API(ret_t, name) HB_INTERNAL ret_t name
#define HB_WASM_API_COMPOUND(ret_t, name) HB_INTERNAL void name

#define HB_WASM_EXEC_ENV wasm_exec_env_t exec_env,
#define HB_WASM_EXEC_ENV_COMPOUND wasm_exec_env_t exec_env, ptr_t() retptr,

#define ptr_t(type_t) uint32_t
#define ptr_d(type_t, name) uint32_t name##ptr

#include "hb-wasm-api.h"

#undef HB_WASM_BEGIN_DECLS
#undef HB_WASM_END_DECLS


enum {
  hb_wasm_ref_type_none,
  hb_wasm_ref_type_face,
  hb_wasm_ref_type_font,
  hb_wasm_ref_type_buffer,
};

#define nullref 0

/* A handle: the number a module holds in place of one of our objects. */
struct hb_wasm_ref_t
{
  int cmp (uint32_t o) const
  { return o < ref ? -1 : o > ref ? +1 : 0; }

  uint32_t ref;
  uint8_t type;
  /* Whether the module created the object.  The instance then owns it,
   * and destroys it when the handle goes. */
  bool owned;
  void *obj;
};

/* A module instance, and what we know of it.  The instance carries a
 * pointer to this as its custom data. */
struct hb_wasm_shape_plan_t
{
  /* Returns the object that a handle of this instance stands for, or
   * nullptr if there is none of that type. */
  void *ref2obj (uint32_t ref, unsigned type)
  {
    const hb_wasm_ref_t *entry = refs.bsearch (ref);
    return entry && entry->type == type ? entry->obj : nullptr;
  }

  /* Returns the handle that the object has, or nullref. */
  uint32_t find_ref (const void *obj, unsigned type) const
  {
    for (const hb_wasm_ref_t &entry : refs)
      if (entry.obj == obj && entry.type == type)
	return entry.ref;
    return nullref;
  }

  /* Returns a handle for the object, or nullref.  An object that the
   * instance does not own keeps the handle it already has. */
  uint32_t obj2ref (void *obj, unsigned type, bool owned = false)
  {
    if (!owned)
    {
      uint32_t ref = find_ref (obj, type);
      if (ref)
	return ref;
    }

    /* Numbers are not used again, so that a handle whose object is gone
     * names nothing.  They only grow, which keeps the table sorted. */
    if (unlikely (refs.length >= HB_WASM_MAX_HANDLES || last_ref == (uint32_t) -1))
      return nullref;
    hb_wasm_ref_t *entry = refs.push ();
    if (unlikely (refs.in_error ()))
      return nullref;

    entry->ref = ++last_ref;
    entry->type = type;
    entry->owned = owned;
    entry->obj = obj;
    num_owned += owned;
    return entry->ref;
  }

  /* Destroys an object that the instance owns, and its handle with it.
   * Any other handle is left alone. */
  void destroy_ref (uint32_t ref, unsigned type)
  {
    unsigned i;
    if (!refs.bfind (ref, &i))
      return;
    if (!refs.arrayZ[i].owned || refs.arrayZ[i].type != type)
      return;
    destroy_obj (refs.arrayZ[i]);
    refs.remove_ordered (i);
    num_owned--;
  }

  /* Whether the instance is to be retired, well before it could run
   * out of numbers in the middle of a call. */
  bool low_on_refs () const
  { return last_ref > (uint32_t) -1 / 2; }

  /* Drops the handles from the given one on, in the order they were
   * made, and destroys the objects that the instance owns among them. */
  void drop_refs (unsigned start = 0)
  {
    for (unsigned i = start; i < refs.length; i++)
    {
      const hb_wasm_ref_t &entry = refs.arrayZ[i];
      if (!entry.owned)
	continue;
      destroy_obj (entry);
      num_owned--;
    }
    refs.shrink (start);
  }

  static void destroy_obj (const hb_wasm_ref_t &entry)
  {
    if (entry.type == hb_wasm_ref_type_face)
      hb_face_destroy ((hb_face_t *) entry.obj);
    else if (entry.type == hb_wasm_ref_type_font)
      hb_font_destroy ((hb_font_t *) entry.obj);
  }

  wasm_module_inst_t module_inst;
  wasm_exec_env_t exec_env;
  ptr_d(void, wasm_shape_plan);

  /* The handles the module can use.  They are this instance's alone:
   * no number names anything in another instance. */
  hb_sorted_vector_t<hb_wasm_ref_t> refs;
  uint32_t last_ref;
  /* How many faces and fonts the instance owns: those that its module
   * created and has not destroyed. */
  unsigned num_owned;
};

#define HB_WASM_PLAN \
  ((hb_wasm_shape_plan_t *) wasm_runtime_get_custom_data (module_inst))

#define HB_REF2OBJ(obj) \
  hb_##obj##_t *obj = (hb_##obj##_t *) \
    HB_WASM_PLAN->ref2obj (obj##ptr, hb_wasm_ref_type_##obj); \
  HB_STMT_START { \
    if (unlikely (!obj)) \
      obj = hb_##obj##_get_empty (); \
  } HB_STMT_END

#define HB_OBJ2REF(obj) \
  uint32_t obj##ref = HB_WASM_PLAN->obj2ref (obj, hb_wasm_ref_type_##obj)

/* For an object the module has just created. */
#define HB_OBJ2REF_OWNED(obj) \
  uint32_t obj##ref = HB_WASM_PLAN->obj2ref (obj, hb_wasm_ref_type_##obj, true)

#define HB_RETURN_STRUCT(type, name) \
  type *_name_ptr = nullptr; \
  { \
    if (likely (wasm_runtime_validate_app_addr (module_inst, \
						retptr, sizeof (type)))) \
      _name_ptr = (type *) wasm_runtime_addr_app_to_native (module_inst, retptr); \
    if (unlikely (!_name_ptr)) \
      return; \
  } \
  type &name = *_name_ptr

#define HB_PTR_PARAM(type, name) \
  type *name = nullptr; \
  HB_STMT_START { \
    if (likely (wasm_runtime_validate_app_addr (module_inst, \
						name##ptr, sizeof (type)))) \
      name = (type *) wasm_runtime_addr_app_to_native (module_inst, name##ptr); \
  } HB_STMT_END

#define HB_ARRAY_PARAM(type, name, length) \
  type *name = nullptr; \
  HB_STMT_START { \
    if (likely (!hb_unsigned_mul_overflows (length, sizeof (type)) && \
		wasm_runtime_validate_app_addr (module_inst, \
						name##ptr, length * sizeof (type)))) \
      name = (type *) wasm_runtime_addr_app_to_native (module_inst, name##ptr); \
  } HB_STMT_END

#define HB_ARRAY_APP2NATIVE(type, name, length) \
    ((type *) (!hb_unsigned_mul_overflows (length, sizeof (type)) && \
	       validate_app_addr (name, (length) * sizeof (type)) ? \
	       addr_app_to_native (name) : nullptr))


#endif /* HB_WASM_API_HH */
