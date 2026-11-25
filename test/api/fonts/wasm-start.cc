/* The Wasm module of wasm-start.ttf, which test-wasm.c shapes with.
 * It calls into Harfbuzz while the runtime is still making the
 * instance.  generate-wasm-api-font.py compiles it. */

#define HB_WASM_INTERFACE(ret_t, name) \
  __attribute__((export_name(#name))) ret_t name

#include <hb-wasm-api.h>

static bool ran;
static unsigned upem;
static font_t *font1;
static face_t *face1, *face2;

/* The runtime runs the exported __wasm_call_ctors when it makes the
 * instance, as it does a start function.  No handle names anything
 * yet, and nothing can be created. */
__attribute__((constructor))
static void
early (void)
{
  if (ran)
    return;
  ran = true;

  static char data[16];
  blob_t blob = {sizeof data, data};

  upem = face_get_upem ((face_t *) 1);
  font1 = font_create ((face_t *) 1);
  face1 = face_create (&blob, 0);
  face2 = font_get_face ((font_t *) 2);
  font_destroy ((font_t *) 2);
  face_destroy ((face_t *) 1);
}

bool_t
shape (void *shape_plan, font_t *font, buffer_t *buffer,
       const feature_t *features, uint32_t num_features)
{
  /* The upem is that of the empty face. */
  return ran && upem == 1000 && !font1 && !face1 && !face2;
}
