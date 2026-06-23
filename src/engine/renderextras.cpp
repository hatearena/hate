#include "../include/cube.h"

extern bool intermission;
extern int intermissiontimer;

void line(int x1, int y1, float z1, int x2, int y2, float z2) {
  glBegin(GL_LINE_STRIP);
  glVertex3f((float)x1, z1, (float)y1);
  glVertex3f((float)x2, z2, (float)y2);
  glEnd();
  xtraverts += 2;
};

void linestyle(float width, int r, int g, int b) {
  glLineWidth(width);
  glColor4ub(r, g, b, 128);
};

void box(block &b, float z1, float z2, float z3, float z4) {
  glBegin(GL_POLYGON);
  glVertex3f((float)b.x, z1, (float)b.y);
  glVertex3f((float)b.x + b.xs, z2, (float)b.y);
  glVertex3f((float)b.x + b.xs, z3, (float)b.y + b.ys);
  glVertex3f((float)b.x, z4, (float)b.y + b.ys);
  glEnd();
  xtraverts += 4;
};

void dot(int x, int y, float z) {
  const float DOF = 0.1f;
  glBegin(GL_POLYGON);
  glVertex3f(x - DOF, (float)z, y - DOF);
  glVertex3f(x + DOF, (float)z, y - DOF);
  glVertex3f(x + DOF, (float)z, y + DOF);
  glVertex3f(x - DOF, (float)z, y + DOF);
  glEnd();
  xtraverts += 4;
};

void blendbox(int x1, int y1, int x2, int y2, bool border) {
  glDepthMask(GL_FALSE);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
  glBegin(GL_QUADS);
  if (border)
    glColor3d(0.5, 0.3, 0.4);
  else
    glColor3d(1.0, 1.0, 1.0);
  glVertex2i(x1, y1);
  glVertex2i(x2, y1);
  glVertex2i(x2, y2);
  glVertex2i(x1, y2);
  glEnd();
  glDisable(GL_BLEND);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glBegin(GL_POLYGON);
  glColor3d(0.2, 0.7, 0.4);
  glVertex2i(x1, y1);
  glVertex2i(x2, y1);
  glVertex2i(x2, y2);
  glVertex2i(x1, y2);
  glEnd();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  xtraverts += 8;
  glEnable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);
};

void overlay(int a) {
  glDepthMask(GL_FALSE);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  glColor4ub(0, 0, 0, a);
  glVertex2i(0, 0);
  glVertex2i(VIRTW, 0);
  glVertex2i(VIRTW, VIRTH);
  glVertex2i(0, VIRTH);
  glEnd();
  glEnable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);
};

void gradientbox(int x1, int y1, int x2, int y2, int r1, int g1, int b1, int r2,
                 int g2, int b2) {
  glDepthMask(GL_FALSE);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  glColor3ub(r1, g1, b1);
  glVertex2i(x1, y1);
  glVertex2i(x2, y1);
  glColor3ub(r2, g2, b2);
  glVertex2i(x2, y2);
  glVertex2i(x1, y2);
  glEnd();
  glEnable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);
};

const int MAXSPHERES = 50;
struct sphere {
  vec o;
  float size, max;
  int type;
  sphere *next;
};
sphere spheres[MAXSPHERES], *slist = NULL, *sempty = NULL;
bool sinit = false;

void newsphere(vec &o, float max, int type) {
  if (!sinit) {
    loopi(MAXSPHERES) {
      spheres[i].next = sempty;
      sempty = &spheres[i];
    };
    sinit = true;
  };
  if (sempty) {
    sphere *p = sempty;
    sempty = p->next;
    p->o = o;
    p->max = max;
    p->size = 1;
    p->type = type;
    p->next = slist;
    slist = p;
  };
};

const int MAXBEAMS = 50;
struct beam {
  vec from, to;
  int spawntime;
  int duration;
  int color;
  beam *next;
};
beam beams[MAXBEAMS], *blist = NULL, *bempty = NULL;
bool binitb = false;

void newbeam(vec &from, vec &to, int duration, int color) {
  if (!binitb) {
    loopi(MAXBEAMS) {
      beams[i].next = bempty;
      bempty = &beams[i];
    };
    binitb = true;
  };
  if (bempty) {
    beam *p = bempty;
    bempty = p->next;
    p->from = from;
    p->to = to;
    p->spawntime = lastmillis;
    p->duration = duration;
    p->color = color;
    p->next = blist;
    blist = p;
  };
};

void renderbeams(int time) {
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_TEXTURE_2D);

  for (beam *p, **pp = &blist; p = *pp;) {
    int lifetime = lastmillis - p->spawntime;
    if (lifetime > p->duration) {
      *pp = p->next;
      p->next = bempty;
      bempty = p;
    } else {
      float alpha = 1.0f - (float)lifetime / p->duration;
      float width = (0.08f + 0.25f * alpha);

      vec dir = p->to;
      vsub(dir, p->from);
      float len = (float)sqrt(dotprod(dir, dir));
      if (len > 0) {
        float invlen = 1.0f / len;
        vmul(dir, invlen);

        vec up = {0, 0, 1};
        float dp = dotprod(dir, up);
        if (dp > 0.9f || dp < -0.9f) {
          up.x = 1;
          up.y = 0;
          up.z = 0;
        };

        vec right;
        right.x = dir.y * up.z - dir.z * up.y;
        right.y = dir.z * up.x - dir.x * up.z;
        right.z = dir.x * up.y - dir.y * up.x;
        float rlen = (float)sqrt(dotprod(right, right));
        if (rlen > 0) {
          float rinv = 1.0f / rlen;
          vmul(right, rinv);
        };

        vec vup;
        vup.x = right.y * dir.z - right.z * dir.y;
        vup.y = right.z * dir.x - right.x * dir.z;
        vup.z = right.x * dir.y - right.y * dir.x;

        float gw = width * 1.5f;
        if (p->color == 1) {
          glColor4f(0.1f, 0.3f, 1.0f, alpha * 0.3f);
        } else {
          glColor4f(1.0f, 0.2f, 0.05f, alpha * 0.3f);
        }
        glBegin(GL_QUADS);
#define BEAMQUAD(rv)                                                           \
  glVertex3f(p->from.x + rv.x * gw, p->from.z + rv.z * gw,                     \
             p->from.y + rv.y * gw);                                           \
  glVertex3f(p->from.x - rv.x * gw, p->from.z - rv.z * gw,                     \
             p->from.y - rv.y * gw);                                           \
  glVertex3f(p->to.x - rv.x * gw, p->to.z - rv.z * gw, p->to.y - rv.y * gw);   \
  glVertex3f(p->to.x + rv.x * gw, p->to.z + rv.z * gw, p->to.y + rv.y * gw);
        BEAMQUAD(right);
        BEAMQUAD(vup);
#undef BEAMQUAD
        glEnd();

        if (p->color == 1) {
          glColor4f(0.3f, 0.6f, 1.0f, alpha);
        } else {
          glColor4f(1.0f, 0.5f, 0.2f, alpha);
        }
        glBegin(GL_QUADS);
#define BEAMQUAD(rv)                                                           \
  glVertex3f(p->from.x + rv.x * width, p->from.z + rv.z * width,               \
             p->from.y + rv.y * width);                                        \
  glVertex3f(p->from.x - rv.x * width, p->from.z - rv.z * width,               \
             p->from.y - rv.y * width);                                        \
  glVertex3f(p->to.x - rv.x * width, p->to.z - rv.z * width,                   \
             p->to.y - rv.y * width);                                          \
  glVertex3f(p->to.x + rv.x * width, p->to.z + rv.z * width,                   \
             p->to.y + rv.y * width);
        BEAMQUAD(right);
        BEAMQUAD(vup);
#undef BEAMQUAD
        glEnd();

        xtraverts += 16;
      };
      pp = &p->next;
    };
  };

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
};

void renderspheres(int time) {
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBindTexture(GL_TEXTURE_2D, 4);

  for (sphere *p, **pp = &slist; p = *pp;) {
    glPushMatrix();
    float size = p->size / p->max;
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f - size);
    glTranslatef(p->o.x, p->o.z, p->o.y);
    glRotatef(lastmillis / 5.0f, 1, 1, 1);
    glScalef(p->size, p->size, p->size);
    glCallList(1);
    glScalef(0.8f, 0.8f, 0.8f);
    glCallList(1);
    glPopMatrix();
    xtraverts += 12 * 6 * 2;

    if (p->size > p->max) {
      *pp = p->next;
      p->next = sempty;
      sempty = p;
    } else {
      p->size += time / 100.0f;
      pp = &p->next;
    };
  };

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
};

char *entnames[] = {
    "none?",          "light",      "playerstart", "shells",
    "bullets",        "rockets",    "railgunammo", "nails",
    "energy",         "health",     "healthboost", "greenarmour",
    "yellowarmour",   "quaddamage", "teleport",    "teledest",
    "mapmodel",       "monster",    "trigger",     "jumppad",
    "particlesource", "?",          "?",           "?",
};

void loadsky(char *basename) {
  static string lastsky = "";
  if (strcmp(lastsky, basename) == 0)
    return;
  char *side[] = {"ft", "bk", "lf", "rt", "dn", "up"};
  int texnum = 14;
  loopi(6) {
    sprintf_sd(name)("packages/%s_%s.dds", basename, side[i]);
    int xs, ys;
    if (!installtex(texnum + i, path(name), xs, ys, true))
      conoutf("Could not load sky textures");
  };
  strcpy_s(lastsky, basename);
};

COMMAND(loadsky, ARG_1STR);
static int __ad_loadsky =
    (addcommanddetail("loadsky", "Loads a skybox texture set"), 0);

float cursordepth = 0.9f;
GLint viewport[4];
GLdouble mm[16], pm[16];
vec worldpos;

void readmatrices() {
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, mm);
  glGetDoublev(GL_PROJECTION_MATRIX, pm);
};

/// Why the fuck does this shit exist?
float depthcorrect(float d) { return (d <= 1 / 256.0f) ? d * 256 : d; };

void readdepth(int w, int h) {
  glReadPixels(w / 2, h / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &cursordepth);
  double worldx = 0, worldy = 0, worldz = 0;
  gluUnProject(w / 2, h / 2, depthcorrect(cursordepth), mm, pm, viewport,
               &worldx, &worldz, &worldy);
  worldpos.x = (float)worldx;
  worldpos.y = (float)worldy;
  worldpos.z = (float)worldz;
  vec r = {(float)mm[0], (float)mm[4], (float)mm[8]};
  vec u = {(float)mm[1], (float)mm[5], (float)mm[9]};
  setorient(r, u);
};

void drawicon(float tx, float ty, int x, int y, int s) {
  glBindTexture(GL_TEXTURE_2D, 5);
  glBegin(GL_QUADS);
  tx /= 192;
  ty /= 192;
  float o = 1 / 3.0f;
  glTexCoord2f(tx, ty);
  glVertex2i(x, y);
  glTexCoord2f(tx + o, ty);
  glVertex2i(x + s, y);
  glTexCoord2f(tx + o, ty + o);
  glVertex2i(x + s, y + s);
  glTexCoord2f(tx, ty + o);
  glVertex2i(x, y + s);
  glEnd();
  xtraverts += 4;
};

void invertperspective() {
  GLdouble inv[16];
  memset(inv, 0, sizeof(inv));

  inv[0 * 4 + 0] = 1.0 / pm[0 * 4 + 0];
  inv[1 * 4 + 1] = 1.0 / pm[1 * 4 + 1];
  inv[2 * 4 + 3] = 1.0 / pm[3 * 4 + 2];
  inv[3 * 4 + 2] = -1.0;
  inv[3 * 4 + 3] = pm[2 * 4 + 2] / pm[3 * 4 + 2];

  glLoadMatrixd(inv);
};

VARP(crosshairsize, 0, 15, 50);
static int __ad_crosshairsize =
    (addcommanddetail("crosshairsize", "Crosshair size in pixels"), 0);

int dblend = 0;
int lastdamage = 0;

void damageblend(int n) {
  if (lastmillis - lastdamage < 300) {
    dblend = min(dblend + n / 2, 100);
  } else {
    dblend = min(n, 100);
  }
  lastdamage = lastmillis;
};

VARP(showstats, 0, 0, 1);
static int __ad_showstats =
    (addcommanddetail("showstats", "Toggles performance stats display"), 0);
VARP(crosshairfx, 0, 1, 1);
static int __ad_crosshairfx =
    (addcommanddetail("crosshairfx", "Toggles crosshair dynamic effects"), 0);

VARP(crosshair_r, 0, 255, 255);
static int __ad_crosshair_r =
    (addcommanddetail("crosshair_r", "Crosshair red color component"), 0);
VARP(crosshair_g, 0, 255, 255);
static int __ad_crosshair_g =
    (addcommanddetail("crosshair_g", "Crosshair green color component"), 0);
VARP(crosshair_b, 0, 255, 255);
static int __ad_crosshair_b =
    (addcommanddetail("crosshair_b", "Crosshair blue color component"), 0);
VARP(crosshair_a, 0, 255, 255);
static int __ad_crosshair_a =
    (addcommanddetail("crosshair_a", "Crosshair alpha transparency"), 0);
void crosshair(int r, int g, int b, int a) {
  setvar("crosshair_r", r);
  setvar("crosshair_g", g);
  setvar("crosshair_b", b);
  setvar("crosshair_a", a);
};
COMMAND(crosshair, ARG_4INT);
static int __ad_crosshair =
    (addcommanddetail("crosshair", "Sets crosshair color (r, g, b, a)"), 0);

void roundedbox(int x1, int y1, int x2, int y2, int r) {
  int segs = 6;
  glBegin(GL_QUADS);
  glVertex2i(x1 + r, y1 + r);
  glVertex2i(x2 - r, y1 + r);
  glVertex2i(x2 - r, y2 - r);
  glVertex2i(x1 + r, y2 - r);
  glEnd();
  glBegin(GL_QUADS);
  glVertex2i(x1 + r, y1);
  glVertex2i(x2 - r, y1);
  glVertex2i(x2 - r, y1 + r);
  glVertex2i(x1 + r, y1 + r);
  glEnd();
  glBegin(GL_QUADS);
  glVertex2i(x1 + r, y2 - r);
  glVertex2i(x2 - r, y2 - r);
  glVertex2i(x2 - r, y2);
  glVertex2i(x1 + r, y2);
  glEnd();
  glBegin(GL_QUADS);
  glVertex2i(x1, y1 + r);
  glVertex2i(x1 + r, y1 + r);
  glVertex2i(x1 + r, y2 - r);
  glVertex2i(x1, y2 - r);
  glEnd();
  glBegin(GL_QUADS);
  glVertex2i(x2 - r, y1 + r);
  glVertex2i(x2, y1 + r);
  glVertex2i(x2, y2 - r);
  glVertex2i(x2 - r, y2 - r);
  glEnd();
  float step = 90.0f / segs;
  for (int c = 0; c < 4; c++) {
    float ax, ay;
    float start;
    switch (c) {
    case 0:
      ax = x1 + r;
      ay = y1 + r;
      start = 180.0f;
      break;
    case 1:
      ax = x2 - r;
      ay = y1 + r;
      start = 270.0f;
      break;
    case 2:
      ax = x2 - r;
      ay = y2 - r;
      start = 0.0f;
      break;
    case 3:
      ax = x1 + r;
      ay = y2 - r;
      start = 90.0f;
      break;
    }
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(ax, ay);
    for (int i = 0; i <= segs; i++) {
      float a = (start + step * i) * M_PI / 180.0f;
      glVertex2f(ax + r * cosf(a), ay + r * sinf(a));
    }
    glEnd();
  }
}

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert,
                bool underwater, bool inlava) {
  readmatrices();

  glDisable(GL_DEPTH_TEST);
  invertperspective();
  glPushMatrix();
  glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
  glEnable(GL_BLEND);
  glDepthMask(GL_FALSE);

  if (spectator && !speclook && !screenshotmode) {
    glEnable(GL_TEXTURE_2D);
    dynent *t = getspectarget();
    if (t && t->name[0]) {
      glColor4ub(255, 255, 255, 255);
      draw_text(t->name, (VIRTW - text_width(t->name)) / 2, 60, 2);
    }
  }

  if (dblend) {
    int since = lastmillis - lastdamage;
    float alpha = 0.0f;
    if (since < 100) {
      alpha = (since / 100.0f) * (dblend / 100.0f) * 0.3f;
    } else if (since < 500) {
      alpha = (1.0f - (since - 100) / 400.0f) * (dblend / 100.0f) * 0.3f;
    }
    if (alpha > 0.01f) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBegin(GL_QUADS);
      glColor4f(1.0f, 0.0f, 0.0f, alpha);
      glVertex2i(0, 0);
      glVertex2i(VIRTW, 0);
      glVertex2i(VIRTW, VIRTH);
      glVertex2i(0, VIRTH);
      glEnd();
    }
    if (since > 500)
      dblend = 0;
  };

  if (underwater) {
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    glColor3d(0.9f, 0.5f, 0.0f);
    glVertex2i(0, 0);
    glVertex2i(VIRTW, 0);
    glVertex2i(VIRTW, VIRTH);
    glVertex2i(0, VIRTH);
    glEnd();
  };

  if (inlava) {
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    glColor3d(0.8f, 0.0f, 0.0f);
    glVertex2i(0, 0);
    glVertex2i(VIRTW, 0);
    glVertex2i(VIRTW, VIRTH);
    glVertex2i(0, VIRTH);
    glEnd();
  };

  glEnable(GL_TEXTURE_2D);

  char *command = getcurcommand();
  {
    extern bool saycommandon;
    extern int saycommand_start;
    extern int saycommand_end;
    extern int commandpos;
    int showbg = 0;
    if (saycommandon) {
      int elapsed = lastmillis - saycommand_start;
      showbg = elapsed < 200 ? elapsed * 127 / 200 : 127;
    } else if (saycommand_end && lastmillis - saycommand_end < 300) {
      int elapsed = lastmillis - saycommand_end;
      showbg = 127 - elapsed * 127 / 300;
    };
    if (showbg > 0) {
      int tw =
          command ? text_width(command) + text_width("$  ") : text_width("$  ");
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4ub(0, 0, 0, showbg);
      roundedbox(10, 1388, 30 + tw + 10, 1420 + FONTH + 4, 6);
      glEnable(GL_TEXTURE_2D);
    };
    if (command) {
      draw_textf("$ %s", 20, 1400, 2, command);
      int len = strlen(command);
      int pos = commandpos > len ? len : commandpos;
      int cx = 20 + text_width("$ ");
      if (pos > 0)
        cx += text_width(command) - text_width(command + pos);
      int cw = FONTH / 3;
      if (command[pos])
        cw = text_width(command + pos) - text_width(command + pos + 1);
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4ub(255, 255, 255, 200);
      glBegin(GL_QUADS);
      glVertex2i(cx, 1400 + FONTH - 3);
      glVertex2i(cx + cw, 1400 + FONTH - 3);
      glVertex2i(cx + cw, 1400 + FONTH);
      glVertex2i(cx, 1400 + FONTH);
      glEnd();
      glEnable(GL_TEXTURE_2D);
    };

    if (saycommandon) {
      int ns = get_numsuggestions();
      int sel = get_sel_suggestion();

      const char *help_name = NULL;
      if (ns > 0 && sel >= 0) {
        help_name = get_suggestion(sel);
      } else {
        help_name = get_command_word();
      }

      if (ns > 0) {
        int visible =
            ns > MAX_VISIBLE_SUGGESTIONS ? MAX_VISIBLE_SUGGESTIONS : ns;
        int vis_start = 0;
        if (sel >= MAX_VISIBLE_SUGGESTIONS)
          vis_start = sel - MAX_VISIBLE_SUGGESTIONS + 1;

        int max_w = 0;
        loopi(visible) {
          string sugline;
          sprintf_s(sugline)("/%s", get_suggestion(vis_start + i));
          int w = text_width(sugline);
          if (w > max_w)
            max_w = w;
        }
        max_w += 24;

        int item_h = FONTH + 24;
        int total_h = visible * item_h + 12;
        int sx = 18;
        int sy = 1388 - total_h;

        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(0, 0, 0, showbg);
        roundedbox(10, sy, sx + max_w + 8, sy + total_h, 6);

        loopi(visible) {
          int item_y = sy + 6 + i * item_h;
          if (vis_start + i == sel) {
            glColor4ub(60, 120, 200, showbg);
            roundedbox(12, item_y, sx + max_w + 4, item_y + item_h, 4);
          }
          glEnable(GL_TEXTURE_2D);
          draw_textf("/%s", sx + 2, item_y + 12, 2,
                     get_suggestion(vis_start + i));
          glDisable(GL_TEXTURE_2D);
        }
        glEnable(GL_TEXTURE_2D);

        if (help_name) {
          const char *sig = getargsig_byname(help_name);
          const char *detail = getdetail(help_name);
          string help_line;
          help_line[0] = 0;
          if (detail) {
            if (sig && sig[0])
              sprintf_s(help_line)("/%s %s  %s", help_name, sig, detail);
            else
              sprintf_s(help_line)("/%s  %s", help_name, detail);
          } else if (sig && sig[0]) {
            sprintf_s(help_line)("/%s %s", help_name, sig);
          }
          if (help_line[0]) {
            int help_y = sy - FONTH - 24;
            if (help_y < 0)
              help_y = 0;
            int help_w = text_width(help_line) + 24;
            glDisable(GL_TEXTURE_2D);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4ub(0, 0, 0, showbg);
            roundedbox(10, help_y, 12 + help_w, help_y + FONTH + 24, 6);
            glEnable(GL_TEXTURE_2D);
            draw_text(help_line, 18, help_y + 12, 2, 255);
          }
        }
      } else if (help_name) {
        const char *sig = getargsig_byname(help_name);
        const char *detail = getdetail(help_name);
        if ((sig && sig[0]) || detail) {
          string help_line;
          if (detail) {
            if (sig && sig[0])
              sprintf_s(help_line)("/%s %s  %s", help_name, sig, detail);
            else
              sprintf_s(help_line)("/%s  %s", help_name, detail);
          } else {
            sprintf_s(help_line)("/%s %s", help_name, sig);
          }
          int help_y = 1388 - FONTH - 24;
          if (help_y < 0)
            help_y = 0;
          int help_w = text_width(help_line) + 24;
          glDisable(GL_TEXTURE_2D);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          glColor4ub(0, 0, 0, showbg);
          roundedbox(10, help_y, 12 + help_w, help_y + FONTH + 24, 6);
          glEnable(GL_TEXTURE_2D);
          draw_text(help_line, 18, help_y + 12, 2, 255);
        }
      }
    }
  }

  renderscores();
  if (!rendermenu() && !intermission && !screenshotmode && !editmode &&
      !spectator) {
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = VIRTW / 2.0f, cy = VIRTH / 2.0f;
    float thick = 3.0f;

    bool moving =
        !player1->ads &&
        (player1->move || player1->strafe || player1->attacking ||
         (player1->vel.x * player1->vel.x + player1->vel.y * player1->vel.y +
          player1->vel.z * player1->vel.z) > 1.0f);

    float targetGap = moving ? crosshairsize * 0.4f : 2.0f;
    float targetLen = moving ? crosshairsize * 1.3f : crosshairsize * 0.9f;

    static float curGap = 2.0f, curLen = crosshairsize * 0.9f;
    float decay = curtime * 0.008f;
    if (decay > 1.0f)
      decay = 1.0f;
    curGap += (targetGap - curGap) * decay;
    curLen += (targetLen - curLen) * decay;

    glColor4ub(crosshair_r, crosshair_g, crosshair_b, crosshair_a);

    glBegin(GL_QUADS);
    glVertex2f(cx - thick / 2, cy - curGap - curLen);
    glVertex2f(cx + thick / 2, cy - curGap - curLen);
    glVertex2f(cx + thick / 2, cy - curGap);
    glVertex2f(cx - thick / 2, cy - curGap);

    glVertex2f(cx - thick / 2, cy + curGap);
    glVertex2f(cx + thick / 2, cy + curGap);
    glVertex2f(cx + thick / 2, cy + curGap + curLen);
    glVertex2f(cx - thick / 2, cy + curGap + curLen);

    glVertex2f(cx - curGap - curLen, cy - thick / 2);
    glVertex2f(cx - curGap, cy - thick / 2);
    glVertex2f(cx - curGap, cy + thick / 2);
    glVertex2f(cx - curGap - curLen, cy + thick / 2);

    glVertex2f(cx + curGap, cy - thick / 2);
    glVertex2f(cx + curGap + curLen, cy - thick / 2);
    glVertex2f(cx + curGap + curLen, cy + thick / 2);
    glVertex2f(cx + curGap, cy + thick / 2);
    glEnd();

    glEnable(GL_TEXTURE_2D);
  };

  if (!screenshotmode) {
    if (player1->quadmillis > 0) {
      int secs = (player1->quadmillis + 999) / 1000;
      string timestr;
      sprintf_s(timestr)("Quad %d", secs);
      int tw = text_width(timestr);
      if (tw > 0) {
        int pad = FONTH / 8;
        int xpos = VIRTW - tw - pad * 2 - FONTH / 3;
        int ypos = FONTH / 3;
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(0, 0, 0, 127);
        roundedbox(xpos - pad, ypos - pad, xpos + tw + pad,
                   ypos + FONTH + pad + 7, 6);
        glEnable(GL_TEXTURE_2D);
        draw_text(timestr, xpos, ypos, 2, 255);
      };
    };
  };

  glPopMatrix();

  glPushMatrix();
  glOrtho(0, VIRTW * 4 / 3, VIRTH * 4 / 3, 0, -1, 1);
  renderconsole();

  if (showstats) {
    glPopMatrix();
    glPushMatrix();
    glOrtho(0, VIRTW * 3 / 2, VIRTH * 3 / 2, 0, -1, 1);
    int sx = 2980, sy = 79, sw = 400, sh = 325, sr = 12;
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(0, 0, 0, 127);
    roundedbox(sx, sy, sx + sw, sy + sh, sr);
    glEnable(GL_TEXTURE_2D);
    draw_textf("FPS: %d", 3005, 100, 2, curfps);
    draw_textf("QS: %d", 3005, 170, 2, nquads);
    draw_textf("CTV: %d", 3005, 240, 2, curvert);
    draw_textf("XVS: %d", 3005, 310, 2, xtraverts);
  };

  glPopMatrix();

  {
    if (editmode || spectator || screenshotmode)
      goto skiphud;
    dynent *d = player1;
    if (spectator && !speclook) {
      dynent *t = getspectarget();
      if (t)
        d = t;
      else
        goto skiphud;
    };
    if (d->state != CS_ALIVE)
      goto skiphud;
    glPushMatrix();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glColor4ub(0, 0, 0, 50);
    glVertex2i(0, 1540);
    glVertex2i(VIRTW, 1540);
    glColor4ub(0, 0, 0, 160);
    glVertex2i(VIRTW, VIRTH);
    glVertex2i(0, VIRTH);
    glEnd();
    glColor4ub(10, 10, 10, 130);
    roundedbox(20, 1580, 225, 1750, 20);

    if (d->armour) {
      glColor4ub(10, 10, 10, 130);
      roundedbox(230, 1580, 434, 1750, 20);
    }

    glColor4ub(10, 10, 10, 130);
    roundedbox(VIRTW - 260, 1580, VIRTW - 30, 1750, 20);
    glEnable(GL_TEXTURE_2D);

    glBlendFunc(GL_ONE, GL_ONE);
    glColor4ub(255, 255, 255, 255);
    drawicon(128, 128, 30, 1620, 64);
    glColor4ub(10, 10, 10, 130);
    draw_textf("%d", 105, 1660, 2, d->health);

    if (d->armour) {
      drawicon((float)(d->armourtype * 64), 0, 240, 1620, 64);
      draw_textf("%d", 315, 1660, 2, d->armour);
    }

    if (d->gunselect == GUN_NAILGUN) {
      glBindTexture(GL_TEXTURE_2D, 11);
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2i(VIRTW - 250, 1620);
      glTexCoord2f(1, 0);
      glVertex2i(VIRTW - 250 + 64, 1620);
      glTexCoord2f(1, 1);
      glVertex2i(VIRTW - 250 + 64, 1620 + 64);
      glTexCoord2f(0, 1);
      glVertex2i(VIRTW - 250, 1620 + 64);
      glEnd();
      xtraverts += 4;
    } else if (d->gunselect == GUN_LIGHTGUN || d->gunselect == GUN_RL ||
               d->gunselect == GUN_RAILGUN) {
      glBindTexture(GL_TEXTURE_2D, 12);
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2i(VIRTW - 250, 1620);
      glTexCoord2f(1, 0);
      glVertex2i(VIRTW - 250 + 64, 1620);
      glTexCoord2f(1, 1);
      glVertex2i(VIRTW - 250 + 64, 1620 + 64);
      glTexCoord2f(0, 1);
      glVertex2i(VIRTW - 250, 1620 + 64);
      glEnd();
      xtraverts += 4;
    } else {
      int g = d->gunselect;
      int r = 64;
      if (g > 4) {
        g -= 5;
        r = 192;
      } else if (g > 2) {
        g -= 3;
        r = 128;
      };
      drawicon((float)(g * 64), (float)r, VIRTW - 250, 1620, 64);
    }
    draw_textf("%d", VIRTW - 170, 1660, 2, d->ammo[d->gunselect]);

    glDisable(GL_TEXTURE_2D);
    {
      int bx = d->armour ? 440 : 230;
      glColor4ub(10, 10, 10, 130);
      roundedbox(bx, 1580, bx + 205, 1750, 20);
      int inner = 6;
      int bw = 205 - inner * 2;
      int bh = 1750 - 1580 - inner * 2;
      int by = 1580 + inner;
      float pct =
          d->boostmillis > 0 ? 1.0f - (float)d->boostmillis / 2500.0f : 1.0f;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4ub(180, 180, 180, d->boostmillis > 0 ? 20 : 54);
      roundedbox(bx + inner, by, bx + inner + (int)(bw * pct), by + bh, 12);
    };
    glEnable(GL_TEXTURE_2D);
    {
      int bx = d->armour ? 440 : 230;
      glBlendFunc(GL_ONE, GL_ONE);
      glColor4ub(255, 255, 255, 255);
      glBindTexture(GL_TEXTURE_2D, 10);
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2i(bx + 10, 1620);
      glTexCoord2f(1, 0);
      glVertex2i(bx + 74, 1620);
      glTexCoord2f(1, 1);
      glVertex2i(bx + 74, 1684);
      glTexCoord2f(0, 1);
      glVertex2i(bx + 10, 1684);
      glEnd();
      int boostval = d->boostmillis > 0
                         ? (int)((1.0f - (float)d->boostmillis / 2500.0f) * 100)
                         : 100;
      draw_textf("%d", bx + 85, 1660, 2, boostval);
    };
    glPopMatrix();
  };
skiphud:

  if (editmode && !screenshotmode) {
    extern int closestent();
    extern int lasttype, lasttex;
    extern string mapname[2048][2];
    int e = closestent();
    glPushMatrix();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(10, 10, 10, 180);
    roundedbox(20, 1490, VIRTW - 20, 1580, 10);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
    {
      char *texnames[] = {"floor", "wall", "ceiling", "upper"};
      char *tname = texnames[lasttype < 0 || lasttype > 3 ? 0 : lasttype];
      char *fname = mapname[lasttex][0];
      if (fname[0]) {
        string displayname;
        strcpy_s(displayname, fname);
        char *dot = strrchr(displayname, '.');
        if (dot) {
          *dot = '\0';
          strcat_s(displayname, ".dds");
        }
        draw_textf("[tex %s #%d]: %s", 30, 1496, 2, tname, lasttex,
                   displayname);
      } else
        draw_textf("[tex %s #%d]", 30, 1496, 2, tname, lasttex);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(10, 10, 10, 180);
    roundedbox(20, 1590, VIRTW - 20, 1680, 10);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
    {
      extern bool selset;
      extern int selh;
      extern block sel;
      if (selset)
        draw_textf("[selection]: (x: %d, y: %d, z: %d)", 30, 1596, 2, sel.xs,
                   sel.ys, selh);
      else
        draw_text("[selection]: (none)", 30, 1596, 2);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(10, 10, 10, 180);
    roundedbox(20, 1690, VIRTW - 20, 1780, 10);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
    if (e >= 0) {
      entity &ent = ents[e];
      draw_textf("[%s] pos:(%d,%d,%d) a1:%d a2:%d a3:%d a4:%d", 30, 1696, 2,
                 entnames[ent.type], ent.x, ent.y, ent.z, ent.attr1, ent.attr2,
                 ent.attr3, ent.attr4);
    } else {
      draw_text("[ ]", 30, 1696, 2);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(10, 10, 10, 140);
    roundedbox(VIRTW - 220, 10, VIRTW - 10, 100, 10);
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
    {
      extern int entselectmode;
      draw_text(entselectmode ? "MANL" : "PROX", VIRTW - 205, 22, 2);
    }
    glPopMatrix();
  };

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
};

void rendereditentities() {
  if (!editmode)
    return;
  extern int closestent();
  int sel = closestent();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_FALSE);

  float sz = 0.3f;

  loopv(ents) {
    entity &e = ents[i];
    if (e.type == NOTUSED)
      continue;
    if (OUTBORD(e.x, e.y))
      continue;

    float x = (float)e.x;
    float y = (float)e.y;
    float z = (float)S(e.x, e.y)->floor + 0.5f;

    if (sel >= 0 && i == sel)
      glColor4f(1.0f, 0.0f, 0.0f, 0.4f);
    else
      glColor4f(0.5f, 0.5f, 0.5f, 0.3f);

    glBegin(GL_QUADS);
    glVertex3f(x - sz, z + sz, y - sz);
    glVertex3f(x + sz, z + sz, y - sz);
    glVertex3f(x + sz, z + sz, y + sz);
    glVertex3f(x - sz, z + sz, y + sz);

    glVertex3f(x - sz, z - sz, y - sz);
    glVertex3f(x + sz, z - sz, y - sz);
    glVertex3f(x + sz, z - sz, y + sz);
    glVertex3f(x - sz, z - sz, y + sz);

    glVertex3f(x - sz, z - sz, y + sz);
    glVertex3f(x + sz, z - sz, y + sz);
    glVertex3f(x + sz, z + sz, y + sz);
    glVertex3f(x - sz, z + sz, y + sz);

    glVertex3f(x - sz, z - sz, y - sz);
    glVertex3f(x + sz, z - sz, y - sz);
    glVertex3f(x + sz, z + sz, y - sz);
    glVertex3f(x - sz, z + sz, y - sz);

    glVertex3f(x - sz, z - sz, y - sz);
    glVertex3f(x - sz, z - sz, y + sz);
    glVertex3f(x - sz, z + sz, y + sz);
    glVertex3f(x - sz, z + sz, y - sz);

    glVertex3f(x + sz, z - sz, y - sz);
    glVertex3f(x + sz, z - sz, y + sz);
    glVertex3f(x + sz, z + sz, y + sz);
    glVertex3f(x + sz, z + sz, y - sz);
    glEnd();
    xtraverts += 24;
  };

  glDepthMask(GL_TRUE);
};
