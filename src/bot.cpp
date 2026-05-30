#include "cube.h"

dvector bots;
int numbots = 0;

dvector &getbots() { return bots; }

void botclear() {
  loopv(bots) gp()->dealloc(bots[i], sizeof(dynent));
  bots.setsize(0);
  numbots = 0;
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

  if (!enemy) {
    m->move = 0;
    m->strafe = 0;
    return;
  }
  m->enemy = enemy;

  vdist(disttoenemy, vectoenemy, m->o, m->enemy->o);
  float enemyyaw =
      -(float)atan2(m->enemy->o.x - m->o.x, m->enemy->o.y - m->o.y) / PI * 180 +
      180;

  m->targetyaw = enemyyaw;
  m->pitch = atan2(m->enemy->o.z - m->o.z, disttoenemy) * 180 / PI;

  normalise(m, m->targetyaw);
  float turnrate = curtime * 0.3f;
  float yawdiff = m->targetyaw - m->yaw;
  if (fabs(yawdiff) < turnrate)
    m->yaw = m->targetyaw;
  else if (yawdiff > 0)
    m->yaw += turnrate;
  else
    m->yaw -= turnrate;

  if (m->blocked) {
    m->blocked = false;
    if (!rnd(3) && lastmillis - m->lastmove > 1500) {
      m->jumpnext = true;
      m->lastmove = lastmillis;
    } else {
      m->targetyaw += 90 + rnd(180);
    }
  }

  m->move = 1;
  m->strafe = 0;

  if (disttoenemy < 4) {
    m->move = -1;
  } else if (disttoenemy < 12) {
    m->strafe = (rnd(3) - 1);
  }

  if (disttoenemy < 128 && m->enemy->state == CS_ALIVE) {
    int attacktime = lastmillis - m->lastaction;
    if (attacktime > 200 + rnd(500)) {
      m->attacktarget = m->enemy->o;
      m->attacking = true;
      shoot(m, m->attacktarget);
      m->lastaction = lastmillis;
    }
  }

  moveplayer(m, 1, false);
}

void botthink() {
  loopv(bots) {
    dynent *b = bots[i];
    if (b->state == CS_ALIVE) {
      botaction(b);
    } else if (b->state == CS_DEAD && lastmillis - b->lastaction > 5000) {
      spawnplayer(b);
      b->health = 100;
      b->armour = 0;
      b->state = CS_ALIVE;
      b->monsterstate = M_HOME;
      b->enemy = player1;
      b->move = 1;
      b->attacking = false;
    }
  }
}

void botrender() {
  loopv(bots) {
    if(bots[i]->state == CS_DEAD) continue;
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
    if (d == player1) {
      player1->frags++;
      addmsg(1, 2, SV_FRAGS, player1->frags);
    }
    conoutf("%s fragged %s", d->name, m->name);
  } else {
    playsound(S_PAIN1 + rnd(5), &m->o);
  }
}

extern bool isdedicated;

void addbotcmd() {
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

  dynent *b = newdynent();
  spawnplayer(b);
  b->monsterstate = M_HOME;
  b->mtype = -1;
  b->enemy = player1;
  b->move = 1;
  b->targetyaw = b->yaw;
  b->gunselect = GUN_SG + rnd(4);
  b->maxspeed = 36.0f;
  b->health = 100;
  b->armour = 0;
  loopi(NUMGUNS) b->ammo[i] = 100;
  b->anger = 0;
  b->lastupdate = lastmillis;
  b->state = CS_ALIVE;
  b->pitch = 0;
  b->roll = 0;
  sprintf_s(b->name)("bot %d", ++numbots);
  bots.add(b);
  conoutf("%s spawned.", b->name);
}

COMMANDN(addbot, addbotcmd, ARG_NONE);
