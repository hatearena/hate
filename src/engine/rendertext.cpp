#include "../include/cube.h"

static TTF_Font *font = NULL;
static bool font_inited = false;

void init_font() {
  font = TTF_OpenFont(path(newstring("data/font.ttf")), FONTH);
  if (!font)
    fatal("Could not load font data/font.ttf");
  font_inited = true;
}

static int pot(int v) {
  int p = 1;
  while (p < v)
    p <<= 1;
  return p;
}

static void draw_texture(SDL_Surface *s, int left, int top, int cr, int cg,
                         int cb, int ca) {
  if (!s)
    return;
  int w = s->w, h = s->h;
  int pw = pot(w), ph = pot(h);

  SDL_Surface *conv =
      SDL_CreateRGBSurface(0, w, h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
  if (!conv) {
    SDL_FreeSurface(s);
    return;
  }

  SDL_LockSurface(s);
  SDL_LockSurface(conv);

  uchar *src = (uchar *)s->pixels;
  uchar *dst = (uchar *)conv->pixels;
  for (int gy = 0; gy < h; gy++)
    for (int gx = 0; gx < w; gx++) {
      uchar a = src[gy * s->pitch + gx * 4 + 3];
      uchar *dp = dst + gy * conv->pitch + gx * 3;
      dp[0] = a;
      dp[1] = a;
      dp[2] = a;
    }

  SDL_UnlockSurface(conv);
  SDL_UnlockSurface(s);
  SDL_FreeSurface(s);

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (pw == w && ph == h) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 conv->pixels);
  } else {
    SDL_Surface *pad =
        SDL_CreateRGBSurface(0, pw, ph, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (pad) {
      SDL_FillRect(pad, NULL, 0);
      SDL_Rect sr = {0, 0, w, h};
      SDL_BlitSurface(conv, &sr, pad, &sr);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pw, ph, 0, GL_RGB,
                   GL_UNSIGNED_BYTE, pad->pixels);
      SDL_FreeSurface(pad);
    } else {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                   conv->pixels);
    }
  }
  SDL_FreeSurface(conv);

  float umax = (float)w / (float)pw;
  float vmax = (float)h / (float)ph;

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBlendFunc(GL_ONE, GL_ONE);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor3ub(cr * ca / 255, cg * ca / 255, cb * ca / 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2i(left, top);
  glTexCoord2f(umax, 0);
  glVertex2i(left + w, top);
  glTexCoord2f(umax, vmax);
  glVertex2i(left + w, top + h);
  glTexCoord2f(0, vmax);
  glVertex2i(left, top + h);
  glEnd();
  xtraverts += 4;

  glDeleteTextures(1, &tex);
}

int text_width(char *str) {
  if (!font_inited)
    return 0;
  int w;
  TTF_SizeUTF8(font, str, &w, NULL);
  return w;
}

void draw_textf(char *fstr, int left, int top, int gl_num, ...) {
  sprintf_sdlv(str, gl_num, fstr);
  draw_text(str, left, top, gl_num);
}

void draw_text(char *str, int left, int top, int gl_num, int alpha) {
  (void)gl_num;
  if (!font_inited)
    return;

  string filtered;
  char *dst = filtered;
  for (char *src = str; *src; src++)
    if (*src != '\f')
      *dst++ = *src == '\t' ? ' ' : *src;
  *dst = 0;

  SDL_Color white = {255, 255, 255, 255};
  if (str[0] == '\f')
    draw_texture(TTF_RenderUTF8_Blended(font, filtered, white), left, top, 64,
                 255, 128, alpha);
  else
    draw_texture(TTF_RenderUTF8_Blended(font, filtered, white), left, top, 255,
                 255, 255, alpha);
}

void draw_envbox_aux(float s0, float t0, int x0, int y0, int z0, float s1,
                     float t1, int x1, int y1, int z1, float s2, float t2,
                     int x2, int y2, int z2, float s3, float t3, int x3, int y3,
                     int z3, int texture) {
  glBindTexture(GL_TEXTURE_2D, texture);
  glBegin(GL_QUADS);
  glTexCoord2f(s3, t3);
  glVertex3d(x3, y3, z3);
  glTexCoord2f(s2, t2);
  glVertex3d(x2, y2, z2);
  glTexCoord2f(s1, t1);
  glVertex3d(x1, y1, z1);
  glTexCoord2f(s0, t0);
  glVertex3d(x0, y0, z0);
  glEnd();
  xtraverts += 4;
}

void draw_envbox(int t, int w) {
  glDepthMask(GL_FALSE);

  draw_envbox_aux(1.0f, 1.0f, -w, -w, w, 0.0f, 1.0f, w, -w, w, 0.0f, 0.0f, w,
                  -w, -w, 1.0f, 0.0f, -w, -w, -w, t);

  draw_envbox_aux(1.0f, 1.0f, +w, w, w, 0.0f, 1.0f, -w, w, w, 0.0f, 0.0f, -w, w,
                  -w, 1.0f, 0.0f, +w, w, -w, t + 1);

  draw_envbox_aux(0.0f, 0.0f, -w, -w, -w, 1.0f, 0.0f, -w, w, -w, 1.0f, 1.0f, -w,
                  w, w, 0.0f, 1.0f, -w, -w, w, t + 2);

  draw_envbox_aux(1.0f, 1.0f, +w, -w, w, 0.0f, 1.0f, +w, w, w, 0.0f, 0.0f, +w,
                  w, -w, 1.0f, 0.0f, +w, -w, -w, t + 3);

  draw_envbox_aux(0.0f, 1.0f, -w, w, w, 0.0f, 0.0f, +w, w, w, 1.0f, 0.0f, +w,
                  -w, w, 1.0f, 1.0f, -w, -w, w, t + 4);

  draw_envbox_aux(0.0f, 1.0f, +w, w, -w, 0.0f, 0.0f, -w, w, -w, 1.0f, 0.0f, -w,
                  -w, -w, 1.0f, 1.0f, +w, -w, -w, t + 5);

  glDepthMask(GL_TRUE);
}
