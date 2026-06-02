#include "../include/cube.h"

static int runchan = -1;

/// Collide monster with player.
bool plcollide(dynent *d, dynent *o, float &headspace, float &hi, float &lo) {
  if (o->state != CS_ALIVE)
    return true;
  const float r = o->radius + d->radius;
  if (fabs(o->o.x - d->o.x) < r && fabs(o->o.y - d->o.y) < r) {
    if (d->o.z - d->eyeheight < o->o.z - o->eyeheight) {
      if (o->o.z - o->eyeheight < hi)
        hi = o->o.z - o->eyeheight - 1;
    } else if (o->o.z + o->aboveeye > lo)
      lo = o->o.z + o->aboveeye + 1;

    if (fabs(o->o.z - d->o.z) < o->aboveeye + d->eyeheight)
      return false;
    if (d->monsterstate && d->mtype >= 0)
      return false;
    headspace = d->o.z - o->o.z - o->aboveeye - d->eyeheight;
    if (headspace < 0)
      headspace = 10;
  };
  return true;
};

bool cornertest(int mip, int x, int y, int dx, int dy, int &bx, int &by,
                int &bs) {
  sqr *w = wmip[mip];
  int sz = ssize >> mip;
  bool stest = SOLID(SWS(w, x + dx, y, sz)) && SOLID(SWS(w, x, y + dy, sz));
  mip++;
  x /= 2;
  y /= 2;
  if (SWS(wmip[mip], x, y, ssize >> mip)->type == CORNER) {
    bx = x << mip;
    by = y << mip;
    bs = 1 << mip;
    return cornertest(mip, x, y, dx, dy, bx, by, bs);
  };
  return stest;
};

void mmcollide(dynent *d, float &hi, float &lo) // collide with a mapmodel
{
  loopv(ents) {
    entity &e = ents[i];
    if (e.type != MAPMODEL)
      continue;
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if (!&mmi || !mmi.h)
      continue;
    const float r = mmi.rad + d->radius;
    if (fabs(e.x - d->o.x) < r && fabs(e.y - d->o.y) < r) {
      float mmz = (float)(S(e.x, e.y)->floor + mmi.zoff + e.attr3);
      if (d->o.z - d->eyeheight < mmz) {
        if (mmz < hi)
          hi = mmz;
      } else if (mmz + mmi.h > lo)
        lo = mmz + mmi.h;
    };
  };
};

// all collision happens here
// spawn is a dirty side effect used in spawning
// drop & rise are supplied by the physics below to indicate gravity/push for
// current mini-timestep

bool collide(dynent *d, bool spawn, float drop, float rise) {
  const float fx1 = d->o.x - d->radius;
  const float fy1 = d->o.y - d->radius;
  const float fx2 = d->o.x + d->radius;
  const float fy2 = d->o.y + d->radius;
  const int x1 = fast_f2nat(fx1);
  const int y1 = fast_f2nat(fy1);
  const int x2 = fast_f2nat(fx2);
  const int y2 = fast_f2nat(fy2);
  float hi = 127, lo = -128;
  float minfloor = (d->monsterstate && !spawn && d->health > 100 && d->mtype >= 0)
                       ? d->o.z - d->eyeheight - 4.5f
                       : -1000.0f;

  for (int x = x1; x <= x2; x++)
    for (int y = y1; y <= y2; y++) // collide with map
    {
      if (OUTBORD(x, y))
        return false;
      sqr *s = S(x, y);
      float ceil = s->ceil;
      float floor = s->floor;
      switch (s->type) {
      case SOLID:
        return false;

      case CORNER: {
        int bx = x, by = y, bs = 1;
        if (x == x1 && y == y1 && cornertest(0, x, y, -1, -1, bx, by, bs) &&
                fx1 - bx + fy1 - by <= bs ||
            x == x2 && y == y1 && cornertest(0, x, y, 1, -1, bx, by, bs) &&
                fx2 - bx >= fy1 - by ||
            x == x1 && y == y2 && cornertest(0, x, y, -1, 1, bx, by, bs) &&
                fx1 - bx <= fy2 - by ||
            x == x2 && y == y2 && cornertest(0, x, y, 1, 1, bx, by, bs) &&
                fx2 - bx + fy2 - by >= bs)
          return false;
        break;
      };

      case FHF: // FIXME: too simplistic collision with slopes, makes it feels
                // like tiny stairs
        floor -= (s->vdelta + S(x + 1, y)->vdelta + S(x, y + 1)->vdelta +
                  S(x + 1, y + 1)->vdelta) /
                 16.0f;
        break;

      case CHF:
        ceil += (s->vdelta + S(x + 1, y)->vdelta + S(x, y + 1)->vdelta +
                 S(x + 1, y + 1)->vdelta) /
                16.0f;
      };
      if (ceil < hi)
        hi = ceil;
      if (floor > lo)
        lo = floor;
      if (floor < minfloor)
        return false;
    };

  if (hi - lo < d->eyeheight + d->aboveeye)
    return false;

  float headspace = 10;
  loopv(players) {
    dynent *o = players[i];
    if (!o || o == d)
      continue;
    if (!plcollide(d, o, headspace, hi, lo))
      return false;
  };
  if (d != player1)
    if (!plcollide(d, player1, headspace, hi, lo))
      return false;
  dvector &v = getmonsters();
  loopv(v) if (!vreject(d->o, v[i]->o, 7.0f) && d != v[i] &&
               !plcollide(d, v[i], headspace, hi, lo)) return false;
  dvector &bv = getbots();
  loopv(bv) if (!vreject(d->o, bv[i]->o, 7.0f) && d != bv[i] &&
                !plcollide(d, bv[i], headspace, hi, lo)) return false;
  headspace -= 0.01f;

  mmcollide(d, hi, lo);

  if (spawn) {
    d->o.z = lo + d->eyeheight;
    d->onfloor = true;
  } else {
    const float space = d->o.z - d->eyeheight - lo;
    if (space < 0) {
      if (space > -0.01)
        d->o.z = lo + d->eyeheight;
      else if (space > -1.26f)
        d->o.z += rise;
      else
        return false;
    } else {
      d->o.z -= min(min(drop, space), headspace); // gravity
    };

    const float space2 = hi - (d->o.z + d->aboveeye);
    if (space2 < 0) {
      if (space2 < -0.1)
        return false;            // hack alert!
      d->o.z = hi - d->aboveeye; // glue to ceiling
      d->vel.z = 0;              // cancel out jumping velocity
    };

    d->onfloor = d->o.z - d->eyeheight - lo < 0.001f;
  };
  return true;
}

float rad(float x) { return x * 3.14159f / 180; };

VARP(maxroll, 0, 3, 20);

int physicsfraction = 0, physicsrepeat = 0;
const int MINFRAMETIME = 20; // physics always simulated at 50fps or better

void physicsframe() // optimally schedule physics frames inside the graphics
                    // frames
{
  if (curtime >= MINFRAMETIME) {
    int faketime = curtime + physicsfraction;
    physicsrepeat = faketime / MINFRAMETIME;
    physicsfraction = faketime - physicsrepeat * MINFRAMETIME;
  } else {
    physicsrepeat = 1;
  };
};

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and
// multiplayer prediction) local is false for multiplayer prediction

void moveplayer(dynent *pl, int moveres, bool local, int curtime) {
  if (noclip) {
    vec d;
    d.x = (float)(pl->move * cos(rad(pl->yaw - 90)));
    d.y = (float)(pl->move * sin(rad(pl->yaw - 90)));
    d.z = (float)(pl->move * sin(rad(pl->pitch)));
    d.x *= (float)cos(rad(pl->pitch));
    d.y *= (float)cos(rad(pl->pitch));
    d.x += (float)(pl->strafe * cos(rad(pl->yaw - 180)));
    d.y += (float)(pl->strafe * sin(rad(pl->yaw - 180)));

    const float speed = curtime / 1000.0f * pl->maxspeed;
    vmul(d, speed);
    pl->o.x += d.x;
    pl->o.y += d.y;
    pl->o.z += d.z;
    pl->timeinair = 0;
    pl->onfloor = false;
    pl->moving = d.x != 0 || d.y != 0 || d.z != 0;
    pl->outsidemap = false;
    vmul(pl->vel, 0);
    return;
  };

  const bool water = hdr.waterlevel > pl->o.z - 0.5f;

  vec d; // vector of direction we ideally want to move in

  d.x = (float)(pl->move * cos(rad(pl->yaw - 90)));
  d.y = (float)(pl->move * sin(rad(pl->yaw - 90)));
  d.z = 0;

  if (water) {
    d.x *= (float)cos(rad(pl->pitch));
    d.y *= (float)cos(rad(pl->pitch));
    d.z = (float)(pl->move * sin(rad(pl->pitch)));
  };

  d.x += (float)(pl->strafe * cos(rad(pl->yaw - 180)));
  d.y += (float)(pl->strafe * sin(rad(pl->yaw - 180)));

  const float speed = curtime / (water ? 2000.0f : 1000.0f) * pl->maxspeed;
  const float friction = water ? 20.0f : (pl->onfloor ? 6.0f : 30.0f);

  const float fpsfric = friction / curtime * 20.0f;

  vmul(pl->vel, fpsfric - 1); // slowly apply friction and direction to
                              // velocity, gives a smooth movement
  vadd(pl->vel, d);
  vdiv(pl->vel, fpsfric);
  d = pl->vel;
  vmul(d, speed); // d is now frametime based velocity vector

  pl->blocked = false;
  pl->moving = true;

  if (pl->onfloor || water || (pl->walljump && !pl->onfloor)) {
    if (pl->jumpnext) {
      pl->jumpnext = false;
      if (pl->walljump && !pl->onfloor && !water) {
        pl->vel.z = 1.4f;
        pl->vel.x = pl->wallnormal_x * 1.2f;
        pl->vel.y = pl->wallnormal_y * 1.2f;
        float strafex = pl->strafe * cos(rad(pl->yaw - 180));
        float strafey = pl->strafe * sin(rad(pl->yaw - 180));
        pl->vel.x += strafex * 0.6f;
        pl->vel.y += strafey * 0.6f;
        pl->walljump = false;
        pl->walljumped = true;
        if (local)
          playsoundc(S_JUMP);
      } else {
        pl->vel.z = 1.7f;
        if (water) {
          pl->vel.x /= 8;
          pl->vel.y /= 8;
        };
        if (local)
          playsoundc(S_JUMP);
        else if (pl->monsterstate)
          playsound(S_JUMP, &pl->o);
      }
    } else if (pl->timeinair > 800) {
      if (local)
        playsoundc(S_LAND);
      else if (pl->monsterstate)
        playsound(S_LAND, &pl->o);
    };
    pl->timeinair = 0;
  } else {
    pl->timeinair += curtime;
  };

  const float gravity = 20;
  const float f = 1.0f / moveres;
  float dropf =
      ((gravity - 1) + pl->timeinair / 15.0f); // incorrect, but works fine
  if (water) {
    dropf = 5;
    pl->timeinair = 0;
  }; // float slowly down in water
  const float drop = dropf * curtime / gravity / 100 /
                     moveres; // at high fps, gravity kicks in too fast
  const float rise =
      speed / moveres / 1.2f; // extra smoothness when lifting up stairs

  bool wallcontact = false;
  float wn_x = 0, wn_y = 0;

  loopi(moveres) // discrete steps collision detection & sliding
  {
    // try move forward
    pl->o.x += f * d.x;
    pl->o.y += f * d.y;
    pl->o.z += f * d.z;
    if (collide(pl, false, drop, rise))
      continue;
    // player stuck, try slide along y axis
    pl->blocked = true;
    pl->o.x -= f * d.x;
    if (collide(pl, false, drop, rise)) {
      if (!pl->onfloor) {
        wallcontact = true;
        wn_x = d.x > 0 ? -1.0f : (d.x < 0 ? 1.0f : 0);
        wn_y = 0;
      };
      d.x = 0;
      continue;
    };
    pl->o.x += f * d.x;
    // still stuck, try x axis
    pl->o.y -= f * d.y;
    if (collide(pl, false, drop, rise)) {
      if (!pl->onfloor) {
        wallcontact = true;
        wn_x = 0;
        wn_y = d.y > 0 ? -1.0f : (d.y < 0 ? 1.0f : 0);
      };
      d.y = 0;
      continue;
    };
    pl->o.y += f * d.y;
    // try just dropping down
    pl->moving = false;
    pl->o.x -= f * d.x;
    pl->o.y -= f * d.y;
    if (collide(pl, false, drop, rise)) {
      if (!pl->onfloor) {
        wallcontact = true;
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len > 0.01f) {
          wn_x = -d.x / len;
          wn_y = -d.y / len;
        };
      };
      d.y = d.x = 0;
      continue;
    };
    pl->o.z -= f * d.z;
    break;
  };

  if (pl->onfloor || water) {
    pl->walljump = false;
    pl->walljumped = false;
  } else if (!pl->walljumped) {
    pl->walljump = wallcontact;
    if (wallcontact) {
      pl->wallnormal_x = wn_x;
      pl->wallnormal_y = wn_y;
    };
  };

  if (local) {
    if (pl->onfloor && !water && pl->moving &&
        (pl->move != 0 || pl->strafe != 0)) {
      if (runchan < 0)
        runchan = playsoundloop(S_RUN);
    } else if (runchan >= 0) {
      stopchan(runchan);
      runchan = -1;
    };
  };

  // detect wether player is outside map, used for skipping zbuffer clear mostly

  if (pl->o.x < 0 || pl->o.x >= ssize || pl->o.y < 0 || pl->o.y > ssize) {
    pl->outsidemap = true;
  } else {
    sqr *s = S((int)pl->o.x, (int)pl->o.y);
    pl->outsidemap =
        SOLID(s) || pl->o.z < s->floor - (s->type == FHF ? s->vdelta / 4 : 0) ||
        pl->o.z > s->ceil + (s->type == CHF ? s->vdelta / 4 : 0);
  };

  if (pl->strafe == 0) {
    pl->roll = pl->roll / (1 + (float)sqrt((float)curtime) / 25);
  } else {
    pl->roll += pl->strafe * curtime / -30.0f;
    if (pl->roll > maxroll)
      pl->roll = (float)maxroll;
    if (pl->roll < -maxroll)
      pl->roll = (float)-maxroll;
  };

  if (!pl->inwater && water) {
    playsound(S_SPLASH2, &pl->o);
    pl->vel.z = 0;
  } else if (pl->inwater && !water)
    playsound(S_SPLASH1, &pl->o);
  pl->inwater = water;
};

void moveplayer(dynent *pl, int moveres, bool local) {
  loopi(physicsrepeat)
      moveplayer(pl, moveres, local,
                 i ? curtime / physicsrepeat
                   : curtime - curtime / physicsrepeat * (physicsrepeat - 1));
};
