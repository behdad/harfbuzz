/*
 * Copyright © 2026  Behdad Esfahbod
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

/* COLR interpolates the colors of a gradient with alpha multiplied
 * into them.  SVG and PDF viewers interpolate the color and the alpha
 * of each stop as written.  Check that the stops that are written make
 * the second come to the first. */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hb.h>
#include <hb-vector.h>

#define MAX_STOPS 256

typedef struct
{
  const hb_color_stop_t *stops;
  unsigned count;
  hb_paint_extend_t extend;
} line_t;

static unsigned
gradient_color_stops (hb_color_line_t *cl, void *cl_data,
		      unsigned start, unsigned *count,
		      hb_color_stop_t *stops, void *user_data)
{
  const line_t *line = (const line_t *) cl_data;
  if (count && stops)
  {
    unsigned n = 0;
    for (; n < *count && start + n < line->count; n++)
      stops[n] = line->stops[start + n];
    *count = n;
  }
  return line->count;
}

static hb_paint_extend_t
gradient_extend (hb_color_line_t *cl, void *cl_data, void *user_data)
{ return ((const line_t *) cl_data)->extend; }

/* Paints a linear gradient with the stops of @line, and returns the
 * document as a string. */
static char *
render (hb_vector_format_t format, line_t *line)
{
  hb_vector_paint_t *paint = hb_vector_paint_create_or_fail (format);
  assert (paint);
  hb_vector_extents_t extents = {0, 0, 100, 100};
  hb_vector_paint_set_extents (paint, &extents);
  hb_paint_funcs_t *funcs = hb_vector_paint_get_funcs (paint);

  hb_color_line_t color_line = {0};
  color_line.data = line;
  color_line.get_color_stops = gradient_color_stops;
  color_line.get_extend = gradient_extend;

  hb_paint_push_clip_rectangle (funcs, paint, 0, 0, 100, 100);
  hb_paint_linear_gradient (funcs, paint, &color_line, 0, 0, 100, 0, 0, 100);
  hb_paint_pop_clip (funcs, paint);

  hb_blob_t *blob = hb_vector_paint_render (paint);
  assert (blob);
  unsigned len;
  const char *data = hb_blob_get_data (blob, &len);
  char *doc = (char *) malloc (len + 1);
  assert (doc);
  memcpy (doc, data, len);
  doc[len] = '\0';
  hb_blob_destroy (blob);
  hb_vector_paint_destroy (paint);
  return doc;
}

/* A stop as a viewer has it: red, green, blue and alpha, each 0 to 1. */
typedef struct
{
  float offset;
  float c[4];
} stop_t;

static unsigned
parse_svg_stops (const char *doc, stop_t *out)
{
  unsigned n = 0;
  for (const char *p = doc; (p = strstr (p, "<stop offset=\"")); p++)
  {
    unsigned r, g, b;
    float opacity = 1.f;
    assert (n < MAX_STOPS);
    assert (4 == sscanf (p, "<stop offset=\"%f\" stop-color=\"rgb(%u,%u,%u)\"",
			 &out[n].offset, &r, &g, &b));
    const char *end = strchr (p, '>');
    const char *o = strstr (p, "stop-opacity=\"");
    if (o && o < end)
      assert (1 == sscanf (o, "stop-opacity=\"%f\"", &opacity));
    out[n].c[0] = r / 255.f;
    out[n].c[1] = g / 255.f;
    out[n].c[2] = b / 255.f;
    out[n].c[3] = opacity;
    n++;
  }
  return n;
}

/* The premultiplied color at @t of a line of @n stops, interpolating
 * the colors premultiplied if @premultiplied, and as they are if not. */
static void
color_at (const stop_t *stops, unsigned n, float t, int premultiplied, float *out)
{
  /* The last segment that starts at or before t. */
  unsigned i = 0;
  while (i + 2 < n && stops[i + 1].offset <= t)
    i++;
  const stop_t *s0 = &stops[i], *s1 = &stops[i + 1];
  float u = s1->offset > s0->offset ? (t - s0->offset) / (s1->offset - s0->offset) : 0.f;
  float a = s0->c[3] + u * (s1->c[3] - s0->c[3]);
  for (unsigned k = 0; k < 3; k++)
    if (premultiplied)
      out[k] = s0->c[k] * s0->c[3] + u * (s1->c[k] * s1->c[3] - s0->c[k] * s0->c[3]);
    else
      out[k] = (s0->c[k] + u * (s1->c[k] - s0->c[k])) * a;
  out[3] = a;
}

/* Renders the stops, and checks what a viewer makes of what is written
 * against what COLR makes of the stops.  Returns how many stops were
 * written. */
static unsigned
check (const hb_color_stop_t *stops, unsigned count)
{
  line_t line = {stops, count, HB_PAINT_EXTEND_PAD};

  stop_t given[MAX_STOPS];
  for (unsigned i = 0; i < count; i++)
  {
    given[i].offset = stops[i].offset;
    given[i].c[0] = hb_color_get_red (stops[i].color) / 255.f;
    given[i].c[1] = hb_color_get_green (stops[i].color) / 255.f;
    given[i].c[2] = hb_color_get_blue (stops[i].color) / 255.f;
    given[i].c[3] = hb_color_get_alpha (stops[i].color) / 255.f;
  }

  char *svg = render (HB_VECTOR_FORMAT_SVG, &line);
  stop_t written[MAX_STOPS];
  unsigned n = parse_svg_stops (svg, written);
  assert (n >= 2);
  free (svg);

  for (unsigned i = 0; i < 1000; i++)
  {
    float t = (i + .5f) / 1000.f;
    float want[4], got[4];
    color_at (given, count, t, 1, want);
    color_at (written, n, t, 0, got);
    for (unsigned k = 0; k < 4; k++)
      assert (fabsf (want[k] - got[k]) < 4.f / 255.f);
  }

  /* PDF writes a color function and, with any alpha, an alpha function
   * for each pair of the same stops. */
  char *pdf = render (HB_VECTOR_FORMAT_PDF, &line);
  unsigned functions = 0;
  for (const char *p = pdf; (p = strstr (p, "/FunctionType 2")); p++)
    functions++;
  int translucent = 0;
  for (unsigned i = 0; i < count; i++)
    if (hb_color_get_alpha (stops[i].color) != 255)
      translucent = 1;
  assert (functions == (n - 1) * (translucent ? 2 : 1));
  free (pdf);

  return n;
}

/* A sweep gradient is written as wedges around its center, a limited
 * number of them.  Check that the stops added for a viewer to come to
 * COLR's colors are not so many that the wedges run out before they
 * are all the way around. */

/* Directions from the center of a sweep, a tenth of a degree apart. */
#define DIRECTIONS 3600

/* Paints a sweep gradient around the origin that repeats the stops of
 * @line every @period radians, and returns the document, which is @len
 * long. */
static char *
render_sweep (hb_vector_format_t format, line_t *line, float period, unsigned *len)
{
  hb_vector_paint_t *paint = hb_vector_paint_create_or_fail (format);
  assert (paint);
  hb_vector_extents_t extents = {-100, -100, 200, 200};
  hb_vector_paint_set_extents (paint, &extents);
  hb_paint_funcs_t *funcs = hb_vector_paint_get_funcs (paint);

  hb_color_line_t color_line = {0};
  color_line.data = line;
  color_line.get_color_stops = gradient_color_stops;
  color_line.get_extend = gradient_extend;

  hb_paint_push_clip_rectangle (funcs, paint, -100, -100, 100, 100);
  hb_paint_sweep_gradient (funcs, paint, &color_line, 0, 0, 0, period);
  hb_paint_pop_clip (funcs, paint);

  hb_blob_t *blob = hb_vector_paint_render (paint);
  assert (blob);
  const char *data = hb_blob_get_data (blob, len);
  char *doc = (char *) malloc (*len + 1);
  assert (doc);
  memcpy (doc, data, *len);
  doc[*len] = '\0';
  hb_blob_destroy (blob);
  hb_vector_paint_destroy (paint);
  return doc;
}

static const char *
find (const char *data, unsigned len, const char *needle)
{
  unsigned n = (unsigned) strlen (needle);
  for (unsigned i = 0; i + n <= len; i++)
    if (!memcmp (data + i, needle, n))
      return data + i;
  return NULL;
}

/* Marks the directions from (@cx, @cy) that a wedge covers: those
 * between the @n points at @xy, which are within half a turn. */
static void
mark_directions (unsigned char *seen, float cx, float cy, const float *xy, unsigned n)
{
  const double per_radian = DIRECTIONS / 2 / 3.14159265358979323846;
  double first = 0, lo = 0, hi = 0;
  int found = 0;
  for (unsigned i = 0; i < n; i++)
  {
    double dx = xy[2 * i] - cx, dy = xy[2 * i + 1] - cy;
    if (dx * dx + dy * dy < 1.)
      continue;
    double a = atan2 (dy, dx) * per_radian;
    if (!found)
    {
      found = 1;
      first = a;
      continue;
    }
    double d = a - first;
    while (d > DIRECTIONS / 2) d -= DIRECTIONS;
    while (d < -DIRECTIONS / 2) d += DIRECTIONS;
    if (d < lo) lo = d;
    if (d > hi) hi = d;
  }
  if (!found)
    return;

  /* A direction is in the middle of its tenth of a degree.  The points
   * are rounded as they are written: a hundredth of a degree of slack. */
  long from = (long) ceil (first + lo - 0.5 - 0.1);
  long to = (long) floor (first + hi - 0.5 + 0.1);
  for (long k = from; k <= to; k++)
    seen[((k % DIRECTIONS) + DIRECTIONS) % DIRECTIONS] = 1;
}

/* Marks the directions that the wedges of an SVG sweep cover, each a
 * path from the center, and returns how many wedges there are. */
static unsigned
svg_sweep_directions (const char *doc, unsigned len, unsigned char *seen)
{
  static const char start[] = "<path d=\"M";
  const unsigned start_len = sizeof (start) - 1;
  unsigned wedges = 0;
  for (const char *p = doc; p + start_len <= doc + len; p++)
  {
    if (*p != '<' || memcmp (p, start, start_len))
      continue;
    p += start_len;

    float xy[2 * 8];
    unsigned n = 0;
    while (*p && *p != 'Z' && *p != '"')
    {
      if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
      {
	char *end;
	assert (n < 2 * 8);
	xy[n++] = strtof (p, &end);
	assert (end > p);
	p = end;
      }
      else
	p++;
    }
    assert (n >= 6 && n % 2 == 0);
    mark_directions (seen, xy[0], xy[1], xy + 2, n / 2 - 1);
    wedges++;
  }
  return wedges;
}

/* The same for a PDF sweep, a mesh of patches: a flag, twelve points
 * of two 16-bit coordinates, and four colors of three bytes.  The
 * first point is the center, and the fourth to the seventh are the
 * far side. */
static unsigned
pdf_sweep_directions (const char *doc, unsigned len, unsigned char *seen)
{
  const char *shading = find (doc, len, "/ShadingType 6 /ColorSpace /DeviceRGB");
  assert (shading);
  unsigned rest = len - (unsigned) (shading - doc);

  float lo[2], hi[2];
  const char *decode = find (shading, rest, "/Decode [");
  assert (decode);
  assert (4 == sscanf (decode, "/Decode [%f %f %f %f", &lo[0], &hi[0], &lo[1], &hi[1]));
  unsigned length;
  const char *l = find (shading, rest, "/Length ");
  assert (l);
  assert (1 == sscanf (l, "/Length %u", &length));
  const char *stream = find (shading, rest, "stream\n");
  assert (stream);
  const unsigned char *p = (const unsigned char *) stream + strlen ("stream\n");
  assert ((const char *) p + length <= doc + len);

  const unsigned patch = 1 + 12 * 4 + 4 * 3;
  assert (length % patch == 0);
  for (unsigned i = 0; i < length / patch; i++, p += patch)
  {
    float xy[2 * 12];
    for (unsigned k = 0; k < 2 * 12; k++)
      xy[k] = lo[k & 1] + (float) ((p[1 + 2 * k] << 8) | p[2 + 2 * k]) / 65535.f * (hi[k & 1] - lo[k & 1]);
    mark_directions (seen, xy[0], xy[1], xy + 2 * 3, 4);
  }
  return length / patch;
}

static void
check_sweep (void)
{
  /* From red to a quarter of blue, every degree.  Sixteen stretches
   * between the two, 360 times around, would be more wedges than there
   * are to a sweep, and the last quadrant would be left unpainted.
   * There are fewer stretches instead: still several to a period, and
   * wedges in every direction. */
  hb_color_stop_t stops[] = {
    {0.f, 0, HB_COLOR (0, 0, 255, 255)},
    {1.f, 0, HB_COLOR (255, 0, 0, 64)},
  };
  static const hb_vector_format_t formats[] = {HB_VECTOR_FORMAT_SVG, HB_VECTOR_FORMAT_PDF};
  static const hb_paint_extend_t extends[] = {HB_PAINT_EXTEND_REPEAT, HB_PAINT_EXTEND_REFLECT};
  const float degree = 3.14159265358979323846f / 180.f;

  for (unsigned f = 0; f < 2; f++)
    for (unsigned e = 0; e < 2; e++)
    {
      line_t line = {stops, 2, extends[e]};
      unsigned len;
      char *doc = render_sweep (formats[f], &line, degree, &len);

      static unsigned char seen[DIRECTIONS];
      memset (seen, 0, sizeof (seen));
      unsigned wedges = formats[f] == HB_VECTOR_FORMAT_SVG
			? svg_sweep_directions (doc, len, seen)
			: pdf_sweep_directions (doc, len, seen);
      assert (wedges >= 4 * 360);
      for (unsigned i = 0; i < DIRECTIONS; i++)
	assert (seen[i]);
      free (doc);
    }
}

int
main (void)
{
  /* Fading out: the transparent stop keeps the color it fades from. */
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (0, 0, 255, 255)},
      {1.f, 0, HB_COLOR (0, 0, 0, 0)},
    };
    assert (check (stops, 2) == 2);
  }

  /* Through transparent: it has one color on each side. */
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (0, 0, 255, 255)},
      {.5f, 0, HB_COLOR (0, 255, 0, 0)},
      {1.f, 0, HB_COLOR (255, 0, 0, 255)},
    };
    assert (check (stops, 3) == 4);
  }

  /* To a translucent color: stops are added along the way. */
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (0, 165, 255, 255)},
      {1.f, 0, HB_COLOR (0, 0, 0, 77)},
    };
    assert (check (stops, 2) > 2);
  }
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (255, 255, 255, 10)},
      {.3f, 0, HB_COLOR (0, 0, 0, 255)},
      {1.f, 0, HB_COLOR (0, 255, 0, 128)},
    };
    assert (check (stops, 3) > 3);
  }

  /* Nothing to do where alpha does not change. */
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (0, 0, 255, 128)},
      {.5f, 0, HB_COLOR (0, 255, 0, 128)},
      {1.f, 0, HB_COLOR (255, 0, 0, 128)},
    };
    assert (check (stops, 3) == 3);
  }
  {
    hb_color_stop_t stops[] = {
      {0.f, 0, HB_COLOR (0, 0, 255, 255)},
      {1.f, 0, HB_COLOR (255, 0, 0, 255)},
    };
    assert (check (stops, 2) == 2);
  }

  check_sweep ();

  return 0;
}
