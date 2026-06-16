#include "../include/cube.h"

const int MAXPARTICLES = 10500;
struct parttype {
  float r, g, b;
  int gr, tex;
  float sz;
};
static parttype parttypes[] = {
    {0.7f, 0.6f, 0.3f, 2, 3, 0.06f},  // 0:  yellow sparks
    {0.5f, 0.5f, 0.5f, 20, 7, 0.15f}, // 1:  grey small smoke
    {0.2f, 0.2f, 1.0f, 20, 3, 0.08f}, // 2:  blue edit entities
    {1.0f, 0.1f, 0.1f, 1, 7, 0.06f},  // 3:  red blood
    {1.0f, 0.8f, 0.8f, 20, 6, 1.2f},  // 4:  yellow fireball1
    {0.5f, 0.5f, 0.5f, 20, 7, 0.6f},  // 5:  grey big smoke
    {1.0f, 1.0f, 1.0f, 20, 8, 1.2f},  // 6:  white fireball2
    {1.0f, 1.0f, 1.0f, 20, 9, 1.2f},  // 7:  white fireball3
    {1.0f, 0.1f, 0.1f, 0, 7, 0.2f},   // 8:  red demotrack
    {1.0f, 1.0f, 1.0f, 0, 7, 0.05f},  // 9:  white pickup glow
    {0.4f, 0.8f, 1.0f, 0, 6, 0.06f},  // 10: water bubble
    {1.0f, 1.0f, 1.0f, 20, 7, 0.04f}, // 11: water mist
    {0.5f, 0.1f, 0.1f, 0, 7, 0.015f}, // 12: small dark red
};

struct particle {
  vec o, d;
  int fade, type;
  int millis;
  uchar cr, cg, cb;
  particle *next;
};

particle particles[MAXPARTICLES], *parlist = NULL, *parempty = NULL;
bool parinit = false;

VARP(maxparticles, 100, 2000, MAXPARTICLES - 500);
static int __ad_maxparticles = (addcommanddetail("maxparticles", "Maximum number of particles"), 0);

void newparticlecol(vec &o, vec &d, int fade, int type, uchar r, uchar g,
                    uchar b) {
  if (!parinit) {
    loopi(MAXPARTICLES) {
      particles[i].next = parempty;
      parempty = &particles[i];
    };
    parinit = true;
  };
  if (parempty) {
    particle *p = parempty;
    parempty = p->next;
    p->o = o;
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = lastmillis;
    p->cr = r;
    p->cg = g;
    p->cb = b;
    p->next = parlist;
    parlist = p;
  };
};

void newparticle(vec &o, vec &d, int fade, int type) {
  if (type >= 0 && type < (int)(sizeof(parttypes) / sizeof(parttypes[0])))
    newparticlecol(o, d, fade, type, (uchar)(parttypes[type].r * 255),
                   (uchar)(parttypes[type].g * 255),
                   (uchar)(parttypes[type].b * 255));
};

VAR(demotracking, 0, 0, 1);
static int __ad_demotracking = (addcommanddetail("demotracking", "Toggles demo particle tracking"), 0);
VARP(particlesize, 20, 100, 500);
static int __ad_particlesize = (addcommanddetail("particlesize", "Particle size scale"), 0);

vec right, up;

void setorient(vec &r, vec &u) {
  right = r;
  up = u;
};

void render_particles(int time) {
  if (demoplayback && demotracking) {
    vec nom = {0, 0, 0};
    newparticle(player1->o, nom, 100000000, 8);
  };

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
  glDisable(GL_FOG);

  int numrender = 0;

  for (particle *p, **pp = &parlist; p = *pp;) {
    parttype *pt = &parttypes[p->type];

    glBindTexture(GL_TEXTURE_2D, pt->tex);
    glBegin(GL_QUADS);

    float mul = 1.0f, sscale = 1.0f;
    if (p->type == 10) {
      float total = (float)(p->fade + (lastmillis - p->millis));
      float t = total > 0 ? (float)(lastmillis - p->millis) / total : 0.0f;
      if (t < 0.2f)
        mul = t / 0.2f;
      else if (t > 0.6f)
        mul = 1.0f - (t - 0.6f) / 0.4f;
      if (t > 0.5f)
        sscale = 1.0f + ((t - 0.5f) / 0.5f) * 0.3f;
    };
    glColor3d(p->cr / 255.0f * mul, p->cg / 255.0f * mul, p->cb / 255.0f * mul);
    float sz = pt->sz * particlesize / 100.0f * sscale;
    // perf varray?
    glTexCoord2f(0.0, 1.0);
    glVertex3d(p->o.x + (-right.x + up.x) * sz, p->o.z + (-right.y + up.y) * sz,
               p->o.y + (-right.z + up.z) * sz);
    glTexCoord2f(1.0, 1.0);
    glVertex3d(p->o.x + (right.x + up.x) * sz, p->o.z + (right.y + up.y) * sz,
               p->o.y + (right.z + up.z) * sz);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(p->o.x + (right.x - up.x) * sz, p->o.z + (right.y - up.y) * sz,
               p->o.y + (right.z - up.z) * sz);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(p->o.x + (-right.x - up.x) * sz, p->o.z + (-right.y - up.y) * sz,
               p->o.y + (-right.z - up.z) * sz);
    glEnd();
    xtraverts += 4;

    if (numrender++ > maxparticles || (p->fade -= time) < 0) {
      *pp = p->next;
      p->next = parempty;
      parempty = p;
    } else {
      if (pt->gr)
        p->o.z -=
            ((lastmillis - p->millis) / 3.0f) * curtime / (pt->gr * 10000);
      vec a = p->d;
      vmul(a, time);
      vdiv(a, 20000.0f);
      vadd(p->o, a);
      pp = &p->next;
    };
  };

  glEnable(GL_FOG);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
};

void particle_splash(int type, int num, int fade, vec &p) {
  loopi(num) {
    const int radius = type == 5 ? 50 : 150;
    int x, y, z;
    do {
      x = rnd(radius * 2) - radius;
      y = rnd(radius * 2) - radius;
      z = rnd(radius * 2) - radius;
    } while (x * x + y * y + z * z > radius * radius);
    vec d = {(float)x, (float)y, (float)z};
    newparticle(p, d, rnd(fade * 3), type);
  };
};

void particle_trail(int type, int fade, vec &s, vec &e) {
  vdist(d, v, s, e);
  vdiv(v, d * 2 + 0.1f);
  vec p = s;
  loopi((int)d * 2) {
    vadd(p, v);
    vec d = {float(rnd(11) - 5), float(rnd(11) - 5), float(rnd(11) - 5)};
    newparticle(p, d, rnd(fade) + fade, type);
  };
};
