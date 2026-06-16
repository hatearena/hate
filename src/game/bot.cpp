#include "../include/cube.h"

extern bool los(float lx, float ly, float lz, float bx, float by, float bz,
                vec &v);

dvector bots;
int numbots = 0;

VAR(botdifficulty, -1, 4, 4);
static int __ad_botdifficulty = (addcommanddetail("botdifficulty", "Bot AI difficulty level"), 0);
VAR(botamount, 1, 4, 16);
static int __ad_botamount = (addcommanddetail("botamount", "Number of bots to spawn"), 0);

static int botspawncycle = -1;

static const char *botnames[] = {
    "Cerelo", "Diaso", "Ceria",  "Deathly", "Ra",      "Va",     "Never",
    "Abu",    "Re",    "Why",    "Lucky",   "Lano",    "Cliff",  "Cobra",
    "Liner",  "Chiba", "Dragon", "Sabre",   "Koffman", "Stuff",  "Bones",
    "Xor",    "Snuff", "Sniff",  "Pain",    "Time",    "Fake",   "Headup",
    "MX",     "Moon",  "Wine",   "Tux",     "Crash",   "Threed", "Backlotter",
    "Risco",  "Disco", "Cheque", "Will",    "Who",     "Cares",  "Anyway"};

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

void spawngibs(vec &pos, int count) {
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
      sqr *s = S(x, y);
      float floor = (float)s->floor;
      if (s->type == FHF)
        floor -= s->vdelta / 4.0f;
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
  botspawncycle = -1;
}

static bool botoutside(dynent *b);
static bool findbotspawn(dynent *b);

static void normalise(dynent *m, float angle) {
  while (m->yaw < angle - 180.0f)
    m->yaw += 360.0f;
  while (m->yaw > angle + 180.0f)
    m->yaw -= 360.0f;
}

static void botaction(dynent *m) {
  if (botoutside(m)) {
    m->vel.x = m->vel.y = m->vel.z = 0;
    m->timeinair = 0;
    if (!findbotspawn(m))
      entinmap(m);
    moveplayer(m, 20, true);
    return;
  }
  dynent *enemy = NULL;
  float bestdist = 1e10f;

  if (player1->state == CS_ALIVE && !screenshotmode) {
    if (!m_teammode || !player1->team[0] || !m->team[0] ||
        strcmp(m->team, player1->team)) {
      vdist(dist, v, m->o, player1->o);
      if (dist < bestdist) {
        bestdist = dist;
        enemy = player1;
      }
    }
  }

  if (botdifficulty >= 0) {
    loopv(players) {
      dynent *o = players[i];
      if (!o || o->state != CS_ALIVE)
        continue;
      if (m_teammode && o->team[0] && m->team[0] && !strcmp(m->team, o->team))
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
      if (m_teammode && o->team[0] && m->team[0] && !strcmp(m->team, o->team))
        continue;
      vdist(dist, v, m->o, o->o);
      if (dist < bestdist) {
        bestdist = dist;
        enemy = o;
      }
    }
  }

  if (!enemy) {
    if (lastmillis - m->lastmove > 3000 + rnd(4500)) {
      m->targetyaw = (float)(rnd(360));
      m->lastmove = lastmillis;
    };
    normalise(m, m->targetyaw);
    float turnrate = curtime * 0.5f;
    float yawdiff = m->targetyaw - m->yaw;
    if (fabs(yawdiff) < turnrate)
      m->yaw = m->targetyaw;
    else if (yawdiff > 0)
      m->yaw += turnrate;
    else
      m->yaw -= turnrate;
    m->move = 1;
    m->strafe = 0;
    moveplayer(m, 20, true);
    return;
  }
  m->enemy = enemy;

  vdist(disttoenemy, vectoenemy, m->o, m->enemy->o);

  if (m_infected && m->team[0] && !strcmp(m->team, "INFD")) {
    m->gunselect = GUN_CSAW;
  } else if (m_noitems && m_noitemsrail) {
    m->gunselect = GUN_RAILGUN;
  } else if (disttoenemy < 8.0f) {
    if (m->gunselect != GUN_CSAW)
      m->gunselect = GUN_CSAW;
    } else if (!m->ammo[m->gunselect]) {
    if (m->ammo[GUN_RL])
      m->gunselect = GUN_RL;
    else if (m->ammo[GUN_CG])
      m->gunselect = GUN_CG;
    else if (m->ammo[GUN_LIGHTGUN])
      m->gunselect = GUN_LIGHTGUN;
    else if (m->ammo[GUN_NAILGUN])
      m->gunselect = GUN_NAILGUN;
    else if (m->ammo[GUN_SG])
      m->gunselect = GUN_SG;
    else if (m->ammo[GUN_RAILGUN])
      m->gunselect = GUN_RAILGUN;
    else
      m->gunselect = GUN_CSAW;
  } else if (m->gunselect == GUN_CSAW && disttoenemy > 15.0f) {
    if (m->ammo[GUN_RL])
      m->gunselect = GUN_RL;
    else if (m->ammo[GUN_CG])
      m->gunselect = GUN_CG;
    else if (m->ammo[GUN_LIGHTGUN])
      m->gunselect = GUN_LIGHTGUN;
    else if (m->ammo[GUN_NAILGUN])
      m->gunselect = GUN_NAILGUN;
    else if (m->ammo[GUN_SG])
      m->gunselect = GUN_SG;
    else if (m->ammo[GUN_RAILGUN])
      m->gunselect = GUN_RAILGUN;
  }

  vec tmp;
  bool haslos = los(m->o.x, m->o.y, m->o.z - 0.2f, m->enemy->o.x, m->enemy->o.y,
                    m->enemy->o.z, tmp);

  if (haslos) {
    float enemyyaw =
        -(float)atan2(m->enemy->o.x - m->o.x, m->enemy->o.y - m->o.y) / PI *
            180 +
        180;

    float aimspread = (4 - botdifficulty) * 12.0f;
    if (m_noitems && m_noitemsrail)
      aimspread += 20.0f;
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
    if (m_noitems && m_noitemsrail)
      turnrate *= 0.3f;
    float yawdiff = m->targetyaw - m->yaw;
    if (fabs(yawdiff) < turnrate)
      m->yaw = m->targetyaw;
    else if (yawdiff > 0)
      m->yaw += turnrate;
    else
      m->yaw -= turnrate;
  };

  if (m->blocked) {
    m->blocked = false;
    m->jumpnext = true;
    m->targetyaw += rnd(2) ? 90 : -90;
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

  moveplayer(m, 20, true);
}

void botthink() {
  updategibs();
  loopv(bots) {
    dynent *b = bots[i];
    if (b->state == CS_ALIVE) {
      if (m_infected && b->team[0] && !strcmp(b->team, "INFD")) {
        // whatever
      } else if (lastmillis - lastammorefill > 15000) {
        lastammorefill = lastmillis;
        b->ammo[GUN_SG] = max(b->ammo[GUN_SG], 10);
        b->ammo[GUN_NAILGUN] = max(b->ammo[GUN_NAILGUN], 40);
        b->ammo[GUN_CG] = max(b->ammo[GUN_CG], 40);
        b->ammo[GUN_RL] = max(b->ammo[GUN_RL], 8);
        b->ammo[GUN_RAILGUN] = max(b->ammo[GUN_RAILGUN], 8);
      }
      botaction(b);
    } else if (b->state == CS_DEAD && lastmillis - b->lastaction > 5000) {
      spawnstate(b);
      if (!findbotspawn(b)) {
        b->state = CS_DEAD;
        b->lastaction = lastmillis;
        continue;
      }
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
    const char *mdl = "monster/player";
    if (m_teammode || m_infected) {
      if (bots[i]->team[0] &&
          !strcmp(bots[i]->team, m_infected ? "RES" : "BLUE"))
        mdl = "monster/blueplayer";
      else
        mdl = "monster/redplayer";
    }
    renderclient(bots[i], isteam(player1->team, bots[i]->team), mdl, false,
                 1.25f);
    bots[i]->maxspeed = saved;
  }
  loopv(players) if (players[i] && i >= BOT_CLIENT_BASE) {
    if (players[i]->state == CS_DEAD) continue;
    renderclient(players[i], false, "monster/player", false, 1.25f);
  }
}

void botpain(dynent *m, int damage, dynent *d) {
  if (m->state == CS_DEAD)
    return;

  if (d != m && d->state == CS_ALIVE)
    m->enemy = d;

  if ((m->health -= damage) <= 0) {
    if (m_infected && d && m->team[0] && d->team[0] &&
        strcmp(m->team, "INFD") != 0 && !strcmp(d->team, "INFD")) {
      strn0cpy(m->team, "INFD", 5);
      conoutf("%s has been infected", m->name);
    };
    m->state = CS_DEAD;
    m->lastaction = lastmillis;
    m->attacking = false;
    m->deaths++;
    playsound(S_DEAD, &m->o);
    spawngibs(m->o, 4);
    if (d == player1) {
      player1->frags++;
      addmsg(1, 2, SV_FRAGS, player1->frags);
      playsound(S_KILL);
    } else if (d->monsterstate && d->mtype == -1) {
      d->frags++;
    }
    if (d == m) {
      if (m_infected && m->team[0] && strcmp(m->team, "INFD")) {
        strn0cpy(m->team, "INFD", 5);
        conoutf("%s suicided and became infected.", m->name);
      } else {
        conoutf("%s suicided", m->name);
      };
      m->suicides++;
    } else {
      conoutf("%s fragged %s", d->name, m->name);
    }
  } else {
    playsound(S_PAIN1 + rnd(5), &m->o);
  }
}

extern bool isdedicated;
extern string ctext;

static bool findbotspawn(dynent *b) {
  int e = findentity(PLAYERSTART, botspawncycle + 1);
  if (e < 0)
    e = findentity(PLAYERSTART, 0);
  if (e >= 0) {
    botspawncycle = e;
    b->o.x = ents[e].x;
    b->o.y = ents[e].y;
    b->o.z = ents[e].z;
    b->yaw = ents[e].attr1;
    b->pitch = 0;
    b->roll = 0;
    entinmap(b);
    return true;
  }
  return false;
}

static bool botoutside(dynent *b) {
  const float fx1 = b->o.x - b->radius;
  const float fy1 = b->o.y - b->radius;
  const float fx2 = b->o.x + b->radius;
  const float fy2 = b->o.y + b->radius;
  const int x1 = fast_f2nat(fx1);
  const int y1 = fast_f2nat(fy1);
  const int x2 = fast_f2nat(fx2);
  const int y2 = fast_f2nat(fy2);
  for (int x = x1; x <= x2; x++)
    for (int y = y1; y <= y2; y++) {
      if (OUTBORD(x, y))
        return true;
      sqr *s = S(x, y);
      if (SOLID(s))
        return true;
      if (b->o.z < s->floor - (s->type == FHF ? s->vdelta / 4 : 0) ||
          b->o.z > s->ceil + (s->type == CHF ? s->vdelta / 4 : 0))
        return true;
    }
  return false;
}

static void autoteambot(dynent *d) {
  if (!m_teammode)
    return;
  if (m_infected) {
    strn0cpy(d->team, "RES", 5);
    return;
  }
  int red = 0, blue = 0;
  if (player1 && player1 != d) {
    if (!strcmp(player1->team, "RED"))
      red++;
    else if (!strcmp(player1->team, "BLUE"))
      blue++;
  }
  loopv(players) {
    dynent *o = players[i];
    if (o && o != d) {
      if (!strcmp(o->team, "RED"))
        red++;
      else if (!strcmp(o->team, "BLUE"))
        blue++;
    }
  }
  dvector &bv = getbots();
  loopv(bv) {
    dynent *b = bv[i];
    if (b && b != d) {
      if (!strcmp(b->team, "RED"))
        red++;
      else if (!strcmp(b->team, "BLUE"))
        blue++;
    }
  }
  strn0cpy(d->team, red <= blue ? "RED" : "BLUE", 5);
}

static void spawnonebot() {
  dynent *b = newdynent();
  spawnplayer(b);
  autoteambot(b);
  if (!findbotspawn(b))
    return;
  b->monsterstate = M_HOME;
  b->mtype = -1;
  b->enemy = player1;
  b->lastmove = 0;
  b->move = 1;
  b->targetyaw = b->yaw;
  if (m_infected && b->team[0] && !strcmp(b->team, "INFD")) {
    b->gunselect = GUN_CSAW;
    loopi(NUMGUNS) b->ammo[i] = 0;
    b->ammo[GUN_CSAW] = 1;
  } else {
    b->gunselect = GUN_SG + rnd(6);
    loopi(NUMGUNS) b->ammo[i] = 100;
  };
  b->maxspeed = max(28.0f + botdifficulty * 4.0f, 16.0f);
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
    conoutf("Bots not supported on dedicated servers");
    return;
  }
  extern ENetHost *clienthost;
  if (clienthost) {
    sprintf_sd(buf)("/addbot %d", n);
    strn0cpy(ctext, buf, 80);
    return;
  }
  if (gamemode < 0) {
    conoutf("Bots only work in multiplayer modes");
    return;
  }
  if (n < 1)
    n = 1;
  loopi(n) spawnonebot();
}

void addbotspawn() { addbotcmd(botamount); }

COMMANDN(addbot, addbotcmd, ARG_1INT);
static int __ad_addbot = (addcommanddetail("addbot", "Adds a bot to the game"), 0);
COMMANDN(addbotspawn, addbotspawn, ARG_NONE);
static int __ad_addbotspawn = (addcommanddetail("addbotspawn", "Spawns bot at current position"), 0);
