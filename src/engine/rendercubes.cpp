#include "../include/cube.h"

vertex *verts = NULL;
int curvert;
int curmaxverts = 10000;

void setarraypointers() {
  glVertexPointer(3, GL_FLOAT, sizeof(vertex), &verts[0].x);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &verts[0].r);
  glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &verts[0].u);
};

void reallocv() {
  verts = (vertex *)realloc(verts, (curmaxverts *= 2) * sizeof(vertex));
  curmaxverts -= 10;
  if (!verts)
    fatal("no vertex memory!");
  setarraypointers();
};

#define vertcheck()                                                            \
  {                                                                            \
    if (curvert >= curmaxverts)                                                \
      reallocv();                                                              \
  }

#define vertf(v1, v2, v3, ls, t1, t2)                                          \
  {                                                                            \
    vertex &v = verts[curvert++];                                              \
    v.u = t1;                                                                  \
    v.v = t2;                                                                  \
    v.x = v1;                                                                  \
    v.y = v2;                                                                  \
    v.z = v3;                                                                  \
    v.r = ls->r;                                                               \
    v.g = ls->g;                                                               \
    v.b = ls->b;                                                               \
    v.a = 255;                                                                 \
  };

#define vert(v1, v2, v3, ls, t1, t2)                                           \
  { vertf((float)(v1), (float)(v2), (float)(v3), ls, t1, t2); }

int nquads;
const float TEXTURESCALE = 128.0f;
bool floorstrip = false, deltastrip = false;
int oh, oy, ox, ogltex; // the o* vars are used by the stripification
int ol3r, ol3g, ol3b, ol4r, ol4g, ol4b;
int firstindex;
bool showm = false;

void showmip() { showm = !showm; };

void mipstats(int a, int b, int c) {
  if (showm)
    conoutf("1x1/2x2/4x4: %d / %d / %d", a, b, c);
};

COMMAND(showmip, ARG_NONE);
static int __ad_showmip = (addcommanddetail("showmip", "Toggles mip level display"), 0);

#define stripend()                                                             \
  {                                                                            \
    if (floorstrip || deltastrip) {                                            \
      addstrip(ogltex, firstindex, curvert - firstindex);                      \
      floorstrip = deltastrip = false;                                         \
    };                                                                         \
  };
void finishstrips() { stripend(); };

sqr sbright, sdark;
VAR(lighterror, 1, 8, 100);
static int __ad_lighterror = (addcommanddetail("lighterror", "Maximum lighting error tolerance"), 0);

void render_flat(int wtex, int x, int y, int size, int h, sqr *l1, sqr *l2,
                 sqr *l3, sqr *l4, bool isceil) // floor/ceil quads
{
  vertcheck();
  if (showm) {
    l3 = l1 = &sbright;
    l4 = l2 = &sdark;
  };

  int sx, sy;
  int gltex = lookuptexture(wtex, sx, sy);
  float xf = TEXTURESCALE / sx;
  float yf = TEXTURESCALE / sy;
  float xs = size * xf;
  float ys = size * yf;
  float xo = xf * x;
  float yo = yf * y;

  bool first =
      !floorstrip || y != oy + size || ogltex != gltex || h != oh || x != ox;

  if (first) // start strip here
  {
    stripend();
    firstindex = curvert;
    ogltex = gltex;
    oh = h;
    ox = x;
    floorstrip = true;
    if (isceil) {
      vert(x + size, h, y, l2, xo + xs, yo);
      vert(x, h, y, l1, xo, yo);
    } else {
      vert(x, h, y, l1, xo, yo);
      vert(x + size, h, y, l2, xo + xs, yo);
    };
    ol3r = l1->r;
    ol3g = l1->g;
    ol3b = l1->b;
    ol4r = l2->r;
    ol4g = l2->g;
    ol4b = l2->b;
  } else // continue strip
  {
    int lighterr = lighterror * 2;
    if ((abs(ol3r - l3->r) < lighterr &&
         abs(ol4r - l4->r) <
             lighterr // skip vertices if light values are close enough
         && abs(ol3g - l3->g) < lighterr && abs(ol4g - l4->g) < lighterr &&
         abs(ol3b - l3->b) < lighterr && abs(ol4b - l4->b) < lighterr) ||
        !wtex) {
      curvert -= 2;
      nquads--;
    } else {
      uchar *p3 = (uchar *)(&verts[curvert - 1].r);
      ol3r = p3[0];
      ol3g = p3[1];
      ol3b = p3[2];
      uchar *p4 = (uchar *)(&verts[curvert - 2].r);
      ol4r = p4[0];
      ol4g = p4[1];
      ol4b = p4[2];
    };
  };

  if (isceil) {
    vert(x + size, h, y + size, l3, xo + xs, yo + ys);
    vert(x, h, y + size, l4, xo, yo + ys);
  } else {
    vert(x, h, y + size, l4, xo, yo + ys);
    vert(x + size, h, y + size, l3, xo + xs, yo + ys);
  };

  oy = y;
  nquads++;
};

void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2,
                      float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4,
                      bool isceil) // floor/ceil quads on a slope
{
  vertcheck();
  if (showm) {
    l3 = l1 = &sbright;
    l4 = l2 = &sdark;
  };

  int sx, sy;
  int gltex = lookuptexture(wtex, sx, sy);
  float xf = TEXTURESCALE / sx;
  float yf = TEXTURESCALE / sy;
  float xs = size * xf;
  float ys = size * yf;
  float xo = xf * x;
  float yo = yf * y;

  bool first = !deltastrip || y != oy + size || ogltex != gltex || x != ox;

  if (first) {
    stripend();
    firstindex = curvert;
    ogltex = gltex;
    ox = x;
    deltastrip = true;
    if (isceil) {
      vertf((float)x + size, h2, (float)y, l2, xo + xs, yo);
      vertf((float)x, h1, (float)y, l1, xo, yo);
    } else {
      vertf((float)x, h1, (float)y, l1, xo, yo);
      vertf((float)x + size, h2, (float)y, l2, xo + xs, yo);
    };
    ol3r = l1->r;
    ol3g = l1->g;
    ol3b = l1->b;
    ol4r = l2->r;
    ol4g = l2->g;
    ol4b = l2->b;
  };

  if (isceil) {
    vertf((float)x + size, h3, (float)y + size, l3, xo + xs, yo + ys);
    vertf((float)x, h4, (float)y + size, l4, xo, yo + ys);
  } else {
    vertf((float)x, h4, (float)y + size, l4, xo, yo + ys);
    vertf((float)x + size, h3, (float)y + size, l3, xo + xs, yo + ys);
  };

  oy = y;
  nquads++;
};

void render_2tris(sqr *h, sqr *s, int x1, int y1, int x2, int y2, int x3,
                  int y3, sqr *l1, sqr *l2,
                  sqr *l3) // floor/ceil tris on a corner cube
{
  stripend();
  vertcheck();

  int sx, sy;
  int gltex = lookuptexture(h->ftex, sx, sy);
  float xf = TEXTURESCALE / sx;
  float yf = TEXTURESCALE / sy;

  vertf((float)x1, h->floor, (float)y1, l1, xf * x1, yf * y1);
  vertf((float)x2, h->floor, (float)y2, l2, xf * x2, yf * y2);
  vertf((float)x3, h->floor, (float)y3, l3, xf * x3, yf * y3);
  addstrip(gltex, curvert - 3, 3);

  gltex = lookuptexture(h->ctex, sx, sy);
  xf = TEXTURESCALE / sx;
  yf = TEXTURESCALE / sy;

  vertf((float)x3, h->ceil, (float)y3, l3, xf * x3, yf * y3);
  vertf((float)x2, h->ceil, (float)y2, l2, xf * x2, yf * y2);
  vertf((float)x1, h->ceil, (float)y1, l1, xf * x1, yf * y1);
  addstrip(gltex, curvert - 3, 3);
  nquads++;
};

void render_tris(int x, int y, int size, bool topleft, sqr *h1, sqr *h2, sqr *s,
                 sqr *t, sqr *u, sqr *v) {
  if (topleft) {
    if (h1)
      render_2tris(h1, s, x + size, y + size, x, y + size, x, y, u, v, s);
    if (h2)
      render_2tris(h2, s, x, y, x + size, y, x + size, y + size, s, t, v);
  } else {
    if (h1)
      render_2tris(h1, s, x, y, x + size, y, x, y + size, s, t, u);
    if (h2)
      render_2tris(h2, s, x + size, y, x + size, y + size, x, y + size, t, u,
                   v);
  };
};

void render_square(int wtex, float floor1, float floor2, float ceil1,
                   float ceil2, int x1, int y1, int x2, int y2, int size,
                   sqr *l1, sqr *l2, bool flip) // wall quads
{
  stripend();
  vertcheck();
  if (showm) {
    l1 = &sbright;
    l2 = &sdark;
  };

  int sx, sy;
  int gltex = lookuptexture(wtex, sx, sy);
  float xf = TEXTURESCALE / sx;
  float yf = TEXTURESCALE / sy;
  float xs = size * xf;
  float xo = xf * (x1 == x2 ? min(y1, y2) : min(x1, x2));

  if (!flip) {
    vertf((float)x2, ceil2, (float)y2, l2, xo + xs, -yf * ceil2);
    vertf((float)x1, ceil1, (float)y1, l1, xo, -yf * ceil1);
    vertf((float)x2, floor2, (float)y2, l2, xo + xs, -floor2 * yf);
    vertf((float)x1, floor1, (float)y1, l1, xo, -floor1 * yf);
  } else {
    vertf((float)x1, ceil1, (float)y1, l1, xo, -yf * ceil1);
    vertf((float)x2, ceil2, (float)y2, l2, xo + xs, -yf * ceil2);
    vertf((float)x1, floor1, (float)y1, l1, xo, -floor1 * yf);
    vertf((float)x2, floor2, (float)y2, l2, xo + xs, -floor2 * yf);
  };

  nquads++;
  addstrip(gltex, curvert - 4, 4);
};

int wx1, wy1, wx2, wy2;
int lx1, ly1, lx2, ly2;

VAR(watersubdiv, 1, 4, 64);
static int __ad_watersubdiv = (addcommanddetail("watersubdiv", "Water surface subdivision level"), 0);
VARF(waterlevel, -128, -128, 127,
     if (!noteditmode()) { hdr.waterlevel = waterlevel; if (waterlevel > -128) hdr.lavalevel = -100000; });
static int __ad_waterlevel = (addcommanddetail("waterlevel", "Water level height"), 0);
VARP(waterspec, 0, 40, 255);
static int __ad_waterspec = (addcommanddetail("waterspec", "Water specular reflection amount"), 0);
VARP(wateralpha, 0, 200, 255);
static int __ad_wateralpha = (addcommanddetail("wateralpha", "Water transparency alpha"), 0);
VARP(waterr, 0, 80, 255);
static int __ad_waterr = (addcommanddetail("waterr", "Water red color component"), 0);
VARP(waterg, 0, 160, 255);
static int __ad_waterg = (addcommanddetail("waterg", "Water green color component"), 0);
VARP(waterb, 0, 200, 255);
static int __ad_waterb = (addcommanddetail("waterb", "Water blue color component"), 0);

VAR(lavasubdiv, 1, 4, 64);
static int __ad_lavasubdiv = (addcommanddetail("lavasubdiv", "Lava surface subdivision level"), 0);
VARF(lavalevel, -128, -128, 127,
     if (!noteditmode()) { hdr.lavalevel = lavalevel; if (lavalevel > -128) hdr.waterlevel = -100000; });
static int __ad_lavalevel = (addcommanddetail("lavalevel", "Lava level height"), 0);
VARP(lavaspec, 0, 10, 255);
static int __ad_lavaspec = (addcommanddetail("lavaspec", "Lava specular reflection amount"), 0);
VARP(lavaalpha, 0, 200, 255);
static int __ad_lavaalpha = (addcommanddetail("lavaalpha", "Lava transparency alpha"), 0);
VARP(lavar, 0, 255, 255);
static int __ad_lavar = (addcommanddetail("lavar", "Lava red color component"), 0);
VARP(lavag, 0, 40, 255);
static int __ad_lavag = (addcommanddetail("lavag", "Lava green color component"), 0);
VARP(lavab, 0, 0, 255);
static int __ad_lavab = (addcommanddetail("lavab", "Lava blue color component"), 0);

inline void vertw(int v1, float v2, int v3, sqr *c, float tu, float tv,
                  float wavetime, float shimmertime) {
  vertcheck();
  float wave = sin(v1 * v3 * 0.1f + wavetime) * 0.2f +
               sin(v1 * 0.13f + v3 * 0.07f + wavetime * 1.4f + 1.3f) * 0.12f;
  vertf((float)v1, v2 - wave, (float)v3, c, tu, tv);
  float av = sin(v1 * 0.5f + v3 * 0.3f + shimmertime) * 0.2f + 0.8f;
  verts[curvert - 1].a = (uchar)(wateralpha * av);
};

inline void vertlava(int v1, float v2, int v3, sqr *c, float tu, float tv,
                     float wavetime, float shimmertime) {
  vertcheck();
  float wave = sin(v1 * v3 * 0.1f + wavetime) * 0.2f +
               sin(v1 * 0.13f + v3 * 0.07f + wavetime * 1.4f + 1.3f) * 0.12f;
  vertf((float)v1, v2 - wave, (float)v3, c, tu, tv);
  float av = sin(v1 * 0.5f + v3 * 0.3f + shimmertime) * 0.2f + 0.8f;
  verts[curvert - 1].a = (uchar)(lavaalpha * av);
};

inline float dx(float x) {
  return x + (float)sin(x * 2 + lastmillis / 1000.0f) * 0.04f;
};
inline float dy(float x) {
  return x + (float)sin(x * 2 + lastmillis / 900.0f + PI / 5) * 0.05f;
};

// renders water for bounding rect area that contains water... simple but very
// inefficient

int renderwater(float hf) {
  if (wx1 < 0)
    return nquads;

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  int sx, sy;
  glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_LIQUID, sx, sy));

  wx1 &= ~(watersubdiv - 1);
  wy1 &= ~(watersubdiv - 1);

  float xf = TEXTURESCALE / sx;
  float yf = TEXTURESCALE / sy;
  float xs = watersubdiv * xf;
  float ys = watersubdiv * yf;
  float wavetime = lastmillis / 300.0f;
  float scroll = lastmillis / 4000.0f;
  float shimmertime = lastmillis / 1000.0f;

  sqr dl;

  for (int xx = wx1; xx < wx2; xx += watersubdiv) {
    for (int yy = wy1; yy < wy2; yy += watersubdiv) {
      float xo = xf * (xx + scroll);
      float yo = yf * (yy + scroll);

      float spec =
          sin(xx * 0.3f + yy * 0.5f + shimmertime * 1.5f) * 0.5f + 0.5f;
      int sv = (int)(spec * waterspec);
      dl.r = (uchar)min(255, waterr + sv);
      dl.g = (uchar)min(255, waterg + sv);
      dl.b = (uchar)min(255, waterb + sv);

      if (yy == wy1) {
        vertw(xx, hf, yy, &dl, dx(xo), dy(yo), wavetime, shimmertime);
        vertw(xx + watersubdiv, hf, yy, &dl, dx(xo + xs), dy(yo), wavetime,
              shimmertime);
      };
      vertw(xx, hf, yy + watersubdiv, &dl, dx(xo), dy(yo + ys), wavetime,
            shimmertime);
      vertw(xx + watersubdiv, hf, yy + watersubdiv, &dl, dx(xo + xs),
            dy(yo + ys), wavetime, shimmertime);
    };
    int n = (wy2 - wy1 - 1) / watersubdiv;
    nquads += n;
    n = (n + 2) * 2;
    glDrawArrays(GL_TRIANGLE_STRIP, curvert -= n, n);
  };

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);

  if (wx1 >= 0) {
    int area = (wx2 - wx1) * (wy2 - wy1);
    int nb = 1 + area / 2000;
    if (nb > 5)
      nb = 5;
    loopi(nb) {
      float bx = wx1 + (float)(rnd(wx2 - wx1)) + (float)rnd(100) / 100.0f;
      float by = wy1 + (float)(rnd(wy2 - wy1)) + (float)rnd(100) / 100.0f;
      vec bo = {bx, by, hf};
      vec bd = {(float)(rnd(21) - 10), (float)(rnd(21) - 10),
                (float)(rnd(8) + 2)};
      newparticlecol(bo, bd, rnd(600) + 600, 10, (uchar)(rnd(40) + 100),
                     (uchar)(rnd(30) + 190), 255);
    };
  };

  return nquads;
};

void addwaterquad(int x, int y, int size) {
  int x2 = x + size;
  int y2 = y + size;
  if (wx1 < 0) {
    wx1 = x;
    wy1 = y;
    wx2 = x2;
    wy2 = y2;
  } else {
    if (x < wx1)
      wx1 = x;
    if (y < wy1)
      wy1 = y;
    if (x2 > wx2)
      wx2 = x2;
    if (y2 > wy2)
      wy2 = y2;
  };
};

int renderlava(float hf) {
  if (lx1 < 0)
    return nquads;

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);

  lx1 &= ~(lavasubdiv - 1);
  ly1 &= ~(lavasubdiv - 1);

  float xf = 1.0f;
  float yf = 1.0f;
  float xs = lavasubdiv * xf;
  float ys = lavasubdiv * yf;
  float wavetime = lastmillis / 300.0f;
  float scroll = lastmillis / 4000.0f;
  float shimmertime = lastmillis / 1000.0f;

  sqr dl;

  for (int xx = lx1; xx < lx2; xx += lavasubdiv) {
    for (int yy = ly1; yy < ly2; yy += lavasubdiv) {
      float xo = xf * (xx + scroll);
      float yo = yf * (yy + scroll);

      float spec =
          sin(xx * 0.3f + yy * 0.5f + shimmertime * 1.5f) * 0.5f + 0.5f;
      int sv = (int)(spec * lavaspec);
      dl.r = (uchar)min(255, lavar + sv);
      dl.g = (uchar)min(255, lavag + sv * 3 / 4);
      dl.b = (uchar)min(255, lavab + sv / 2);

      if (yy == ly1) {
        vertlava(xx, hf, yy, &dl, dx(xo), dy(yo), wavetime, shimmertime);
        vertlava(xx + lavasubdiv, hf, yy, &dl, dx(xo + xs), dy(yo), wavetime,
                 shimmertime);
      };
      vertlava(xx, hf, yy + lavasubdiv, &dl, dx(xo), dy(yo + ys), wavetime,
               shimmertime);
      vertlava(xx + lavasubdiv, hf, yy + lavasubdiv, &dl, dx(xo + xs),
               dy(yo + ys), wavetime, shimmertime);
    };
    int n = (ly2 - ly1 - 1) / lavasubdiv;
    nquads += n;
    n = (n + 2) * 2;
    glDrawArrays(GL_TRIANGLE_STRIP, curvert -= n, n);
  };

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);

  if (lx1 >= 0) {
    int area = (lx2 - lx1) * (ly2 - ly1);
    int np = 1 + area / 2000;
    if (np > 5)
      np = 5;
    loopi(np) {
      float px = lx1 + (float)(rnd(lx2 - lx1)) + (float)rnd(100) / 100.0f;
      float py = ly1 + (float)(rnd(ly2 - ly1)) + (float)rnd(100) / 100.0f;
      vec po = {px, py, hf};
      vec pd = {(float)(rnd(21) - 10), (float)(rnd(21) - 10),
                (float)(rnd(8) + 2)};
      newparticlecol(po, pd, rnd(600) + 600, 10, (uchar)(rnd(100) + 155),
                     (uchar)(rnd(40) + 30), (uchar)(rnd(20) + 10));
    };
  };

  return nquads;
};

void addlavaquad(int x, int y, int size) {
  int x2 = x + size;
  int y2 = y + size;
  if (lx1 < 0) {
    lx1 = x;
    ly1 = y;
    lx2 = x2;
    ly2 = y2;
  } else {
    if (x < lx1)
      lx1 = x;
    if (y < ly1)
      ly1 = y;
    if (x2 > lx2)
      lx2 = x2;
    if (y2 > ly2)
      ly2 = y2;
  };
};

void resetcubes() {
  if (!verts)
    reallocv();
  floorstrip = deltastrip = false;
  wx1 = -1;
  lx1 = -1;
  nquads = 0;
  sbright.r = sbright.g = sbright.b = 255;
  sdark.r = sdark.g = sdark.b = 0;
};
