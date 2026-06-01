#include "cube.h"

extern bool los(float lx, float ly, float lz, float bx, float by, float bz,
                vec &v);

dvector bots;
int numbots = 0;
VAR(botdifficulty, -1, 4, 4);
VAR(botamount, 1, 4, 16);

static const char *botnames[] = {
    "Cerelo", "Diaso", "Ceria",  "Deathly", "Ra",      "Va",    "Never",
    "Abu",    "Re",    "Why",    "Lucky",   "Lano",    "Cliff", "Cobra",
    "Liner",  "Chiba", "Dragon", "Sabre",   "Koffman", "Stuff", "Bones",
    "Xor",    "Snuff", "Sniff",  "Pain",    "Time",    "Fake",  "Headup"};

static const int numbotnames = sizeof(botnames) / sizeof(botnames[0]);

static void genbotname(char *buf) {
  const char *base = botnames[rnd(numbotnames)];
  int suffix = 1;
  for (;;) {
    if (suffix == 1) {
      bool taken = !strcmp(base, player1->name);
      if (!taken)
        loopv(players) if (players[i] && !strcmp(base, players[i]->name)) {
          taken = true;
          break;
        }
      if (!taken)
        loopv(bots) if (!strcmp(base, bots[i]->name)) {
          taken = true;
          break;
        }
      if (!taken) {
        sprintf_s(buf)("%s", base);
        return;
      }
    } else {
      sprintf_sd(tmp)("%s (%d)", base, suffix);
      bool taken = !strcmp(tmp, player1->name);
      if (!taken)
        loopv(players) if (players[i] && !strcmp(tmp, players[i]->name)) {
          taken = true;
          break;
        }
      if (!taken)
        loopv(bots) if (!strcmp(tmp, bots[i]->name)) {
          taken = true;
          break;
        }
      if (!taken) {
        sprintf_s(buf)("%s", tmp);
        return;
      }
    }
    suffix++;
  }
}

dvector &getbots() { return bots; }

#define MAXGIBS 48

struct gib {
  vec o, vel;
  int lifetime;
  const char *mdl;
  bool inuse;
};

static gib gibs[MAXGIBS];

static void initgibs() { loopi(MAXGIBS) gibs[i].inuse = false; }

static void spawngibs(vec &pos, int count) {
  int n = 0;
  loopi(MAXGIBS) {
    if (gibs[i].inuse)
      continue;
    gibs[i].o = pos;
    gibs[i].vel.x = (float)(rnd(61) - 30);
    gibs[i].vel.y = (float)(rnd(61) - 30);
    gibs[i].vel.z = (float)(rnd(21) + 5);
    gibs[i].lifetime = 4000 + rnd(2000);
    gibs[i].mdl = rnd(2) ? "gibc" : "gibh";
    gibs[i].inuse = true;
    if (++n >= count)
      break;
  }
}

static void updategibs() {
  loopi(MAXGIBS) {
    if (!gibs[i].inuse)
      continue;
    float dt = curtime / 1000.0f;
    gibs[i].vel.z -= 18.0f * dt;
    gibs[i].o.x += gibs[i].vel.x * dt;
    gibs[i].o.y += gibs[i].vel.y * dt;
    gibs[i].o.z += gibs[i].vel.z * dt;
    int x = (int)gibs[i].o.x, y = (int)gibs[i].o.y;
    if (!OUTBORD(x, y)) {
      float floor = S(x, y)->floor;
      if (gibs[i].o.z < floor) {
        gibs[i].o.z = floor;
        gibs[i].vel.x *= -0.3f;
        gibs[i].vel.y *= -0.3f;
        gibs[i].vel.z *= -0.3f;
        if (fabs(gibs[i].vel.z) < 1.0f)
          gibs[i].vel.z = 0;
      }
    }
    gibs[i].lifetime -= curtime;
    if (gibs[i].lifetime <= 0)
      gibs[i].inuse = false;
  }
}

static void rendergibs() {
  loopi(MAXGIBS) {
    if (!gibs[i].inuse)
      continue;
    rendermodel((char *)gibs[i].mdl, 0, 1, 0, 1.5f, gibs[i].o.x, gibs[i].o.z,
                gibs[i].o.y, (float)(i * 37), 0, false, 1.2f, 100.0f, 0, 0);
  }
}

static int lastammorefill = 0;

void botclear() {
  initgibs();
  loopv(bots) gp()->dealloc(bots[i], sizeof(dynent));
  bots.setsize(0);
  numbots = 0;
  lastammorefill = 0;
}

static void normalise(dynent *m, float angle) {
  while (m->yaw < angle - 180.0f)
    m->yaw += 360.0f;
  while (m->yaw > angle + 180.0f)
    m->yaw -= 360.0f;
}

static void botaction(dynent *m) {
  dynent *enemy = NULL;
  float bestdist = 1e10f;

  if (player1->state == CS_ALIVE) {
    vdist(dist, v, m->o, player1->o);
    if (dist < bestdist) {
      bestdist = dist;
      enemy = player1;
    }
  }

  if (botdifficulty >= 0) {
    loopv(players) {
      dynent *o = players[i];
      if (!o || o->state != CS_ALIVE)
        continue;
      vdist(dist, v, m->o, o->o);
      if (dist < bestdist) {
        bestdist = dist;
        enemy = o;
      }
    }

    loopv(bots) {
      dynent *o = bots[i];
      if (o == m || o->state != CS_ALIVE)
        continue;
      vdist(dist, v, m->o, o->o);
      if (dist < bestdist) {
        bestdist = dist;
        enemy = o;
      }
    }
  }

  if (!enemy) {
    m->move = 0;
    m->strafe = 0;
    return;
  }
  m->enemy = enemy;

  vdist(disttoenemy, vectoenemy, m->o, m->enemy->o);

  if (disttoenemy < 8.0f) {
    if (m->gunselect != GUN_CSAW)
      m->gunselect = GUN_CSAW;
  } else if (!m->ammo[m->gunselect]) {
    if (m->ammo[GUN_RL])
      m->gunselect = GUN_RL;
    else if (m->ammo[GUN_CG])
      m->gunselect = GUN_CG;
    else if (m->ammo[GUN_SG])
      m->gunselect = GUN_SG;
    else if (m->ammo[GUN_RIFLE])
      m->gunselect = GUN_RIFLE;
    else
      m->gunselect = GUN_CSAW;
  } else if (m->gunselect == GUN_CSAW && disttoenemy > 15.0f) {
    if (m->ammo[GUN_RL])
      m->gunselect = GUN_RL;
    else if (m->ammo[GUN_CG])
      m->gunselect = GUN_CG;
    else if (m->ammo[GUN_SG])
      m->gunselect = GUN_SG;
    else if (m->ammo[GUN_RIFLE])
      m->gunselect = GUN_RIFLE;
  }

  float enemyyaw =
      -(float)atan2(m->enemy->o.x - m->o.x, m->enemy->o.y - m->o.y) / PI * 180 +
      180;

  float aimspread = (4 - botdifficulty) * 6.0f;
  if (aimspread > 0) {
    enemyyaw += (rnd(101) - 50) / 100.0f * aimspread;
  }
  m->targetyaw = enemyyaw;

  float enemypitch = atan2(m->enemy->o.z - m->o.z, disttoenemy) * 180 / PI;
  if (aimspread > 0) {
    enemypitch += (rnd(101) - 50) / 100.0f * aimspread * 0.5f;
  }
  m->pitch = enemypitch;

  normalise(m, m->targetyaw);
  float turnrate = curtime * (0.15f + botdifficulty * 0.05f);
  float yawdiff = m->targetyaw - m->yaw;
  if (fabs(yawdiff) < turnrate)
    m->yaw = m->targetyaw;
  else if (yawdiff > 0)
    m->yaw += turnrate;
  else
    m->yaw -= turnrate;

  if (m->blocked) {
    m->blocked = false;
    if (m->gunselect == GUN_CSAW) {
      m->jumpnext = false;
    } else if (!rnd(3) && lastmillis - m->lastmove > 1500) {
      m->jumpnext = true;
      m->lastmove = lastmillis;
    } else {
      m->targetyaw += 90 + rnd(180);
    }
  }

  m->move = 1;
  m->strafe = 0;

  if (disttoenemy < 4 && m->gunselect != GUN_CSAW) {
    m->move = -1;
  } else if (disttoenemy < 12) {
    m->strafe = (rnd(3) - 1);
  }

  if (botdifficulty >= 0) {
    int maxrange = 48 + botdifficulty * 20;
    int reacttime = 200 - botdifficulty * 40;
    if (reacttime < 50)
      reacttime = 50;
    if (disttoenemy < maxrange && m->enemy->state == CS_ALIVE) {
      int attacktime = lastmillis - m->lastaction;
      if (attacktime > reacttime) {
        vec tmp;
        if (los(m->o.x, m->o.y, m->o.z - 0.2f, m->enemy->o.x, m->enemy->o.y,
                m->enemy->o.z, tmp)) {
          m->attacktarget = m->enemy->o;
          m->attacking = true;
          shoot(m, m->attacktarget);
        }
      }
    }
  }

  moveplayer(m, 1, false);
}

void botthink() {
  updategibs();
  loopv(bots) {
    dynent *b = bots[i];
    if (b->state == CS_ALIVE) {
      if (lastmillis - lastammorefill > 15000) {
        lastammorefill = lastmillis;
        b->ammo[GUN_SG] = max(b->ammo[GUN_SG], 10);
        b->ammo[GUN_CG] = max(b->ammo[GUN_CG], 40);
        b->ammo[GUN_RL] = max(b->ammo[GUN_RL], 8);
        b->ammo[GUN_RIFLE] = max(b->ammo[GUN_RIFLE], 8);
      }
      botaction(b);
    } else if (b->state == CS_DEAD && lastmillis - b->lastaction > 5000) {
      spawnplayer(b);
      b->health = max(50 + botdifficulty * 25, 1);
      b->armour = max(botdifficulty * 25, 0);
      b->state = CS_ALIVE;
      b->monsterstate = M_HOME;
      b->enemy = player1;
      b->lastmove = 0;
      b->move = 1;
      b->attacking = false;
    }
  }
}

void botrender() {
  rendergibs();
  loopv(bots) {
    if (bots[i]->state == CS_DEAD)
      continue;
    float saved = bots[i]->maxspeed;
    bots[i]->maxspeed = 20.0f;
    renderclient(bots[i], false, "monster/player", false, 1.25f);
    bots[i]->maxspeed = saved;
  }
}

void botpain(dynent *m, int damage, dynent *d) {
  if (m->state == CS_DEAD)
    return;

  if (d != m && d->state == CS_ALIVE)
    m->enemy = d;

  if ((m->health -= damage) <= 0) {
    m->state = CS_DEAD;
    m->lastaction = lastmillis;
    m->attacking = false;
    m->deaths++;
    spawngibs(m->o, 4);
    if (d == player1) {
      player1->frags++;
      addmsg(1, 2, SV_FRAGS, player1->frags);
    } else if (d->monsterstate && d->mtype == -1) {
      d->frags++;
    }
    conoutf("%s fragged %s", d->name, m->name);
  } else {
    playsound(S_PAIN1 + rnd(5), &m->o);
  }
}

extern bool isdedicated;

static void spawnonebot() {
  dynent *b = newdynent();
  spawnplayer(b);
  b->monsterstate = M_HOME;
  b->mtype = -1;
  b->enemy = player1;
  b->lastmove = 0;
  b->move = 1;
  b->targetyaw = b->yaw;
  b->gunselect = GUN_SG + rnd(4);
  b->maxspeed = max(28.0f + botdifficulty * 4.0f, 16.0f);
  b->health = max(50 + botdifficulty * 25, 1);
  b->armour = max(botdifficulty * 25, 0);
  loopi(NUMGUNS) b->ammo[i] = 100;
  b->anger = 0;
  b->lastupdate = lastmillis;
  b->state = CS_ALIVE;
  b->pitch = 0;
  b->roll = 0;
  numbots++;
  genbotname(b->name);
  bots.add(b);
  conoutf("%s spawned.", b->name);
}

void addbotcmd(int n) {
  if (isdedicated) {
    conoutf("bots not supported on dedicated servers");
    return;
  }
  if (multiplayer()) {
    conoutf("only the server admin can spawn bots");
    return;
  }
  if (gamemode < 0) {
    conoutf("bots only work in multiplayer modes");
    return;
  }
  if (n < 1)
    n = 1;
  loopi(n) spawnonebot();
}

void addbotspawn() { addbotcmd(botamount); }

COMMANDN(addbot, addbotcmd, ARG_1INT);
COMMANDN(addbotspawn, addbotspawn, ARG_NONE);
