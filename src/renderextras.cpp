#include "cube.h"

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
  beam *next;
};
beam beams[MAXBEAMS], *blist = NULL, *bempty = NULL;
bool binitb = false;

void newbeam(vec &from, vec &to, int duration) {
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
        glColor4f(1.0f, 0.2f, 0.05f, alpha * 0.3f);
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

        glColor4f(1.0f, 0.5f, 0.2f, alpha);
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
    "none?",
    "light",
    "playerstart",
    "shells",
    "bullets",
    "rockets",
    "riflerounds",
    "health",
    "healthboost",
    "greenarmour",
    "yellowarmour",
    "quaddamage",
    "teleport",
    "teledest",
    "mapmodel",
    "monster",
    "trigger",
    "jumppad",
    "?",
    "?",
    "?",
    "?",
    "?",
};

void loadsky(char *basename) {
  static string lastsky = "";
  if (strcmp(lastsky, basename) == 0)
    return;
  char *side[] = {"ft", "bk", "lf", "rt", "dn", "up"};
  int texnum = 14;
  loopi(6) {
    sprintf_sd(name)("packages/%s_%s.jpg", basename, side[i]);
    int xs, ys;
    if (!installtex(texnum + i, path(name), xs, ys, true))
      conoutf("could not load sky textures");
  };
  strcpy_s(lastsky, basename);
};

COMMAND(loadsky, ARG_1STR);

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

int dblend = 0;
int lastdamage = 0;

void damageblend(int n) {
  if (lastmillis - lastdamage < 300) {
    dblend = min(dblend + n/2, 100);
  } else {
    dblend = min(n, 100);
  }
  lastdamage = lastmillis;
};

VAR(hidestats, 0, 0, 1);
VARP(crosshairfx, 0, 1, 1);

void roundedbox(int x1, int y1, int x2, int y2, int r) {
    int segs = 6;
    glBegin(GL_QUADS);
    glVertex2i(x1+r, y1+r);
    glVertex2i(x2-r, y1+r);
    glVertex2i(x2-r, y2-r);
    glVertex2i(x1+r, y2-r);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2i(x1+r, y1);
    glVertex2i(x2-r, y1);
    glVertex2i(x2-r, y1+r);
    glVertex2i(x1+r, y1+r);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2i(x1+r, y2-r);
    glVertex2i(x2-r, y2-r);
    glVertex2i(x2-r, y2);
    glVertex2i(x1+r, y2);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2i(x1, y1+r);
    glVertex2i(x1+r, y1+r);
    glVertex2i(x1+r, y2-r);
    glVertex2i(x1, y2-r);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2i(x2-r, y1+r);
    glVertex2i(x2, y1+r);
    glVertex2i(x2, y2-r);
    glVertex2i(x2-r, y2-r);
    glEnd();
    float step = 90.0f / segs;
    for (int c = 0; c < 4; c++) {
        float ax, ay;
        float start;
        switch (c) {
            case 0: ax = x1+r; ay = y1+r; start = 180.0f; break;
            case 1: ax = x2-r; ay = y1+r; start = 270.0f; break;
            case 2: ax = x2-r; ay = y2-r; start = 0.0f;   break;
            case 3: ax = x1+r; ay = y2-r; start = 90.0f;  break;
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
                bool underwater) {
  readmatrices();

  glDisable(GL_DEPTH_TEST);
  invertperspective();
  glPushMatrix();
  glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
  glEnable(GL_BLEND);
  glDepthMask(GL_FALSE);

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

  glEnable(GL_TEXTURE_2D);

  char *command = getcurcommand();
  char *player = playerincrosshair();
  if (command)
    draw_textf("$ %s_", 20, 1400, 2, command);
  else if (player)
    draw_text(player, 20, 1400, 2);

  renderscores();
  if (!rendermenu()) {
    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, 1);
    glBegin(GL_QUADS);
    glColor3ub(255, 255, 255);
    if (crosshairfx) {
      if (player1->gunwait)
        glColor3ub(128, 128, 128);
      else if (player1->health <= 25)
        glColor3ub(255, 0, 0);
      else if (player1->health <= 50)
        glColor3ub(255, 128, 0);
    };
    float chsize = (float)crosshairsize;
    glTexCoord2d(0.0, 0.0);
    glVertex2f(VIRTW / 2 - chsize, VIRTH / 2 - chsize);
    glTexCoord2d(1.0, 0.0);
    glVertex2f(VIRTW / 2 + chsize, VIRTH / 2 - chsize);
    glTexCoord2d(1.0, 1.0);
    glVertex2f(VIRTW / 2 + chsize, VIRTH / 2 + chsize);
    glTexCoord2d(0.0, 1.0);
    glVertex2f(VIRTW / 2 - chsize, VIRTH / 2 + chsize);
    glEnd();
  };

  glPopMatrix();

  glPushMatrix();
  glOrtho(0, VIRTW * 4 / 3, VIRTH * 4 / 3, 0, -1, 1);
  renderconsole();

  if (!hidestats) {
    glPopMatrix();
    glPushMatrix();
    glOrtho(0, VIRTW * 3 / 2, VIRTH * 3 / 2, 0, -1, 1);
    draw_textf("FPS: %d", 3000, 100, 2, curfps);
    draw_textf("QS: %d", 3000, 170, 2, nquads);
    draw_textf("CTV: %d", 3000, 240, 2, curvert);
    draw_textf("XVS: %d", 3000, 310, 2, xtraverts);
  };

  glPopMatrix();

  if (player1->state == CS_ALIVE && !editmode) {
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

    if (player1->armour) {
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
    draw_textf("%d", 105, 1660, 2, player1->health);

    if (player1->armour) {
      drawicon((float)(player1->armourtype * 64), 0, 240, 1620, 64);
      draw_textf("%d", 315, 1660, 2, player1->armour);
    }

    int g = player1->gunselect;
    int r = 64;
    if (g > 2) {
      g -= 3;
      r = 128;
    };
    drawicon((float)(g * 64), (float)r, VIRTW - 250, 1620, 64);
    draw_textf("%d", VIRTW - 170, 1660, 2, player1->ammo[player1->gunselect]);

    glPopMatrix();
  };

  if (editmode) {
    extern int closestent();
    int e = closestent();
    glPushMatrix();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(10, 10, 10, 160);
    roundedbox(20, 1660, VIRTW - 20, 1760, 10);
    glColor4ub(255, 255, 255, 255);
    if (e >= 0) {
      entity &ent = ents[e];
      draw_textf("[%s] pos:(%d,%d,%d)  a1:%d a2:%d a3:%d a4:%d",
                 30, 1690, 2, entnames[ent.type],
                 ent.x, ent.y, ent.z,
                 ent.attr1, ent.attr2, ent.attr3, ent.attr4);
    } else {
      draw_text("[ ]", 30, 1690, 2);
    }
    glPopMatrix();
  };

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
};
