#include "../include/cube.h"

extern int mode;

struct serverbot {
  int id;
  char name[16];
  char team[5];
  int health, gunselect, ammo[11];
  float x, y, z;
  float yaw, pitch, roll;
  int state, lastaction, frags, deaths;
  int enemy, lastmove, lastattack;
  float targetyaw;
  enet_uint32 lastbroadcast;
};

static serverbot sbot[MAXBOTS];
static int numsbots = 0;
static int nextid = 0;
static enet_uint32 lastbotthink = 0;

static const char *sbotnames[] = {
    "Cerelo", "Diaso", "Ceria", "Deathly", "Ra",  "Va",  "Never", "Abu",
    "Re",     "Why",   "Lucky", "Lano",    "Cliff", "Cobra", "Liner", "Chiba",
    "Dragon", "Sabre", "Xor",   "Snuff",   "Tux",  "Moon", "Risco", "Disco"};

static const int nsbotnames = sizeof(sbotnames) / sizeof(sbotnames[0]);

int serverbot_count() { return numsbots; }

int serverbot_getid(int i) {
  if (i < 0 || i >= numsbots) return -1;
  return sbot[i].id;
}

const char *serverbot_name(int i) {
  if (i < 0 || i >= numsbots) return NULL;
  return sbot[i].name;
}

void serverbot_getpos(int i, float &x, float &y, float &z, float &yaw,
                       float &pitch, float &roll) {
  if (i < 0 || i >= numsbots) return;
  x = sbot[i].x;
  y = sbot[i].y;
  z = sbot[i].z;
  yaw = sbot[i].yaw;
  pitch = sbot[i].pitch;
  roll = sbot[i].roll;
}

int serverbot_getstate(int i) {
  if (i < 0 || i >= numsbots) return CS_DEAD;
  return sbot[i].state;
}

int serverbot_gethealth(int i) {
  if (i < 0 || i >= numsbots) return 0;
  return sbot[i].health;
}

int serverbot_getgun(int i) {
  if (i < 0 || i >= numsbots) return 0;
  return sbot[i].gunselect;
}

void serverbot_damage(int id, int damage) {
  int i = -1;
  loopj(numsbots) if (sbot[j].id == id) { i = j; break; }
  if (i < 0) return;
  sbot[i].health -= damage;
  if (sbot[i].health <= 0) {
    sbot[i].state = CS_DEAD;
    sbot[i].lastaction = enet_time_get();
  }
}

void serverbot_spawn(int count) {
  if (count < 1) count = 1;
  if (numsbots + count > MAXBOTS) count = MAXBOTS - numsbots;
  loopj(count) {
    if (numsbots >= MAXBOTS) break;
    int i = numsbots;
    numsbots++;
    serverbot &b = sbot[i];
    b.id = nextid++;
    strcpy_s(b.name, sbotnames[rnd(nsbotnames)]);
    b.x = (float)(rnd(401) - 200);
    b.y = (float)(rnd(401) - 200);
    b.z = 4;
    b.yaw = (float)rnd(360);
    b.pitch = 0;
    b.roll = 0;
    b.health = 100;
    b.gunselect = 1 + rnd(6);
    loopk(11) b.ammo[k] = 100;
    b.state = CS_ALIVE;
    b.lastaction = 0;
    b.frags = 0;
    b.deaths = 0;
    b.enemy = -1;
    b.lastmove = 0;
    b.lastattack = 0;
    b.targetyaw = b.yaw;
    b.lastbroadcast = 0;
    strcpy_s(b.team,
             ((mode & 1 && mode > 2) || mode == 12) ? (rnd(2) ? "RED" : "BLUE") : "");
    printf("server bot spawned: %s\n", b.name);
  }
}

void serverbot_clear() { numsbots = 0; }

bool serverbot_kick(const char *name) {
  loopi(numsbots) {
    if (!strcmp(sbot[i].name, name)) {
      memmove(&sbot[i], &sbot[i + 1], (numsbots - i - 1) * sizeof(serverbot));
      numsbots--;
      return true;
    }
  }
  return false;
}

void serverbot_update() {
  enet_uint32 now = enet_time_get();
  if (now - lastbotthink < 50) return;
  int diff = (int)(now - lastbotthink);
  if (diff > 200) diff = 200;
  lastbotthink = now;

  loopi(numsbots) {
    serverbot &b = sbot[i];
    if (b.state == CS_DEAD) {
      if (now - b.lastaction > 5000) {
        b.x = (float)(rnd(401) - 200);
        b.y = (float)(rnd(401) - 200);
        b.z = 4;
        b.health = 100;
        b.state = CS_ALIVE;
        b.enemy = -1;
        b.lastmove = 0;
      }
      continue;
    }

    if (now - b.lastmove > 3000 + rnd(4500)) {
      b.targetyaw = (float)rnd(360);
      b.lastmove = now;
    }

    float turnrate = diff * 0.3f;
    float yawdiff = b.targetyaw - b.yaw;
    if (fabs(yawdiff) < turnrate)
      b.yaw = b.targetyaw;
    else if (yawdiff > 0)
      b.yaw += turnrate;
    else
      b.yaw -= turnrate;

    float rad = b.yaw / 180.0f * PI;
    b.x += sinf(rad) * diff * 0.05f;
    b.y += cosf(rad) * diff * 0.05f;
  }
}
