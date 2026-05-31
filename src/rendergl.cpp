#include "cube.h"

#ifdef DARWIN
#define GL_COMBINE_EXT GL_COMBINE_ARB
#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
#define GL_SOURCE0_RBG_EXT GL_SOURCE0_RGB_ARB
#define GL_SOURCE1_RBG_EXT GL_SOURCE1_RGB_ARB
#define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB
#endif

extern int curvert;

bool hasoverbright = false;

void purgetextures();

GLUquadricObj *qsphere = NULL;
int glmaxtexsize = 256;

void gl_init(int w, int h) {
  glViewport(0, 0, w, h);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_FOG);
  glFogi(GL_FOG_MODE, GL_LINEAR);
  glFogf(GL_FOG_DENSITY, 0.25);
  glHint(GL_FOG_HINT, GL_NICEST);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_POLYGON_OFFSET_LINE);
  glPolygonOffset(-3.0, -3.0);
  glCullFace(GL_FRONT);
  glEnable(GL_CULL_FACE);

  char *exts = (char *)glGetString(GL_EXTENSIONS);

  if (strstr(exts, "GL_EXT_texture_env_combine"))
    hasoverbright = true;
  else
    conoutf(
        "WARNING: cannot use overbright lighting, using old lighting model!");

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glmaxtexsize);

  purgetextures();

  if (!(qsphere = gluNewQuadric()))
    fatal("glu sphere");
  gluQuadricDrawStyle(qsphere, GLU_FILL);
  gluQuadricOrientation(qsphere, GLU_INSIDE);
  gluQuadricTexture(qsphere, GL_TRUE);
  glNewList(1, GL_COMPILE);
  gluSphere(qsphere, 1, 12, 6);
  glEndList();
};

void cleangl() {
  if (qsphere)
    gluDeleteQuadric(qsphere);
};

bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp) {
  SDL_Surface *s = IMG_Load(texname);
  if (!s) {
    conoutf("couldn't load texture %s", texname);
    return false;
  };
  if (s->format->BitsPerPixel != 24) {
    conoutf("texture must be 24bpp: %s", texname);
    return false;
  };

  glBindTexture(GL_TEXTURE_2D, tnum);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  xs = s->w;
  ys = s->h;

  while (xs > glmaxtexsize || ys > glmaxtexsize) {
    xs /= 2;
    ys /= 2;
  };

  void *scaledimg = s->pixels;
  if (xs != s->w) {
    conoutf("warning: quality loss: scaling %s", texname);
    scaledimg = alloc(xs * ys * 3);
    gluScaleImage(GL_RGB, s->w, s->h, GL_UNSIGNED_BYTE, s->pixels, xs, ys,
                  GL_UNSIGNED_BYTE, scaledimg);
  };
  if (gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, xs, ys, GL_RGB, GL_UNSIGNED_BYTE,
                        scaledimg))
    fatal("could not build mipmaps");
  if (xs != s->w)
    free(scaledimg);
  SDL_FreeSurface(s);
  return true;
};

const int MAXTEX = 1000;
int texx[MAXTEX]; // ( loaded texture ) -> ( name, size )
int texy[MAXTEX];
string texname[MAXTEX];
int curtex = 0;

const int FIRSTTEX = 1000; // opengl id = loaded id + FIRSTTEX
const int MAXFRAMES = 2;   // increase to allow more complex shader defs

int mapping[256][MAXFRAMES]; // ( cube texture, frame ) -> ( opengl id, name )
string mapname[256][MAXFRAMES];

void purgetextures() { loopi(256) loop(j, MAXFRAMES) mapping[i][j] = 0; };

int curtexnum = 0;

void texturereset() { curtexnum = 0; };

void texture(char *aframe, char *name) {
  int num = curtexnum++, frame = atoi(aframe);
  if (num < 0 || num >= 256 || frame < 0 || frame >= MAXFRAMES)
    return;
  mapping[num][frame] = 1;
  char *n = mapname[num][frame];
  strcpy_s(n, name);
  path(n);
};

COMMAND(texturereset, ARG_NONE);
COMMAND(texture, ARG_2STR);

int lookuptexture(int tex, int &xs, int &ys) {
  int frame = 0; // other frames?
  int tid = mapping[tex][frame];

  if (tid >= FIRSTTEX) {
    xs = texx[tid - FIRSTTEX];
    ys = texy[tid - FIRSTTEX];
    return tid;
  };

  xs = ys = 16;
  if (!tid)
    return 1;

  loopi(curtex) {
    if (strcmp(mapname[tex][frame], texname[i]) == 0) {
      mapping[tex][frame] = tid = i + FIRSTTEX;
      xs = texx[i];
      ys = texy[i];
      return tid;
    };
  };

  if (curtex == MAXTEX)
    fatal("loaded too many textures");

  int tnum = curtex + FIRSTTEX;
  strcpy_s(texname[curtex], mapname[tex][frame]);

  sprintf_sd(name)("packages%c%s", PATHDIV, texname[curtex]);

  if (installtex(tnum, name, xs, ys)) {
    mapping[tex][frame] = tnum;
    texx[curtex] = xs;
    texy[curtex] = ys;
    curtex++;
    return tnum;
  } else {
    return mapping[tex][frame] = FIRSTTEX; // temp fix
  };
};

void setupworld() {
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  setarraypointers();

  if (hasoverbright) {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
  };
};

int skyoglid;

struct strip {
  int tex, start, num;
};
vector<strip> strips;

void renderstripssky() {
  glBindTexture(GL_TEXTURE_2D, skyoglid);
  loopv(strips) if (strips[i].tex == skyoglid)
      glDrawArrays(GL_TRIANGLE_STRIP, strips[i].start, strips[i].num);
};

void renderstrips() {
  int lasttex = -1;
  loopv(strips) if (strips[i].tex != skyoglid) {
    if (strips[i].tex != lasttex) {
      glBindTexture(GL_TEXTURE_2D, strips[i].tex);
      lasttex = strips[i].tex;
    };
    glDrawArrays(GL_TRIANGLE_STRIP, strips[i].start, strips[i].num);
  };
};

void overbright(float amount) {
  if (hasoverbright)
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, amount);
};

void addstrip(int tex, int start, int n) {
  strip &s = strips.add();
  s.tex = tex;
  s.start = start;
  s.num = n;
};

extern SDL_Window *window;
VARFP(gamma, 30, 100, 300, {
  const float f = gamma / 100.0f;
  Uint16 ramp[256];
  SDL_CalculateGammaRamp(f, ramp);
  if (SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) == -1) {
    conoutf("Could not set gamma (card/driver doesn't support it?)");
    conoutf("sdl: %s", SDL_GetError());
  };
});

void getcamerapos(float &vx, float &vy, float &vz) {
  if (thirdperson && player1->state == CS_ALIVE) {
    float dist = 10;
    float yawrad = rad(player1->yaw);
    vx = player1->o.x - dist * sinf(yawrad);
    vy = player1->o.y + dist * cosf(yawrad);
    vz = player1->o.z;
    int sz = ssize;
    if (vx < MINBORD)
      vx = MINBORD;
    if (vx > sz - MINBORD)
      vx = sz - MINBORD;
    if (vy < MINBORD)
      vy = MINBORD;
    if (vy > sz - MINBORD)
      vy = sz - MINBORD;
  } else {
    vx = player1->o.x;
    vy = player1->o.y;
    vz = player1->o.z;
  }
}

void transplayer() {
  glLoadIdentity();

  glRotated(player1->roll, 0.0, 0.0, 1.0);
  glRotated(player1->pitch, -1.0, 0.0, 0.0);
  glRotated(player1->yaw, 0.0, 1.0, 0.0);

  if (thirdperson && player1->state == CS_ALIVE) {
    float dist = 10;
    float yawrad = rad(player1->yaw);
    float cam_x = player1->o.x - dist * sinf(yawrad);
    float cam_y = player1->o.y + dist * cosf(yawrad);
    glTranslated(-cam_x, -player1->o.z, -cam_y);
  } else {
    glTranslated(-player1->o.x,
                 (player1->state == CS_DEAD ? player1->eyeheight - 0.2f : 0) -
                     player1->o.z,
                 -player1->o.y);
  }
};

VARP(fov, 10, 120, 150);

int xtraverts;

static GLuint bloomtex[2] = {0, 0};
static int bloomw = 0, bloomh = 0;

VARP(bloom, 0, 1, 1);
VARP(bloomintensity, 0, 30, 100);

void addbloom(int w, int h) {
  if (!bloom || w < 8 || h < 8)
    return;
  if (!bloomtex[0])
    glGenTextures(2, bloomtex);

  int bw = max(8, w >> 2);
  int bh = max(8, h >> 2);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  loopi(2) {
    glBindTexture(GL_TEXTURE_2D, bloomtex[i]);
    if (bloomw != w || bloomh != h)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i == 0 ? w : bw, i == 0 ? h : bh,
                   0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };
  bloomw = w;
  bloomh = h;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glBindTexture(GL_TEXTURE_2D, bloomtex[0]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
  glViewport(0, 0, bw, bh);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, bw, 0, bh, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glBindTexture(GL_TEXTURE_2D, bloomtex[0]);
  glColor3f(1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(bw, 0);
  glTexCoord2f(1, 1);
  glVertex2i(bw, bh);
  glTexCoord2f(0, 1);
  glVertex2i(0, bh);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, bloomtex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glBindTexture(GL_TEXTURE_2D, bloomtex[0]);
  glColor3f(1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(w, 0);
  glTexCoord2f(1, 1);
  glVertex2i(w, h);
  glTexCoord2f(0, 1);
  glVertex2i(0, h);
  glEnd();

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  float intensity = bloomintensity / 100.0f;
  glColor3f(intensity, intensity, intensity);
  glBindTexture(GL_TEXTURE_2D, bloomtex[1]);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(w, 0);
  glTexCoord2f(1, 1);
  glVertex2i(w, h);
  glTexCoord2f(0, 1);
  glVertex2i(0, h);
  glEnd();

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
};

static GLuint grtex[2] = {0, 0};
static int grw = 0, grh = 0;

VARP(godrays, 0, 1, 1);
VARP(godraysintensity, 0, 20, 100);
VARP(godrayssamples, 1, 24, 64);
VARP(godraysscale, 10, 200, 500);
VARP(godrayslightx, 0, 50, 100);
VARP(godrayslighty, 0, 30, 100);

void addgodrays(int w, int h) {
  if (!godrays || w < 8 || h < 8)
    return;
  if (!grtex[0])
    glGenTextures(2, grtex);

  int bw = max(8, w >> 2);
  int bh = max(8, h >> 2);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  loopi(2) {
    glBindTexture(GL_TEXTURE_2D, grtex[i]);
    if (grw != w || grh != h)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i == 0 ? w : bw, i == 0 ? h : bh,
                   0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };
  grw = w;
  grh = h;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMatrixMode(GL_TEXTURE);
  glPushMatrix();

  glBindTexture(GL_TEXTURE_2D, grtex[0]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

  glViewport(0, 0, bw, bh);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, bw, 0, bh, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glColor3f(1, 1, 1);
  glBindTexture(GL_TEXTURE_2D, grtex[0]);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(bw, 0);
  glTexCoord2f(1, 1);
  glVertex2i(bw, bh);
  glTexCoord2f(0, 1);
  glVertex2i(0, bh);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, grtex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);

  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_BLEND);
  glBindTexture(GL_TEXTURE_2D, grtex[0]);
  glColor3f(1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(w, 0);
  glTexCoord2f(1, 1);
  glVertex2i(w, h);
  glTexCoord2f(0, 1);
  glVertex2i(0, h);
  glEnd();

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glBindTexture(GL_TEXTURE_2D, grtex[1]);

  float lightU = godrayslightx / 100.0f;
  float lightV = godrayslighty / 100.0f;
  float intensity = godraysintensity / 100.0f;
  float maxscale = godraysscale / 100.0f;
  int ns = godrayssamples;

  for (int i = 0; i < ns; i++) {
    float t = (float)i / (float)ns;
    float scale = 1.0f + t * t * (maxscale - 1.0f);
    float alpha = intensity / (float)ns;

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(lightU, lightV, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-lightU, -lightV, 0.0f);

    glColor3f(alpha, alpha, alpha);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(1, 0);
    glVertex2i(w, 0);
    glTexCoord2f(1, 1);
    glVertex2i(w, h);
    glTexCoord2f(0, 1);
    glVertex2i(0, h);
    glEnd();
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
};

static GLuint flaretex = 0;

VARP(lensflare, 0, 1, 1);
VARP(lensflareintensity, 0, 12, 100);
VARP(lensflarethreshold, 0, 60, 255);
VARP(sunyaw, 0, 270, 360);
VARP(sunpitch, -90, 30, 90);

void genflaretex() {
  const int S = 64;
  unsigned char pixels[S * S * 4];
  float c = (S - 1) * 0.5f;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c)) / c;
      float a = d < 1 ? (1 - d) * (1 - d) : 0;
      int i = (y * S + x) * 4;
      pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
      pixels[i + 3] = (uchar)(a * 255);
    };
  glGenTextures(1, &flaretex);
  glBindTexture(GL_TEXTURE_2D, flaretex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
};

void addlensflare(int w, int h) {
  if (!lensflare)
    return;
  if (OUTBORD((int)player1->o.x, (int)player1->o.y))
    return;
  sqr *sq = S((int)player1->o.x, (int)player1->o.y);
  if (!sq || (sq->r + sq->g + sq->b) / 3 < lensflarethreshold)
    return;
  if (!flaretex)
    genflaretex();

  float sy = sinf(sunyaw * (PI / 180.0f)), cy = cosf(sunyaw * (PI / 180.0f));
  float sp = sinf(sunpitch * (PI / 180.0f)),
        cp = cosf(sunpitch * (PI / 180.0f));
  float sunwx = cp * sy, sunwy = cp * cy, sunwz = sp;
  float swx = player1->o.x + 1000 * sunwx;
  float swy = player1->o.y + 1000 * sunwy;
  float swz = player1->o.z + 1000 * sunwz;

  GLdouble mv[16], proj[16];
  GLint vp[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, mv);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, vp);

  GLdouble sx, sy2, sz;
  if (gluProject(swx, swy, swz, mv, proj, vp, &sx, &sy2, &sz) == GL_FALSE)
    return;
  if (sz < 0 || sz > 1)
    return;

  float lx = (float)sx;
  float ly = (float)(h - sy2);
  if (lx < -w * 0.5f || lx > w * 1.5f || ly < -h * 0.5f || ly > h * 1.5f)
    return;

  float scx = w * 0.5f, scy = h * 0.5f;
  float dx = scx - lx, dy = scy - ly;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1)
    dx = 1, dy = 0;
  float intensity = lensflareintensity / 100.0f;

  float margin = max(w, h) * 0.15f;
  float fade = 1.0f;
  if (lx < -margin)
    fade = max(0.0f, (lx + w * 0.5f) / margin);
  else if (lx > w + margin)
    fade = max(0.0f, (w + margin - lx) / margin);
  if (ly < -margin)
    fade = min(fade, max(0.0f, (ly + h * 0.5f) / margin));
  else if (ly > h + margin)
    fade = min(fade, max(0.0f, (h + margin - ly) / margin));
  intensity *= fade;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_DEPTH_TEST);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBindTexture(GL_TEXTURE_2D, flaretex);

  float maxdim = max(w, h);

  {
    float s = maxdim * 0.15f, a = intensity * 0.35f;
    glColor3f(a, a * 0.7f, a * 0.3f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(lx - s, ly - s);
    glTexCoord2f(1, 0);
    glVertex2f(lx + s, ly - s);
    glTexCoord2f(1, 1);
    glVertex2f(lx + s, ly + s);
    glTexCoord2f(0, 1);
    glVertex2f(lx - s, ly + s);
    glEnd();
  };

  {
    float s = maxdim * 0.035f, a = intensity;
    glColor3f(a, a * 0.88f, a * 0.65f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(lx - s, ly - s);
    glTexCoord2f(1, 0);
    glVertex2f(lx + s, ly - s);
    glTexCoord2f(1, 1);
    glVertex2f(lx + s, ly + s);
    glTexCoord2f(0, 1);
    glVertex2f(lx - s, ly + s);
    glEnd();
  };

  {
    float dists[6] = {0.3f, 0.7f, 1.0f, 1.6f, 2.5f, 4.0f};
    float sizes[6] = {0.025f, 0.02f, 0.015f, 0.012f, 0.009f, 0.005f};
    float alphas[6] = {0.5f, 0.4f, 0.25f, 0.15f, 0.08f, 0.04f};
    for (int i = 0; i < 6; i++) {
      float px = lx + dx * dists[i];
      float py = ly + dy * dists[i];
      float s = maxdim * sizes[i], a = intensity * alphas[i];
      glColor3f(a, a * 0.92f, a * 0.85f);
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2f(px - s, py - s);
      glTexCoord2f(1, 0);
      glVertex2f(px + s, py - s);
      glTexCoord2f(1, 1);
      glVertex2f(px + s, py + s);
      glTexCoord2f(0, 1);
      glVertex2f(px - s, py + s);
      glEnd();
    };
  };

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
};

VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);
VARP(hudgun, 0, 1, 1);
VARP(viewbob, 0, 1, 1);
VARP(viewbobamp, 0, 10, 50);

char *hudgunnames[] = {"hudguns/hate_csaw", "hudguns/hate_shotg",
                       "hudguns/hate_rifle", "hudguns/hate_rocket",
                       "hudguns/hate_rail"};

void drawhudmodel(int start, int end, float speed, int base) {
  float bx = 0, bz = 0;
  if (viewbob && player1->onfloor && (player1->move || player1->strafe)) {
    float bobamp = viewbobamp * 0.004f;
    float bobphase = lastmillis * 0.008f;
    bz = sinf(bobphase * 2.0f) * bobamp;
    bx = sinf(bobphase) * bobamp * 0.3f;
  }

  float scale = 1.0f;

  if (gunswitchtime) {
    int elapsed = lastmillis - gunswitchtime;
    if (elapsed < 250) {
      float t = (float)elapsed / 250.0f;
      float ease = t * (2.0f - t);
      bz += (1.0f - ease) * -1.5f;
      scale = 0.8f + 0.2f * ease;
    } else {
      gunswitchtime = 0;
    }
  }

  if (player1->ads && player1->gunselect != GUN_CSAW) {
    int elapsed = lastmillis - player1->adstime;
    float t = elapsed < 200 ? (float)elapsed / 200.0f : 1.0f;
    bx *= 1.0f - t;
    bz -= t * 1.2f;
    scale += t * 0.1f;
  }

  rendermodel(hudgunnames[player1->gunselect], start, end, 0, 1.0f,
              player1->o.x + bx, player1->o.z + bz, player1->o.y,
              player1->yaw + 90, player1->pitch, false, scale, speed, 0, base);
};

void drawhudgun(float fovy, float aspect, int farplane) {
  if (!hudgun /*|| !player1->gunselect*/)
    return;

  glEnable(GL_CULL_FACE);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(fovy, aspect, 0.3f, farplane);
  glMatrixMode(GL_MODELVIEW);

  // glClear(GL_DEPTH_BUFFER_BIT);
  int rtime = reloadtime(player1->gunselect);
  if (player1->lastaction && player1->lastattackgun == player1->gunselect &&
      lastmillis - player1->lastaction < rtime) {
    if (player1->gunselect == GUN_RIFLE)
      drawhudmodel(7, 18, rtime / 16.0f, player1->lastaction);
    else if (player1->gunselect == GUN_SG) {
      int sgtime = lastmillis - player1->lastaction;
      if (sgtime < 900)
        drawhudmodel(9, 11, 100.0f, player1->lastaction);
      else
        drawhudmodel(19, 1, 100, 0);
    } else if (player1->gunselect == GUN_CSAW)
      drawhudmodel(2, 3, rtime / 3.0f, player1->lastaction);
    else
      drawhudmodel(7, 18, rtime / 18.0f, player1->lastaction);
  } else {
    if (player1->gunselect == GUN_RIFLE && player1->lastaction &&
        player1->lastattackgun == player1->gunselect)
      gunidletime = lastmillis;
    else
      gunidletime = 0;

    if (player1->gunselect == GUN_RIFLE)
      drawhudmodel(25, 1, 100, 0);
    else if (player1->gunselect == GUN_SG)
      drawhudmodel(19, 1, 100, 0);
    else if (player1->gunselect == GUN_CSAW)
      drawhudmodel(1, 1, 100, 0);
    else
      drawhudmodel(6, 1, 100, 0);
  };

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(fovy, aspect, 0.15f, farplane);
  glMatrixMode(GL_MODELVIEW);

  glDisable(GL_CULL_FACE);
};

void gl_drawframe(int w, int h, float curfps) {
  float hf = hdr.waterlevel - 0.3f;
  float afov = (float)fov;
  if (player1->ads && player1->gunselect != GUN_CSAW) {
    int elapsed = lastmillis - player1->adstime;
    float t = elapsed < 200 ? (float)elapsed / 200.0f : 1.0f;
    afov = fov * (1.0f - t * 0.45f);
  };
  float fovy = afov * h / w;
  float aspect = w / (float)h;
  bool underwater = player1->o.z < hf;

  glFogi(GL_FOG_START, (fog + 64) / 8);
  glFogi(GL_FOG_END, fog);
  float fogc[4] = {(fogcolour >> 16) / 256.0f,
                   ((fogcolour >> 8) & 255) / 256.0f,
                   (fogcolour & 255) / 256.0f, 1.0f};
  glFogfv(GL_FOG_COLOR, fogc);
  glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

  if (underwater) {
    fovy += (float)sin(lastmillis / 1000.0) * 2.0f;
    aspect += (float)sin(lastmillis / 1000.0 + PI) * 0.1f;
    glFogi(GL_FOG_START, 0);
    glFogi(GL_FOG_END, (fog + 96) / 8);
  };

  glClear((player1->outsidemap ? GL_COLOR_BUFFER_BIT : 0) |
          GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  int farplane = fog * 5 / 2;
  gluPerspective(fovy, aspect, 0.15f, farplane);
  glMatrixMode(GL_MODELVIEW);

  transplayer();

  glEnable(GL_TEXTURE_2D);

  int xs, ys;
  skyoglid = lookuptexture(DEFAULT_SKY, xs, ys);

  resetcubes();

  curvert = 0;
  strips.setsize(0);

  float vx, vy, vz;
  getcamerapos(vx, vy, vz);
  render_world(vx, vy, vz, (int)player1->yaw, (int)player1->pitch, afov,
               w, h);
  finishstrips();

  setupworld();

  renderstripssky();

  glLoadIdentity();
  glRotated(player1->pitch, -1.0, 0.0, 0.0);
  glRotated(player1->yaw, 0.0, 1.0, 0.0);
  glRotated(90.0, 1.0, 0.0, 0.0);
  glColor3f(1.0f, 1.0f, 1.0f);
  glDisable(GL_FOG);
  glDepthFunc(GL_GREATER);
  draw_envbox(14, fog * 4 / 3);
  glDepthFunc(GL_LESS);
  glEnable(GL_FOG);

  transplayer();

  overbright(2);

  renderstrips();

  xtraverts = 0;

  renderclients();
  if (thirdperson)
    renderclient(player1, isteam(player1->team, player1->team),
                 "monster/player", false, 1.25f);
  monsterrender();
  botrender();

  renderentities();

  renderspheres(curtime);
  renderbeams(curtime);

  glDisable(GL_CULL_FACE);

  if (!thirdperson)
    drawhudgun(fovy, aspect, farplane);

  overbright(1);
  int nquads = renderwater(hf);

  overbright(2);
  render_particles(curtime);
  overbright(1);

  addbloom(w, h);
  addgodrays(w, h);
  addlensflare(w, h);

  glDisable(GL_FOG);

  glDisable(GL_TEXTURE_2D);

  gl_drawhud(w, h, (int)curfps, nquads, curvert, underwater);

  glEnable(GL_CULL_FACE);
  glEnable(GL_FOG);
};
