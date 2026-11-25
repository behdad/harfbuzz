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

  /* Charges the module for work that a host function does for it, in
   * the unit of the instruction limit.  When the allowance of the call
   * is spent, stops the module: the runtime fails the call once the
   * host function returns. */
  bool charge (uint64_t cost)
  {
    if (likely (cost <= work_left))
    {
      work_left -= cost;
      return true;
    }
    work_left = 0;
    wasm_runtime_set_exception (module_inst, "host work limit exceeded");
    return false;
  }

  /* The memory the instance may use, in bytes; the buffer length that
   * was sized for; and how much of it went into faces the module
   * created. */
  uint64_t max_memory;
  unsigned max_buffer_len;
  uint64_t face_bytes;
  /* What host functions may still do for the module in this call. */
  uint64_t work_left;
  /* Not zero while the runtime may be running the module's own
   * allocator for us, in the middle of one of our functions. */
  unsigned allocating;
  /* How many blocks of memory we have allocated in the instance and
   * not freed: what the module had us copy and has not handed back. */
  unsigned num_allocations;
  /* Whether the instance has served a shape() call. */
  bool used;
};

/* The plan of the instance a host function was called from.  There is
 * none yet while the runtime is still making the instance, which is
 * when a module's start function and constructors run.  No handle
 * names anything then, and no object can be given one. */
#define HB_WASM_PLAN \
  ((hb_wasm_shape_plan_t *) wasm_runtime_get_custom_data (module_inst))

static inline uint32_t
_hb_wasm_obj2ref (hb_wasm_shape_plan_t *plan,
		  void *obj, unsigned type, bool owned)
{ return likely (plan) ? plan->obj2ref (obj, type, owned) : nullref; }

/* The runtime counts the instructions a module runs, not what we do on
 * its behalf.  So each host function charges the module: this much for
 * being called, more where the work grows with a size.  The amounts
 * are in instructions, at what the work takes next to the interpreter;
 * they need not be exact, only not so low that a module can keep us
 * busy for long on a small allowance. */
#define HB_WASM_COST_CALL 64
#define HB_WASM_COST_BYTES(n) ((uint64_t) (n) / 8)

/* Every host function passes through here.  It stops a module that
 * calls us from its own allocator, which the runtime runs in the middle
 * of our functions: see HB_MODULE_MALLOC.  And with the limits it takes
 * the charge; an instance that is still being made has no allowance
 * yet, so a module that calls us from its start function is stopped
 * there. */
static inline bool
_hb_wasm_charge (wasm_module_inst_t inst,
		 hb_wasm_shape_plan_t *plan,
		 uint64_t cost HB_UNUSED)
{
  if (unlikely (plan && plan->allocating))
  {
    wasm_runtime_set_exception (inst, "called from the module's allocator");
    return false;
  }
#ifndef HB_NO_WASM_BUDGET
  if (likely (plan))
    return plan->charge (cost);
  wasm_runtime_set_exception (inst, "host work limit exceeded");
  return false;
#else
  return true;
#endif
}

#define HB_WASM_CHARGE_MORE(cost) _hb_wasm_charge (module_inst, HB_WASM_PLAN, cost)
#define HB_WASM_CHARGE(cost) HB_WASM_CHARGE_MORE (HB_WASM_COST_CALL + (uint64_t) (cost))

/* Memory for a module, in the module.  The runtime has a heap of its
 * own in an instance for these, unless the module exports malloc() and
 * free(): then it runs those, in the middle of our function.  Two
 * things follow from that.
 *
 * The module's allocator can grow the module's memory, and a runtime
 * without hardware bounds checks moves the memory when it grows.  So a
 * native pointer into a module's memory is good until the next of
 * these calls, and no longer: take it again after, with
 * HB_PTR_PARAM_AGAIN or HB_ARRAY_APP2NATIVE.
 *
 * And the module's allocator can call our functions, while one of them
 * is half way through with a buffer or a font.  The plan counts the
 * calls in progress, and _hb_wasm_charge() refuses.
 *
 * An instance that is still being made has no plan to count in, and
 * gets no memory from us. */
static inline uint32_t
_hb_wasm_module_malloc (wasm_module_inst_t inst,
			hb_wasm_shape_plan_t *plan,
			uint32_t size)
{
  if (unlikely (!plan))
    return nullref;
  plan->allocating++;
  uint32_t ref = (uint32_t) wasm_runtime_module_malloc (inst, size, nullptr);
  plan->allocating--;
  plan->num_allocations += !!ref;
  return ref;
}

static inline uint32_t
_hb_wasm_module_dup (wasm_module_inst_t inst,
		     hb_wasm_shape_plan_t *plan,
		     const char *data, uint32_t size)
{
  if (unlikely (!plan))
    return nullref;
  plan->allocating++;
  uint32_t ref = (uint32_t) wasm_runtime_module_dup_data (inst, data, size);
  plan->allocating--;
  plan->num_allocations += !!ref;
  return ref;
}

static inline void
_hb_wasm_module_free (wasm_module_inst_t inst,
		      hb_wasm_shape_plan_t *plan,
		      uint32_t ref)
{
  if (unlikely (!plan))
    return;
  plan->allocating++;
  wasm_runtime_module_free (inst, ref);
  plan->allocating--;
  /* A module can have us free what we never allocated, so this is no
   * more than a hint. */
  if (ref && plan->num_allocations)
    plan->num_allocations--;
}

#define HB_MODULE_MALLOC(size) \
  _hb_wasm_module_malloc (module_inst, HB_WASM_PLAN, size)
#define HB_MODULE_DUP(data, size) \
  _hb_wasm_module_dup (module_inst, HB_WASM_PLAN, (const char *) (data), size)
#define HB_MODULE_FREE(ref) \
  _hb_wasm_module_free (module_inst, HB_WASM_PLAN, ref)

#define HB_REF2OBJ(obj) \
  hb_##obj##_t *obj = nullptr; \
  HB_STMT_START { \
    hb_wasm_shape_plan_t *_plan = HB_WASM_PLAN; \
    if (likely (_plan)) \
      obj = (hb_##obj##_t *) \
	_plan->ref2obj (obj##ptr, hb_wasm_ref_type_##obj); \
    if (unlikely (!obj)) \
      obj = hb_##obj##_get_empty (); \
  } HB_STMT_END

#define HB_OBJ2REF(obj) \
  uint32_t obj##ref = _hb_wasm_obj2ref (HB_WASM_PLAN, obj, \
					hb_wasm_ref_type_##obj, false)

/* For an object the module has just created. */
#define HB_OBJ2REF_OWNED(obj) \
  uint32_t obj##ref = _hb_wasm_obj2ref (HB_WASM_PLAN, obj, \
					hb_wasm_ref_type_##obj, true)

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

/* Takes a pointer parameter again, after a call that may have moved
 * the module's memory. */
#define HB_PTR_PARAM_AGAIN(type, name) \
  HB_STMT_START { \
    name = nullptr; \
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
