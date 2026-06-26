#include "../include/cube.h"

#ifndef APIENTRY
#define APIENTRY
#endif

#ifdef DARWIN
#define GL_COMBINE_EXT GL_COMBINE_ARB
#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
#define GL_SOURCE0_RBG_EXT GL_SOURCE0_RGB_ARB
#define GL_SOURCE1_RBG_EXT GL_SOURCE1_RGB_ARB
#define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB

#endif

#ifdef __APPLE__
#ifndef GL_COMBINE_EXT
#define GL_COMBINE_EXT GL_COMBINE
#endif
#ifndef GL_COMBINE_RGB_EXT
#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB
#endif
#ifndef GL_SOURCE0_RGB_EXT
#define GL_SOURCE0_RGB_EXT GL_SOURCE0_RGB
#endif
#ifndef GL_SOURCE1_RGB_EXT
#define GL_SOURCE1_RGB_EXT GL_SOURCE1_RGB
#endif
#ifndef GL_PRIMARY_COLOR_EXT
#define GL_PRIMARY_COLOR_EXT GL_PRIMARY_COLOR
#endif
#ifndef GL_RGB_SCALE_EXT
#define GL_RGB_SCALE_EXT GL_RGB_SCALE
#endif
#endif

extern int curvert;
extern bool intermission;

bool hasoverbright = false;

void purgetextures();

GLUquadricObj *qsphere = NULL;
int glmaxtexsize = 1024;

typedef void(APIENTRY *glGenMipmapFunc)(GLenum);
static glGenMipmapFunc glGenMipmap_ = NULL;
static int genmipinit = 0;

VARFP(fsaa, 0, 8, 16,
      { conoutf("Anti-aliasing will take effect on next restart"); });
static int __ad_fsaa =
    (addcommanddetail("fsaa", "Full-scene anti-aliasing samples"), 0);

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

  if (fsaa)
    glEnable(GL_MULTISAMPLE);

  char *exts = (char *)glGetString(GL_EXTENSIONS);

  if (strstr(exts, "GL_EXT_texture_env_combine"))
    hasoverbright = true;
  else
    conoutf(
        "Warning: Cannot use overbright lighting, using old lighting model");

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

typedef void(APIENTRY *glCompressedTexImage2DFunc)(GLenum, GLint, GLenum,
                                                   GLsizei, GLsizei, GLint,
                                                   GLsizei, const GLvoid *);
static glCompressedTexImage2DFunc glCompressedTexImage2D_ = NULL;
static int glCompressedTexImage2DInit = 0;

static int s3tcsupport = -1;

static bool hass3tc() {
  if (s3tcsupport < 0) {
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    s3tcsupport =
        exts && strstr(exts, "GL_EXT_texture_compression_s3tc") ? 1 : 0;
  };
  return s3tcsupport != 0;
};

static bool loaddds(GLenum tnum, char *name, int &xs, int &ys, bool clamp) {
  FILE *f = fopen(name, "rb");
  if (!f)
    return false;

  char magic[4];
  if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "DDS ", 4) != 0) {
    fclose(f);
    return false;
  };

  unsigned int hdrsz, flags, h, w, pitch, depth, mips;
  unsigned int rsv1[11];
  unsigned int pfsz, pfflags;
  char fourcc[4];
  unsigned int bitcnt, rmask, gmask, bmask, amask;
  unsigned int caps, caps2, caps3, caps4, rsv2;

  fread(&hdrsz, 4, 1, f);
  fread(&flags, 4, 1, f);
  fread(&h, 4, 1, f);
  fread(&w, 4, 1, f);
  fread(&pitch, 4, 1, f);
  fread(&depth, 4, 1, f);
  fread(&mips, 4, 1, f);
  fread(rsv1, 4, 11, f);
  fread(&pfsz, 4, 1, f);
  fread(&pfflags, 4, 1, f);
  fread(fourcc, 1, 4, f);
  fread(&bitcnt, 4, 1, f);
  fread(&rmask, 4, 1, f);
  fread(&gmask, 4, 1, f);
  fread(&bmask, 4, 1, f);
  fread(&amask, 4, 1, f);
  fread(&caps, 4, 1, f);
  fread(&caps2, 4, 1, f);
  fread(&caps3, 4, 1, f);
  fread(&caps4, 4, 1, f);
  fread(&rsv2, 4, 1, f);

  xs = w;
  ys = h;

  GLenum glfmt = 0;
  int blocksize = 0;

  if (memcmp(fourcc, "DXT1", 4) == 0) {
    glfmt = amask ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                  : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    blocksize = 8;
  } else if (memcmp(fourcc, "DXT3", 4) == 0) {
    glfmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    blocksize = 16;
  } else if (memcmp(fourcc, "DXT5", 4) == 0) {
    glfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    blocksize = 16;
  } else if (pfflags & 0x40) {
    int bpp = bitcnt / 8;
    if (bitcnt != 24 && bitcnt != 32) {
      fclose(f);
      return false;
    };
    GLenum fmt = GL_BGR, ifmt = GL_RGB;
    if (bitcnt == 32) {
      fmt = (amask == 0x000000ff) ? GL_RGBA : GL_BGRA;
      ifmt = GL_RGBA;
    } else {
      fmt = (rmask == 0x000000ff) ? GL_RGB : GL_BGR;
    };

    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mips > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    xs = w;
    ys = h;

    int lw = w, lh = h;
    int levels = mips > 0 ? mips : 1;
    loopi(levels) {
      int size = lw * lh * bpp;
      void *data = alloc(size);
      fread(data, 1, size, f);
      glTexImage2D(GL_TEXTURE_2D, i, ifmt, lw, lh, 0, fmt, GL_UNSIGNED_BYTE,
                   data);
      free(data);
      lw = max(1, lw >> 1);
      lh = max(1, lh >> 1);
    };

    fclose(f);
    return true;
  } else {
    fclose(f);
    return false;
  };

  if (!glCompressedTexImage2DInit) {
    glCompressedTexImage2DInit = 1;
    glCompressedTexImage2D_ = (glCompressedTexImage2DFunc)SDL_GL_GetProcAddress(
        "glCompressedTexImage2D");
    if (!glCompressedTexImage2D_)
      glCompressedTexImage2D_ =
          (glCompressedTexImage2DFunc)SDL_GL_GetProcAddress(
              "glCompressedTexImage2DARB");
  }

  glBindTexture(GL_TEXTURE_2D, tnum);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  mips > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  int lw = w, lh = h;
  int levels = mips > 0 ? mips : 1;

  loopi(levels) {
    int size = ((lw + 3) / 4) * ((lh + 3) / 4) * blocksize;
    void *data = alloc(size);
    fread(data, 1, size, f);
    if (glCompressedTexImage2D_)
      glCompressedTexImage2D_(GL_TEXTURE_2D, i, glfmt, lw, lh, 0, size, data);
    free(data);
    lw = max(1, lw >> 1);
    lh = max(1, lh >> 1);
  };

  fclose(f);
  return true;
};

bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp) {
  if (hass3tc()) {
    char *dot = strrchr(texname, '.');
    if (dot) {
      string ddsname;
      strcpy_s(ddsname, texname);
      ddsname[dot - texname] = '\0';
      strcat_s(ddsname, ".dds");
      if (loaddds(tnum, ddsname, xs, ys, clamp))
        return true;
    };
  };

  SDL_Surface *s = IMG_Load(texname);
  if (!s) {
    conoutf("Couldn't load texture %s", texname);
    return false;
  };
  int bpp = s->format->BitsPerPixel;
  GLenum fmt = bpp == 32 ? GL_RGBA : GL_RGB;
  GLenum ifmt = bpp == 32 ? GL_RGBA : GL_RGB;
  if (bpp != 24 && bpp != 32) {
    conoutf("Texture must be 24bpp or 32bpp: %s", texname);
    return false;
  };

  glBindTexture(GL_TEXTURE_2D, tnum);
  glPixelStorei(GL_UNPACK_ALIGNMENT, bpp == 32 ? 4 : 1);
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
    conoutf("Warning: Quality loss: Scaling %s", texname);
    scaledimg = alloc(xs * ys * (bpp / 8));
    gluScaleImage(fmt, s->w, s->h, GL_UNSIGNED_BYTE, s->pixels, xs, ys,
                  GL_UNSIGNED_BYTE, scaledimg);
  };
  if (!genmipinit) {
    genmipinit = 1;
    glGenMipmap_ = (glGenMipmapFunc)SDL_GL_GetProcAddress("glGenerateMipmap");
  };
  if (glGenMipmap_) {
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, xs, ys, 0, fmt, GL_UNSIGNED_BYTE,
                 scaledimg);
    glGenMipmap_(GL_TEXTURE_2D);
  } else if (gluBuild2DMipmaps(GL_TEXTURE_2D, ifmt, xs, ys, fmt,
                               GL_UNSIGNED_BYTE, scaledimg))
    fatal("Could not build mipmaps");
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

int mapping[2048][MAXFRAMES]; // ( cube texture, frame ) -> ( opengl id, name )
string mapname[2048][MAXFRAMES];

void purgetextures() { loopi(2048) loop(j, MAXFRAMES) mapping[i][j] = 0; };

int curtexnum = 0;

void texturereset() { curtexnum = 0; };

void texture(char *aframe, char *name) {
  int num = curtexnum++, frame = atoi(aframe);
  if (num < 0 || num >= 2048 || frame < 0 || frame >= MAXFRAMES)
    return;
  mapping[num][frame] = 1;
  char *n = mapname[num][frame];
  strcpy_s(n, name);
  path(n);
};

COMMAND(texturereset, ARG_NONE);
static int __ad_texturereset =
    (addcommanddetail("texturereset", "Resets all textures"), 0);
COMMAND(texture, ARG_2STR);
static int __ad_texture =
    (addcommanddetail("texture", "Sets a texture by name"), 0);

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
static int __ad_gamma =
    (addcommanddetail("gamma", "Gamma correction level"), 0);

void getcamerapos(float &vx, float &vy, float &vz) {
  if (spectator && !speclook) {
    dynent *t = getspectarget();
    if (t) {
      vx = t->o.x;
      vy = t->o.y;
      vz = t->o.z + t->eyeheight;
      return;
    }
  }
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
  if (spectator && !speclook) {
    dynent *t = getspectarget();
    if (t) {
      glLoadIdentity();
      glRotated(t->roll, 0.0, 0.0, 1.0);
      glRotated(t->pitch, -1.0, 0.0, 0.0);
      glRotated(t->yaw, 0.0, 1.0, 0.0);
      glTranslated(-t->o.x, -t->o.z - t->eyeheight, -t->o.y);
      return;
    }
  }
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
static int __ad_fov =
    (addcommanddetail("fov", "Field of view angle in degrees"), 0);

int xtraverts;

static GLuint bloomtex[2] = {0, 0};
static int bloomw = 0, bloomh = 0;

VARP(bloom, 0, 1, 1);
static int __ad_bloom =
    (addcommanddetail("bloom", "Toggles bloom post-processing"), 0);
VARP(bloomintensity, 0, 30, 100);
static int __ad_bloomintensity =
    (addcommanddetail("bloomintensity", "Bloom intensity level"), 0);

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
static int __ad_godrays =
    (addcommanddetail("godrays", "Toggles god rays effect"), 0);
VAR(godraysintensity, 0, 10, 100);
static int __ad_godraysintensity =
    (addcommanddetail("godraysintensity", "God rays intensity"), 0);
VARP(godrayssamples, 1, 24, 64);
static int __ad_godrayssamples =
    (addcommanddetail("godrayssamples", "God rays sample count"), 0);
VARP(godraysscale, 10, 200, 500);
static int __ad_godraysscale =
    (addcommanddetail("godraysscale", "God rays scale factor"), 0);
VARP(godrayslightx, 0, 50, 100);
static int __ad_godrayslightx =
    (addcommanddetail("godrayslightx", "God rays light X position"), 0);
VARP(godrayslighty, 0, 30, 100);
static int __ad_godrayslighty =
    (addcommanddetail("godrayslighty", "God rays light Y position"), 0);

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
static int __ad_lensflare =
    (addcommanddetail("lensflare", "Toggles lens flare effect"), 0);
VARP(lensflareintensity, 0, 12, 100);
static int __ad_lensflareintensity =
    (addcommanddetail("lensflareintensity", "Lens flare intensity"), 0);
VARP(lensflarethreshold, 0, 60, 255);
static int __ad_lensflarethreshold =
    (addcommanddetail("lensflarethreshold", "Lens flare brightness threshold"),
     0);
VARP(sunyaw, 0, 270, 360);
static int __ad_sunyaw = (addcommanddetail("sunyaw", "Sun yaw angle"), 0);
VARP(sunpitch, -90, 30, 90);
static int __ad_sunpitch = (addcommanddetail("sunpitch", "Sun pitch angle"), 0);

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

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

typedef void(APIENTRY *glActiveTexFunc)(GLenum);
typedef void(APIENTRY *glMultiTexFunc)(GLenum, GLfloat, GLfloat);
static glActiveTexFunc glActiveTex = NULL;
static glMultiTexFunc glMultiTex = NULL;
static int ssaoinit = 0;

static int checkssao() {
  if (!ssaoinit) {
    ssaoinit = 1;
    glActiveTex = (glActiveTexFunc)SDL_GL_GetProcAddress("glActiveTexture");
    glMultiTex = (glMultiTexFunc)SDL_GL_GetProcAddress("glMultiTexCoord2f");
    if (!glActiveTex)
      glActiveTex =
          (glActiveTexFunc)SDL_GL_GetProcAddress("glActiveTextureARB");
    if (!glMultiTex)
      glMultiTex =
          (glMultiTexFunc)SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
  };
  return glActiveTex && glMultiTex;
};

VARP(ssao, 0, 1, 1);
static int __ad_ssao = (addcommanddetail("ssao", "Toggles SSAO effect"), 0);
VARP(ssaostrength, 0, 10, 100);
static int __ad_ssaostrength =
    (addcommanddetail("ssaostrength", "SSAO strength"), 0);
VARP(ssaoradius, 1, 2, 10);
static int __ad_ssaoradius =
    (addcommanddetail("ssaoradius", "SSAO sampling radius"), 0);

static GLuint ssaotex[3] = {0, 0, 0};
static int ssaow = 0, ssaoh = 0;

void addssao(int w, int h) {
  if (!ssao || !checkssao() || w < 8 || h < 8)
    return;
  if (!ssaotex[0])
    glGenTextures(3, ssaotex);

  int sw = max(8, w >> 2);
  int sh = max(8, h >> 2);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  loopi(3) {
    glBindTexture(GL_TEXTURE_2D, ssaotex[i]);
    if (ssaow != w || ssaoh != h)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i == 0 ? w : sw, i == 0 ? h : sh,
                   0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  ssaow = w;
  ssaoh = h;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMatrixMode(GL_TEXTURE);
  glPushMatrix();
  glBindTexture(GL_TEXTURE_2D, ssaotex[0]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
  glViewport(0, 0, sw, sh);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, sw, 0, sh, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glColor3f(1, 1, 1);
  glBindTexture(GL_TEXTURE_2D, ssaotex[0]);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(0, 0);
  glTexCoord2f(1, 0);
  glVertex2i(sw, 0);
  glTexCoord2f(1, 1);
  glVertex2i(sw, sh);
  glTexCoord2f(0, 1);
  glVertex2i(0, sh);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, ssaotex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sw, sh);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(0, 0, 0, 0);

  float radius = ssaoradius / (float)sw;
  float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  loopi(4) {
    glActiveTex(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaotex[1]);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glActiveTex(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ssaotex[1]);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_SUBTRACT);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(dirs[i][0] * radius, dirs[i][1] * radius, 0);
    glBegin(GL_QUADS);
    glMultiTex(GL_TEXTURE0, 0, 0);
    glMultiTex(GL_TEXTURE1, 0, 0);
    glVertex2i(0, 0);
    glMultiTex(GL_TEXTURE0, 1, 0);
    glMultiTex(GL_TEXTURE1, 1, 0);
    glVertex2i(sw, 0);
    glMultiTex(GL_TEXTURE0, 1, 1);
    glMultiTex(GL_TEXTURE1, 1, 1);
    glVertex2i(sw, sh);
    glMultiTex(GL_TEXTURE0, 0, 1);
    glMultiTex(GL_TEXTURE1, 0, 1);
    glVertex2i(0, sh);
    glEnd();
  }

  glActiveTex(GL_TEXTURE1);
  glDisable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glActiveTex(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ssaotex[2]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sw, sh);

  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBindTexture(GL_TEXTURE_2D, ssaotex[0]);
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
  float strength = ssaostrength / 100.0f;
  glColor3f(strength, strength, strength);
  glBindTexture(GL_TEXTURE_2D, ssaotex[2]);
  glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
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
  glMatrixMode(GL_TEXTURE);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glEnable(GL_DEPTH_TEST);
};

static GLuint doftex[2] = {0, 0};
static int dofw = 0, dofh = 0;

VARP(dof, 0, 0, 1);
static int __ad_dof =
    (addcommanddetail("dof", "Toggles depth of field effect"), 0);
VARP(dofstrength, 1, 4, 5);
static int __ad_dofstrength =
    (addcommanddetail("dofstrength", "Depth of field blur strength"), 0);

void adddof(int w, int h) {
  if (!dof || w < 8 || h < 8)
    return;
  if (!doftex[0])
    glGenTextures(2, doftex);

  int bw = max(8, w >> 2);
  int bh = max(8, h >> 2);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  loopi(2) {
    glBindTexture(GL_TEXTURE_2D, doftex[i]);
    if (dofw != w || dofh != h)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, i == 0 ? w : bw, i == 0 ? h : bh,
                   0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };
  dofw = w;
  dofh = h;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMatrixMode(GL_TEXTURE);
  glPushMatrix();

  glBindTexture(GL_TEXTURE_2D, doftex[0]);
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
  glBindTexture(GL_TEXTURE_2D, doftex[0]);
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

  glBindTexture(GL_TEXTURE_2D, doftex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);

  float hweights[7] = {0.015f, 0.085f, 0.22f, 0.36f, 0.22f, 0.085f, 0.015f};
  float tw = 0;
  loopi(7) tw += hweights[i];
  loopi(7) hweights[i] /= tw;

  float strength = dofstrength / (float)bw;

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  glBindTexture(GL_TEXTURE_2D, doftex[1]);
  loopi(7) {
    float ox = (i - 3) * strength;
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(ox, 0, 0);
    float wgt = hweights[i];
    glColor3f(wgt, wgt, wgt);
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
  };

  glBindTexture(GL_TEXTURE_2D, doftex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);

  glClear(GL_COLOR_BUFFER_BIT);
  loopi(7) {
    float oy = (i - 3) * strength;
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0, oy, 0);
    float wgt = hweights[i];
    glColor3f(wgt, wgt, wgt);
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
  };

  glBindTexture(GL_TEXTURE_2D, doftex[1]);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBindTexture(GL_TEXTURE_2D, doftex[0]);
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

  float blurAmount = dofstrength / 20.0f;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindTexture(GL_TEXTURE_2D, doftex[1]);
  glColor4f(1, 1, 1, blurAmount);
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

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
};

VARP(vignette, 0, 1, 1);
static int __ad_vignette =
    (addcommanddetail("vignette", "Toggles vignette effect"), 0);
VARP(vignettestrength, 0, 22, 100);
static int __ad_vignettestrength =
    (addcommanddetail("vignettestrength", "Vignette darkness strength"), 0);

static GLuint vigtex = 0;

void genvigtex() {
  const int S = 256;
  unsigned char pixels[S * S * 4];
  float c = (S - 1) * 0.5f;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      float dx = (x - c) / c;
      float dy = (y - c) / c;
      float d = sqrtf(dx * dx + dy * dy);
      float a = d < 1 ? d * d : 1;
      int i = (y * S + x) * 4;
      pixels[i] = pixels[i + 1] = pixels[i + 2] = (uchar)(a * 255);
      pixels[i + 3] = 255;
    };
  glGenTextures(1, &vigtex);
  glBindTexture(GL_TEXTURE_2D, vigtex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
};

VARP(exposure, 1, 100, 500);
VARP(contrast, 1, 100, 500);
VARP(saturation, 0, 100, 500);
VARP(redgain, 1, 100, 500);
VARP(greengain, 1, 100, 500);
VARP(bluegain, 1, 100, 500);

static int __ad_exposure =
    (addcommanddetail("exposure", "Overall brightness multiplier (100 = 1.0)"),
     0);
static int __ad_contrast =
    (addcommanddetail("contrast", "Contrast adjustment (100 = 1.0)"), 0);
static int __ad_saturation =
    (addcommanddetail("saturation", "Color saturation (100 = 1.0)"), 0);
static int __ad_redgain =
    (addcommanddetail("redgain", "Red channel gain multiplier (100 = 1.0)"), 0);
static int __ad_greengain =
    (addcommanddetail("greengain", "Green channel gain multiplier (100 = 1.0)"),
     0);
static int __ad_bluegain =
    (addcommanddetail("bluegain", "Blue channel gain multiplier (100 = 1.0)"),
     0);

static void addcolorgrade(int w, int h) {
  float exp = exposure / 100.0f;
  float con = contrast / 100.0f;
  float sat = saturation / 100.0f;
  float rg = redgain / 100.0f;
  float gg = greengain / 100.0f;
  float bg = bluegain / 100.0f;

  if (exp == 1.0f && con == 1.0f && sat == 1.0f && rg == 1.0f && gg == 1.0f &&
      bg == 1.0f)
    return;

  float mult_r = con * exp * rg * sat;
  float mult_g = con * exp * gg * sat;
  float mult_b = con * exp * bg * sat;
  float add_r =
      (0.5f * (1.0f - con) * exp * rg * sat) + (0.5f * (1.0f - sat) * exp * rg);
  float add_g =
      (0.5f * (1.0f - con) * exp * gg * sat) + (0.5f * (1.0f - sat) * exp * gg);
  float add_b =
      (0.5f * (1.0f - con) * exp * bg * sat) + (0.5f * (1.0f - sat) * exp * bg);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glColor3f(mult_r, mult_g, mult_b);
  glBegin(GL_QUADS);
  glVertex2i(0, 0);
  glVertex2i(w, 0);
  glVertex2i(w, h);
  glVertex2i(0, h);
  glEnd();

  glBlendFunc(GL_ONE, GL_ONE);
  glColor3f(add_r, add_g, add_b);
  glBegin(GL_QUADS);
  glVertex2i(0, 0);
  glVertex2i(w, 0);
  glVertex2i(w, h);
  glVertex2i(0, h);
  glEnd();

  glDisable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
};

void addvignette(int w, int h) {
  if (!vignette)
    return;
  if (!vigtex)
    genvigtex();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  float strength = vignettestrength / 100.0f;
  glColor3f(strength, strength, strength);
  glBindTexture(GL_TEXTURE_2D, vigtex);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
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

VAR(fog, 64, 180, 1024);
static int __ad_fog = (addcommanddetail("fog", "Fog distance"), 0);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);
static int __ad_fogcolour =
    (addcommanddetail("fogcolour", "Fog color as hex RGB"), 0);

VARP(hudgun, 0, 1, 1);
static int __ad_hudgun =
    (addcommanddetail("hudgun", "Toggles HUD gun display"), 0);
VARP(viewbob, 0, 1, 1);
static int __ad_viewbob =
    (addcommanddetail("viewbob", "Toggles view bobbing"), 0);
VARP(viewbobamp, 0, 10, 50);
static int __ad_viewbobamp =
    (addcommanddetail("viewbobamp", "View bobbing amplitude"), 0);

char *hudgunnames[] = {"hudguns/hate_csaw",    "hudguns/hate_shotg",
                       "hudguns/hate_cgun",    "hudguns/hate_rocket",
                       "hudguns/hate_rail",    "hudguns/hate_nailgun",
                       "hudguns/hate_lightgun"};

void drawhudmodel(int start, int end, float speed, int base) {
  float target_intensity =
      (viewbob && player1->onfloor && (player1->move || player1->strafe))
          ? 1.0f
          : 0.0f;
  static float bob_intensity = 0.0f;
  if (target_intensity > bob_intensity) {
    bob_intensity += curtime * 0.008f;
    if (bob_intensity > target_intensity)
      bob_intensity = target_intensity;
  } else if (target_intensity < bob_intensity) {
    bob_intensity -= curtime * 0.04f;
    if (bob_intensity < target_intensity)
      bob_intensity = target_intensity;
  }

  float bx = 0, bz = 0;
  if (bob_intensity > 0.0f) {
    float bobamp = viewbobamp * 0.004f * bob_intensity;
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

  if (player1->gunselect != GUN_CSAW) {
    int elapsed = lastmillis - player1->adstime;
    if (elapsed < 200) {
      float t = (float)elapsed / 200.0f;
      float adsp = player1->ads ? t : 1.0f - t;
      bx *= 1.0f - adsp * 0.8f;
      scale += adsp * 0.3f;
    } else if (player1->ads) {
      bx *= 0.2f;
      scale += 0.3f;
    };
  }

  float px = player1->o.x + bx;
  float py = player1->o.z + bz;
  float pz = player1->o.y;
  float pyaw = player1->yaw + 90;
  float ppitch = player1->pitch;

  float glow = 0.0f;
  const vec *glowcol = NULL;
  vec teamcol;

  if (player1->gunselect == GUN_RAILGUN) {
    float t = (float)(lastmillis % 2000) / 2000.0f;
    float pulse = (t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f) * 0.05f;
    if (m_teammode && (!strcmp(player1->team, "RED") ||
                       (m_infected && !strcmp(player1->team, "INFD")))) {
      glow = 0.2f + pulse;
      teamcol.x = 1.0f; teamcol.y = 0.0f; teamcol.z = 0.0f;
      glowcol = &teamcol;
    } else {
      glow = pulse;
      teamcol.x = 0.4f; teamcol.y = 0.7f; teamcol.z = 1.0f; 
      glowcol = &teamcol;
    }
  } else if (m_teammode) {
    if (!strcmp(player1->team, "RED") ||
        (m_infected && !strcmp(player1->team, "INFD"))) {
      glow = 0.3f;
      teamcol.x = 1.0f; teamcol.y = 0.0f; teamcol.z = 0.0f;
      glowcol = &teamcol;
    } else if (!strcmp(player1->team, "BLUE") ||
               (m_infected && !strcmp(player1->team, "RES"))) {
      glow = 0.2f;
      teamcol.x = 0.0f; teamcol.y = 0.0f; teamcol.z = 1.0f;
      glowcol = &teamcol;
    }
  }

  float alpha = 1.0f;
  if (player1->spawnprotectfade > 0)
    alpha = 0.3f + 0.7f * (1.0f - (float)player1->spawnprotectfade / 200.0f);
  else if (player1->spawnprotectmillis > 0)
    alpha = 0.3f;
  rendermodel(hudgunnames[player1->gunselect], start, end, 0, 1.0f, px, py, pz,
              pyaw, ppitch, false, scale, speed, 0, base, glow, glowcol, alpha);
};

dynent *specplayer() {
  if (spectator && !speclook)
    return getspectarget();
  return player1;
}

void drawhudgun(float fovy, float aspect, int farplane) {
  dynent *d = specplayer();
  if (!hudgun || !d)
    return;

  glEnable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(fovy, aspect, 0.3f, farplane);
  glMatrixMode(GL_MODELVIEW);

  int rtime = reloadtime(d->gunselect);
  if (d->lastaction && d->lastattackgun == d->gunselect &&
      lastmillis - d->lastaction < rtime) {
    if (d->gunselect == GUN_RAILGUN) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.82f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      int sgtime = lastmillis - d->lastaction;
      if (sgtime < 610)
        drawhudmodel(6, 4, 200, d->lastaction);
      else
        drawhudmodel(4, 1, 100, 0);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_SG) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.88f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      int sgtime = lastmillis - d->lastaction;
      if (sgtime < 690)
        drawhudmodel(0, 7, 100.0f, d->lastaction);
      else
        drawhudmodel(0, 1, 100, 0);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_CSAW)
      drawhudmodel(2, 3, rtime / 3.0f, d->lastaction);
    else if (d->gunselect == GUN_NAILGUN)
      drawhudmodel(1, 7, rtime / 4.0f, d->lastaction);
    else if (d->gunselect == GUN_CG) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.88f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      drawhudmodel(1, 8, rtime / 8.0f, d->lastaction);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_RL)
      drawhudmodel(1, 6, 170, d->lastaction);
    else if (d->gunselect == GUN_LIGHTGUN) {
      if (d->attacking)
        drawhudmodel(1, 4, 150.0f, lastmillis - (lastmillis % 600));
      else
        drawhudmodel(0, 1, 100, 0);
    } else
      drawhudmodel(7, 18, rtime / 18.0f, d->lastaction);
  } else {
    if (d->gunselect == GUN_RAILGUN && d->lastaction &&
        d->lastattackgun == d->gunselect)
      gunidletime = lastmillis;
    else
      gunidletime = 0;

    if (d->gunselect == GUN_RAILGUN) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.82f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      drawhudmodel(4, 1, 100, 0);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_SG) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.88f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      drawhudmodel(0, 0, 100, 0);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_NAILGUN)
      drawhudmodel(0, 1, 100, 0);
    else if (d->gunselect == GUN_CG) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(fovy * 0.88f, aspect, 0.3f, farplane);
      glMatrixMode(GL_MODELVIEW);
      drawhudmodel(0, 1, 100, 0);
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
    } else if (d->gunselect == GUN_CSAW)
      drawhudmodel(1, 1, 100, 0);
    else if (d->gunselect == GUN_LIGHTGUN)
      drawhudmodel(0, 1, 100, 0);
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
  if (!spectator && player1->gunselect != GUN_CSAW) {
    int elapsed = lastmillis - player1->adstime;
    if (elapsed < 200) {
      float t = (float)elapsed / 200.0f;
      float adsp = player1->ads ? t : 1.0f - t;
      afov = fov * (1.0f - adsp * 0.45f);
    } else if (player1->ads) {
      afov = fov * 0.55f;
    };
  };
  float fovy = afov * h / w;
  float aspect = w / (float)h;

  float vx, vy, vz;
  getcamerapos(vx, vy, vz);

  float camyaw, campitch;
  if (spectator && !speclook) {
    dynent *t = getspectarget();
    if (t) {
      camyaw = t->yaw;
      campitch = t->pitch;
    } else {
      camyaw = player1->yaw;
      campitch = player1->pitch;
    }
  } else {
    camyaw = player1->yaw;
    campitch = player1->pitch;
  }

  float lf = hdr.lavalevel - 0.3f;
  bool underwater = vz < hf;
  bool inlava = vz < lf;

  bool cam_outside = vx < 0 || vx >= ssize || vy < 0 || vy >= ssize;
  if (!cam_outside) {
    sqr *s = S((int)vx, (int)vy);
    cam_outside = SOLID(s) ||
                  vz < s->floor - (s->type == FHF ? s->vdelta / 4.0f : 0) ||
                  vz > s->ceil + (s->type == CHF ? s->vdelta / 4.0f : 0);
  }

  glFogi(GL_FOG_START, (fog + 64) / 8);
  glFogi(GL_FOG_END, fog);
  float fogc[4] = {(fogcolour >> 16) / 256.0f,
                   ((fogcolour >> 8) & 255) / 256.0f,
                   (fogcolour & 255) / 256.0f, 1.0f};
  glFogfv(GL_FOG_COLOR, fogc);
  glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

  if (underwater || inlava) {
    fovy += (float)sin(lastmillis / 1000.0) * 2.0f;
    aspect += (float)sin(lastmillis / 1000.0 + PI) * 0.1f;
    glFogi(GL_FOG_START, 0);
    glFogi(GL_FOG_END, (fog + 96) / 8);
  };
  if (inlava) {
    float lavafog[4] = {0.8f, 0.1f, 0.0f, 1.0f};
    glFogfv(GL_FOG_COLOR, lavafog);
    glClearColor(lavafog[0], lavafog[1], lavafog[2], 1.0f);
  };

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

  render_world(vx, vy, vz, (int)camyaw, (int)campitch, afov, w, h);

  finishstrips();
  setupworld();
  renderstripssky();

  glLoadIdentity();
  glRotated(campitch, -1.0, 0.0, 0.0);
  glRotated(camyaw, 0.0, 1.0, 0.0);
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
  if (thirdperson && !spectator && !screenshotmode) {
    const char *mdl = "monster/player";
    if (m_teammode || m_infected) {
      if (player1->team[0] &&
          !strcmp(player1->team, m_infected ? "RES" : "BLUE"))
        mdl = "monster/blueplayer";
      else
        mdl = "monster/redplayer";
    }
    renderclient(player1, isteam(player1->team, player1->team), mdl, false,
                 1.25f);
  }
  monsterrender();
  botrender();

  renderentities();

  renderspheres(curtime);
  renderbeams(curtime);

  glDisable(GL_CULL_FACE);

  if (!thirdperson && !editmode && !intermission && !screenshotmode &&
      !spectator)
    drawhudgun(fovy, aspect, farplane);

  overbright(1);
  int nquads = renderwater(hf);
  renderlava(lf);

  overbright(2);
  render_particles(curtime);
  overbright(1);

  addssao(w, h);
  addbloom(w, h);
  addgodrays(w, h);
  addlensflare(w, h);
  adddof(w, h);
  addvignette(w, h);
  addcolorgrade(w, h);

  glDisable(GL_FOG);
  glDisable(GL_TEXTURE_2D);

  if (editmode) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    extern void cursorupdate();
    cursorupdate();
    rendereditentities();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
  };

  gl_drawhud(w, h, (int)curfps, nquads, curvert, underwater, inlava);

  glEnable(GL_CULL_FACE);
  glEnable(GL_FOG);
};
