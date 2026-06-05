#include "../include/cube.h"

vector<entity> ents;

char *entmdlnames[] = {
    "shells", "bullets",  "rockets",  "rrounds", "nails",
    "health", "boost",  "g_armour", "y_armour", "quad",    "teleporter",
};

int triggertime = 0;

void renderent(entity &e, char *mdlname, float z, float yaw, int frame = 0,
               int numf = 1, int basetime = 0, float speed = 10.0f) {
  rendermodel(mdlname, frame, numf, 0, 1.1f, e.x, z + S(e.x, e.y)->floor, e.y,
              yaw, 0, false, 1.0f, speed, 0, basetime);
};

void renderentities() {
  if (lastmillis > triggertime + 1000)
    triggertime = 0;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type == MAPMODEL) {
      mapmodelinfo &mmi = getmminfo(e.attr2);
      if (!&mmi)
        continue;
      rendermodel(mmi.name, 0, 1, e.attr4, (float)mmi.rad, e.x,
                  (float)S(e.x, e.y)->floor + mmi.zoff + e.attr3, e.y,
                  (float)((e.attr1 + 7) - (e.attr1 + 7) % 15), 0, false, 1.0f,
                  10.0f, mmi.snap);
    } else {
      if (OUTBORD(e.x, e.y))
        continue;
      if (e.type != CARROT) {
        if (!e.spawned && e.type != TELEPORT)
          continue;
        if (e.type < I_SHELLS || e.type > TELEPORT)
          continue;
        renderent(e, entmdlnames[e.type - I_SHELLS], 1.0f, lastmillis / 10.0f);
        if (e.spawned && (rnd(8) == 0)) {
          float f = S(e.x, e.y)->floor;
          vec pos = {(float)e.x + (rnd(21) - 10) * 0.1f,
                     (float)e.y + (rnd(21) - 10) * 0.1f,
                     f + 0.8f + (rnd(11) - 5) * 0.08f};
          vec vel = {(rnd(21) - 10) * 0.6f, (rnd(21) - 10) * 0.6f,
                     3.0f + rnd(8) * 0.5f};
          static const uchar glowcols[10][3] = {
              {160, 130, 60}, {130, 130, 70}, {160, 100, 50},
              {80, 140, 180}, {120, 120, 200}, {160, 80, 80},
              {80, 160, 80},  {80, 150, 80},   {160, 150, 60},
              {160, 80, 160},
          };
          newparticlecol(
              pos, vel, rnd(300) + 400, 9, glowcols[e.type - I_SHELLS][0],
              glowcols[e.type - I_SHELLS][1], glowcols[e.type - I_SHELLS][2]);
        };
      } else
        switch (e.attr2) {
        case 1:
        case 3:
          continue;

        case 2:
        case 0:
          if (!e.spawned)
            continue;
          renderent(e, "carrot", 1.0f, lastmillis / (e.attr2 ? 1.0f : 10.0f));
          break;

        case 4:
          renderent(e, "switch2", 3, (float)e.attr3 * 90,
                    (!e.spawned && !triggertime) ? 1 : 0,
                    (e.spawned || !triggertime) ? 1 : 2, triggertime, 1050.0f);
          break;
        case 5:
          renderent(e, "switch1", -0.15f, (float)e.attr3 * 90,
                    (!e.spawned && !triggertime) ? 30 : 0,
                    (e.spawned || !triggertime) ? 1 : 30, triggertime, 35.0f);
          break;
        };
    };
  };
};

struct itemstat {
  int add, max, sound;
} itemstats[] = {
    10,  50,  S_ITEMAMMO,   20,  100, S_ITEMAMMO,   5,     25,    S_ITEMAMMO,
    5,   25,  S_ITEMAMMO,   15,  75,  S_ITEMAMMO,   25,  100, S_ITEMHEALTH,
    50,  200, S_ITEMHEALTH, 100, 100, S_ITEMARMOUR, 150, 150, S_ITEMARMOUR,
    20000, 30000, S_ITEMPUP,
};

void baseammo(int gun) { player1->ammo[gun] = itemstats[gun - 1].add * 2; };

void radditem(int i, int &v) {
  itemstat &is = itemstats[ents[i].type - I_SHELLS];
  ents[i].spawned = false;
  v += is.add;
  if (v > is.max)
    v = is.max;
  playsoundc(is.sound);
};

void realpickup(int n, dynent *d) {
  if (m_infected && d->team[0] && !strcmp(d->team, "INFD")) {
    if (ents[n].type == I_HEALTH) { radditem(n, d->health); }
    else if (ents[n].type == I_BOOST) { radditem(n, d->health); };
    return;
  };
  switch (ents[n].type) {
  case I_SHELLS:
    radditem(n, d->ammo[1]);
    break;
  case I_BULLETS:
    radditem(n, d->ammo[2]);
    break;
  case I_ROCKETS:
    radditem(n, d->ammo[3]);
    break;
  case I_ROUNDS:
    radditem(n, d->ammo[4]);
    break;
  case I_NAILS:
    radditem(n, d->ammo[5]);
    break;
  case I_HEALTH:
    radditem(n, d->health);
    break;
  case I_BOOST:
    radditem(n, d->health);
    break;

  case I_GREENARMOUR:
    radditem(n, d->armour);
    d->armourtype = A_GREEN;
    break;

  case I_YELLOWARMOUR:
    radditem(n, d->armour);
    d->armourtype = A_YELLOW;
    break;

  case I_QUAD:
    radditem(n, d->quadmillis);
    conoutf("You got quad damage.");
    break;
  };
};

void additem(int i, int &v, int spawnsec) {
  if (v < itemstats[ents[i].type - I_SHELLS].max) {
    addmsg(1, 3, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec);
    ents[i].spawned = false;
  };
};

void teleport(int n, dynent *d) {
  int e = -1, tag = ents[n].attr1, beenhere = -1;
  for (;;) {
    e = findentity(TELEDEST, e + 1);
    if (e == beenhere || e < 0) {
      conoutf("No teleport destination for tag %d", tag);
      return;
    };
    if (beenhere < 0)
      beenhere = e;
    if (ents[e].attr2 == tag) {
      d->o.x = ents[e].x;
      d->o.y = ents[e].y;
      d->o.z = ents[e].z;
      d->yaw = ents[e].attr1;
      d->pitch = 0;
      d->vel.x = d->vel.y = d->vel.z = 0;
      entinmap(d);
      playsoundc(S_TELEPORT);
      break;
    };
  };
};

void pickup(int n, dynent *d) {
  if (m_infected && d->team[0] && !strcmp(d->team, "INFD")) {
    if (ents[n].type == I_HEALTH || ents[n].type == I_BOOST) {
      int np = 1;
      loopv(players) if (players[i]) np++;
      additem(n, d->health, ents[n].type == I_BOOST ? 60 : np * 5);
    };
    return;
  };
  int np = 1;
  loopv(players) if (players[i]) np++;
  if (np < 3)
    np = 4;
  else if (np > 4)
    np = 2;
  else
    np = 3;

  int ammo = np * 2;
  switch (ents[n].type) {
  case I_SHELLS:
    additem(n, d->ammo[1], ammo);
    break;
  case I_BULLETS:
    additem(n, d->ammo[2], ammo);
    break;
  case I_ROCKETS:
    additem(n, d->ammo[3], ammo);
    break;
  case I_ROUNDS:
    additem(n, d->ammo[4], ammo);
    break;
  case I_NAILS:
    additem(n, d->ammo[5], ammo);
    break;
  case I_HEALTH:
    additem(n, d->health, np * 5);
    break;
  case I_BOOST:
    additem(n, d->health, 60);
    break;

  case I_GREENARMOUR:
    // (100h/100g only absorbs 166 damage)
    if (d->armourtype == A_YELLOW && d->armour > 66)
      break;
    additem(n, d->armour, 20);
    break;

  case I_YELLOWARMOUR:
    additem(n, d->armour, 20);
    break;

  case I_QUAD:
    additem(n, d->quadmillis, 60);
    break;

  case CARROT:
    ents[n].spawned = false;
    triggertime = lastmillis;
    trigger(ents[n].attr1, ents[n].attr2,
            false); // needs to go over server for multiplayer
    break;

  case TELEPORT: {
    static int lastteleport = 0;
    if (lastmillis - lastteleport < 500)
      break;
    lastteleport = lastmillis;
    teleport(n, d);
    break;
  };

  case JUMPPAD: {
    static int lastjumppad = 0;
    if (lastmillis - lastjumppad < 300)
      break;
    lastjumppad = lastmillis;
    vec v = {(int)(char)ents[n].attr3 / 10.0f, (int)(char)ents[n].attr2 / 10.0f,
             ents[n].attr1 / 10.0f};
    player1->vel.z = 0;
    vadd(player1->vel, v);
    playsoundc(S_JUMPPAD);
    break;
  };
  };
};

void checkitems() {
  if (editmode)
    return;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type == NOTUSED)
      continue;
    if (!ents[i].spawned && e.type != TELEPORT && e.type != JUMPPAD)
      continue;
    if (OUTBORD(e.x, e.y))
      continue;
    vec v = {e.x, e.y, S(e.x, e.y)->floor + player1->eyeheight};
    vdist(dist, t, player1->o, v);
    if (dist < (e.type == TELEPORT ? 4 : 2.5))
      pickup(i, player1);
  };
};

void checkquad(int time) {
  if (player1->quadmillis && (player1->quadmillis -= time) < 0) {
    player1->quadmillis = 0;
    playsoundc(S_PUPOUT);
    conoutf("Quad damage is over.");
  };
};

void putitems(
    uchar *&p) // puts items in network stream and also spawns them locally
{
  loopv(ents) if ((ents[i].type >= I_SHELLS && ents[i].type <= I_QUAD) ||
                  ents[i].type == CARROT) {
    putint(p, i);
    ents[i].spawned = true;
  };
};

void resetspawns() { loopv(ents) ents[i].spawned = false; };
void setspawn(uint i, bool on) {
  if (i < (uint)ents.length())
    ents[i].spawned = on;
};
