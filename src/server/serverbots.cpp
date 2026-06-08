#include "../include/cube.h"
#include <zlib.h>

extern int mode;
extern vector<client> clients;
extern ENetHost *serverhost;
extern string smapname;
extern void endianswap(void *, int, int);

struct serverbot {
  int cn;
  char name[16];
  float x, y, z, yaw, pitch, roll;
  int state, health, gunselect, frags, deaths;
  int lastaction, lastmove, lastattack;
  float targetyaw;
  int ammo[NUMGUNS];
  int movemode, strafemode;
  float fallvelocity;
  bool onfloor;
  enet_uint32 blocktime;
  enet_uint32 lastjump;
};

struct walkinfo {
  char floor;
  char ceil;
  bool walkable;
};

struct clstate {
  float x, y, z;
  float yaw, pitch;
  int state;
  int lifesequence;
  bool active;
};

static serverbot sbot[MAXBOTS];
static int numsbots = 0;
static int nextcn = BOT_CLIENT_BASE;
static enet_uint32 lastthink = 0;
static float spawnx[MAXBOTS], spawny[MAXBOTS], spawnz[MAXBOTS];
static int numspawns = 0;
static walkinfo *walkdata = NULL;
static int mapsize = 512;
static clstate playerpos[MAXCLIENTS];

static const float BOT_EYEHEIGHT = 3.2f;
static const float BOT_ABOVEEYE = 0.7f;
static const float BOT_RADIUS = 1.1f;
static const float BOT_HIT_RADIUS = 1.5f;
static const float BOT_MAXSTEP = 1.0f;
static const float BOT_ATTACK_RANGE = 80.0f;
static const float BOT_ATTACK_RANGE_SQ = 6400.0f;

static const int BOT_WEAPON_DELAYS[NUMGUNS] = {150, 1000, 100, 800, 1500, 100,
                                               50,  200,  200, 200, 250};
static const int BOT_WEAPON_DAMAGES[NUMGUNS] = {20, 10, 30, 120, 100, 25,
                                                15, 20, 40, 30,  50};

static bool server_los(float lx, float ly, float lz, float bx, float by,
                       float bz) {
  if (!walkdata || mapsize <= 0)
    return true;
  float dx = bx - lx;
  float dy = by - ly;
  float dz = bz - lz;
  float hdist = sqrtf(dx * dx + dy * dy);
  if (hdist < 0.01f)
    return true;
  int steps = (int)(hdist / 0.5f);
  if (steps < 1)
    steps = 1;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float x = lx + dx * t;
    float y = ly + dy * t;
    float z = lz + dz * t;
    int cx = (int)x, cy = (int)y;
    if (cx < 0 || cy < 0 || cx >= mapsize || cy >= mapsize)
      return false;
    walkinfo &wi = walkdata[cy * mapsize + cx];
    if (!wi.walkable)
      return false;
    if (z < (float)wi.floor - 0.5f || z > (float)wi.ceil + 0.5f)
      return false;
  }
  return true;
}

static const char *bnames[] = {
    "Cerelo", "Diaso", "Ceria",  "Deathly", "Ra",      "Va",     "Never",
    "Abu",    "Re",    "Why",    "Lucky",   "Lano",    "Cliff",  "Cobra",
    "Liner",  "Chiba", "Dragon", "Sabre",   "Koffman", "Stuff",  "Bones",
    "Xor",    "Snuff", "Sniff",  "Pain",    "Time",    "Fake",   "Headup",
    "MX",     "Moon",  "Wine",   "Tux",     "Crash",   "Threed", "Backlotter",
    "Risco",  "Disco", "Cheque", "Will",    "Who",     "Cares",  "Anyway"};

static void loadspawns() {
  numspawns = 0;
  mapsize = 512;
  free(walkdata);
  walkdata = NULL;
  if (!smapname[0])
    return;
  char cgzname[256], pakname[64], mapname[64];
  char *slash = strpbrk(smapname, "/\\");
  if (slash) {
    strn0cpy(pakname, smapname, slash - smapname + 1);
    strcpy_s(mapname, slash + 1);
  } else {
    strcpy_s(pakname, "base");
    strcpy_s(mapname, smapname);
  };
  sprintf_s(cgzname)("packages/%s/%s.cgz", pakname, mapname);
  gzFile f = gzopen(cgzname, "rb");
  if (!f)
    return;
  header hdr;
  gzread(f, &hdr, sizeof(header) - sizeof(int) * 16);
  endianswap(&hdr.version, sizeof(int), 4);
  if (strncmp(hdr.head, "CUBE", 4) != 0 || hdr.version > MAPVERSION) {
    gzclose(f);
    return;
  };
  mapsize = 1 << hdr.sfactor;
  if (hdr.version >= 4) {
    gzread(f, &hdr.waterlevel, sizeof(int) * 16);
    endianswap(&hdr.waterlevel, sizeof(int), 16);
  };
  loopi(hdr.numents) {
    persistent_entity e;
    gzread(f, &e, sizeof(persistent_entity));
    endianswap(&e, sizeof(short), 4);
    if (e.type == PLAYERSTART && numspawns < MAXBOTS) {
      spawnx[numspawns] = (float)e.x;
      spawny[numspawns] = (float)e.y;
      spawnz[numspawns] = (float)e.z;
      numspawns++;
    };
  };
  int total = mapsize * mapsize;
  walkdata = (walkinfo *)calloc(total, sizeof(walkinfo));
  if (!walkdata) {
    gzclose(f);
    return;
  }
  walkinfo prevdata;
  bool hasprev = false;
  int k = 0;
  while (k < total) {
    int type = gzgetc(f);
    if (type < 0)
      break;
    if (type == 255) {
      int n = gzgetc(f);
      if (n < 0)
        break;
      for (int r = 0; r < n && k < total; r++, k++) {
        if (hasprev)
          walkdata[k] = prevdata;
      }
      continue;
    }
    walkinfo &wi = walkdata[k];
    if (type == 0) {
      wi.walkable = false;
      wi.floor = 0;
      wi.ceil = 16;
      gzgetc(f);
      gzgetc(f);
      if (hdr.version <= 2) {
        gzgetc(f);
        gzgetc(f);
      }
    } else {
      wi.floor = (char)gzgetc(f);
      wi.ceil = (char)gzgetc(f);
      wi.walkable = (type != 1);
      gzgetc(f);
      gzgetc(f);
      gzgetc(f);
      if (hdr.version <= 2) {
        gzgetc(f);
        gzgetc(f);
      }
      gzgetc(f);
      if (hdr.version >= 2)
        gzgetc(f);
      if (hdr.version >= 5)
        gzgetc(f);
    }
    prevdata = wi;
    hasprev = true;
    k++;
  }
  gzclose(f);
}

int serverbot_count() { return numsbots; }

void serverbot_spawn(int count) {
  if (count < 1)
    count = 1;
  loadspawns();
  if (numspawns == 0) {
    spawnx[0] = spawny[0] = 64;
    spawnz[0] = 0;
    numspawns = 1;
  };
  int n = 0;
  while (n < count && numsbots < MAXBOTS) {
    int i = numsbots;
    serverbot &b = sbot[i];
    b.cn = nextcn++;
    strcpy_s(b.name, bnames[rnd(24)]);
    int si = rnd(numspawns);
    b.x = spawnx[si];
    b.y = spawny[si];
    b.z = spawnz[si] + BOT_EYEHEIGHT;
    b.yaw = (float)rnd(360);
    b.pitch = 0;
    b.roll = 0;
    b.state = CS_ALIVE;
    b.health = 100;
    b.gunselect = 1 + rnd(6);
    loopk(NUMGUNS) b.ammo[k] = 100;
    b.ammo[GUN_CSAW] = 1;
    b.movemode = 1;
    b.strafemode = 0;
    b.fallvelocity = 0;
    b.onfloor = true;
    b.blocktime = 0;
    b.frags = 0;
    b.deaths = 0;
    b.lastaction = 0;
    b.lastmove = 0;
    b.lastattack = 0;
    b.targetyaw = b.yaw;
    b.lastjump = 0;
    numsbots++;
    n++;
  }
}

void serverbot_clear() {
  numsbots = 0;
  numspawns = 0;
  free(walkdata);
  walkdata = NULL;
}

bool serverbot_kick(const char *name) {
  loopi(numsbots) if (!strcmp(sbot[i].name, name)) {
    memmove(&sbot[i], &sbot[i + 1], (numsbots - i - 1) * sizeof(serverbot));
    numsbots--;
    return true;
  }
  return false;
}

int serverbot_getid(int i) {
  if (i < 0 || i >= numsbots)
    return -1;
  return sbot[i].cn;
}

const char *serverbot_name(int i) {
  if (i < 0 || i >= numsbots)
    return NULL;
  return sbot[i].name;
}

void serverbot_fragged(int cn) {
  if (cn < BOT_CLIENT_BASE)
    return;
  loopi(numsbots) if (sbot[i].cn == cn) {
    sbot[i].frags++;
    return;
  }
}

void serverbot_damage(int cn, int damage, int attacker) {
  loopi(numsbots) if (sbot[i].cn == cn) {
    sbot[i].health -= damage;
    bool died = sbot[i].health <= 0;
    if (died) {
      sbot[i].state = CS_DEAD;
      sbot[i].lastaction = enet_time_get();
      sbot[i].deaths++;
      if (attacker >= BOT_CLIENT_BASE) {
        loopk(numsbots) if (sbot[k].cn == attacker) {
          sbot[k].frags++;
          break;
        }
      }
    }
    ENetPacket *packet = enet_packet_create(NULL, 80, 0);
    uchar *start = packet->data;
    uchar *p = start + 2;
    putint(p, SV_POS);
    putint(p, cn);
    putint(p, (int)(sbot[i].x * DMF));
    putint(p, (int)(sbot[i].y * DMF));
    putint(p, (int)(sbot[i].z * DMF));
    putint(p, (int)(sbot[i].yaw * DAF));
    putint(p, (int)(sbot[i].pitch * DAF));
    putint(p, (int)(sbot[i].roll * DAF));
    putint(p, 0);
    putint(p, 0);
    putint(p, 0);
    putint(p, (sbot[i].state << 5));
    if (died) {
      putint(p, SV_DIED);
      putint(p, attacker);
    } else {
      putint(p, SV_DAMAGE);
      putint(p, cn);
      putint(p, damage);
      putint(p, 0);
    }
    *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
    enet_packet_resize(packet, p - start);
    loopv(clients) {
      if (clients[i].type == ST_TCPIP)
        enet_peer_send(clients[i].peer, 0, packet);
    }
    return;
  }
}

void serverbot_broadcast() {
  if (!serverhost)
    return;
  static enet_uint32 lastbc = 0;
  if (enet_time_get() - lastbc < 40)
    return;
  lastbc = enet_time_get();
  loopi(numsbots) {
    serverbot &b = sbot[i];
    ENetPacket *packet = enet_packet_create(NULL, 40, 0);
    uchar *start = packet->data;
    uchar *p = start + 2;
    putint(p, SV_POS);
    putint(p, b.cn);
    putint(p, (int)(b.x * DMF));
    putint(p, (int)(b.y * DMF));
    putint(p, (int)(b.z * DMF));
    putint(p, (int)(b.yaw * DAF));
    putint(p, (int)(b.pitch * DAF));
    putint(p, (int)(b.roll * DAF));
    if (b.state == CS_ALIVE) {
      putint(p, 0);
      putint(p, 0);
      putint(p, 0);
      putint(p, (b.strafemode & 3) | ((b.movemode & 3) << 2) |
                    ((b.onfloor ? 1 : 0) << 4) | (b.state << 5));
    } else {
      putint(p, 0);
      putint(p, 0);
      putint(p, 0);
      putint(p, (b.state << 5));
    }
    putint(p, SV_FRAGS);
    putint(p, b.frags);
    *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
    enet_packet_resize(packet, p - start);
    loopv(clients) {
      if (clients[i].type == ST_TCPIP)
        enet_peer_send(clients[i].peer, 0, packet);
    }
  }
}

void serverbot_sendinit(int cn) {
  if (cn < 0 || cn >= clients.length() || clients[cn].type != ST_TCPIP)
    return;
  loopi(numsbots) {
    serverbot &b = sbot[i];
    ENetPacket *pkt = enet_packet_create(NULL, 96, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = pkt->data;
    uchar *p = start + 2;
    putint(p, SV_POS);
    putint(p, b.cn);
    putint(p, (int)(b.x * DMF));
    putint(p, (int)(b.y * DMF));
    putint(p, (int)(b.z * DMF));
    putint(p, (int)(b.yaw * DAF));
    putint(p, (int)(b.pitch * DAF));
    putint(p, (int)(b.roll * DAF));
    if (b.state == CS_ALIVE) {
      putint(p, 0);
      putint(p, 0);
      putint(p, 0);
      putint(p, (b.strafemode & 3) | ((b.movemode & 3) << 2) |
                    ((b.onfloor ? 1 : 0) << 4) | (b.state << 5));
    } else {
      putint(p, 0);
      putint(p, 0);
      putint(p, 0);
      putint(p, (b.state << 5));
    }
    putint(p, SV_INITC2S);
    sendstring(b.name, p);
    sendstring("", p);
    putint(p, 0);
    *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
    enet_packet_resize(pkt, p - start);
    enet_peer_send(clients[cn].peer, 0, pkt);
  }
}

void serverbot_trackplayer(int cn, float x, float y, float z, float yaw,
                           float pitch, int state) {
  if (cn < 0 || cn >= MAXCLIENTS)
    return;
  playerpos[cn].x = x;
  playerpos[cn].y = y;
  playerpos[cn].z = z;
  playerpos[cn].yaw = yaw;
  playerpos[cn].pitch = pitch;
  playerpos[cn].state = state;
  playerpos[cn].active = true;
}

void serverbot_clearplayer(int cn) {
  if (cn < 0 || cn >= MAXCLIENTS)
    return;
  playerpos[cn].active = false;
}

void serverbot_player_died(int cn) {
  if (cn >= 0 && cn < MAXCLIENTS && playerpos[cn].active)
    playerpos[cn].lifesequence++;
}

static bool intersect_bot(serverbot &b, vec &from, vec &to) {
  float vx = to.x - from.x;
  float vy = to.y - from.y;
  float vz = to.z - from.z;
  float wx = b.x - from.x;
  float wy = b.y - from.y;
  float wz = b.z - from.z;
  float c1 = wx * vx + wy * vy + wz * vz;
  float c2 = vx * vx + vy * vy + vz * vz;
  float px, py, pz;
  if (c1 <= 0) {
    px = from.x;
    py = from.y;
    pz = from.z;
  } else if (c2 <= c1) {
    px = to.x;
    py = to.y;
    pz = to.z;
  } else {
    float f = c1 / c2;
    px = from.x + vx * f;
    py = from.y + vy * f;
    pz = from.z + vz * f;
  }
  return px >= b.x - BOT_HIT_RADIUS && px <= b.x + BOT_HIT_RADIUS &&
         py >= b.y - BOT_HIT_RADIUS && py <= b.y + BOT_HIT_RADIUS &&
         pz >= b.z - BOT_EYEHEIGHT && pz <= b.z + BOT_ABOVEEYE;
}

void serverbot_hitscan(int gun, vec &from, vec &to, int sender) {
  if (sender < 0 || sender >= MAXCLIENTS)
    return;
  if (gun == GUN_SG) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float dz = to.z - from.z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dist < 1.0f)
      dist = 1.0f;
    float udx = dx / dist, udy = dy / dist, udz = dz / dist;
    float rightx, righty, rightz;
    if (fabs(udx) < 0.9f) {
      rightx = -udy;
      righty = udx;
      rightz = 0;
    } else {
      rightx = 0;
      righty = -udz;
      rightz = udy;
    }
    float rlen = sqrtf(rightx * rightx + righty * righty + rightz * rightz);
    rightx /= rlen;
    righty /= rlen;
    rightz /= rlen;
    float upx = udy * rightz - udz * righty;
    float upy = udz * rightx - udx * rightz;
    float upz = udx * righty - udy * rightx;
    const int SGRAYS = 20;
    const float SGSPREAD = 2.0f * PI / 180.0f;
    loopi(numsbots) {
      serverbot &b = sbot[i];
      if (b.state != CS_ALIVE)
        continue;
      int totaldamage = 0;
      loopk(SGRAYS) {
        float spreadx = (rnd(101) - 50) / 50.0f * SGSPREAD;
        float spready = (rnd(101) - 50) / 50.0f * SGSPREAD;
        float rx = udx + rightx * spreadx + upx * spready;
        float ry = udy + righty * spreadx + upy * spready;
        float rz = udz + rightz * spreadx + upz * spready;
        float rdist = sqrtf(rx * rx + ry * ry + rz * rz);
        rx /= rdist;
        ry /= rdist;
        rz /= rdist;
        vec rto;
        rto.x = from.x + rx * dist;
        rto.y = from.y + ry * dist;
        rto.z = from.z + rz * dist;
        if (intersect_bot(b, from, rto))
          totaldamage += 10;
      }
      if (totaldamage > 0) {
        serverbot_damage(b.cn, totaldamage, sender);
        break;
      }
    }
    return;
  }
  loopi(numsbots) {
    serverbot &b = sbot[i];
    if (b.state != CS_ALIVE)
      continue;
    if (intersect_bot(b, from, to)) {
      int damage = 10;
      switch (gun) {
      case GUN_CSAW:
        damage = 20;
        break;
      case GUN_CG:
        damage = 30;
        break;
      case GUN_RL:
        damage = 120;
        break;
      case GUN_RIFLE:
        damage = 100;
        break;
      case GUN_NAILGUN:
        damage = 25;
        break;
      case GUN_LIGHTGUN:
        damage = 15;
        break;
      }
      serverbot_damage(b.cn, damage, sender);
      break;
    }
  }
}

static void send_bot_shot(serverbot &b, float tx, float ty, float tz) {
  ENetPacket *packet = enet_packet_create(NULL, 80, 0);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_POS);
  putint(p, b.cn);
  putint(p, (int)(b.x * DMF));
  putint(p, (int)(b.y * DMF));
  putint(p, (int)(b.z * DMF));
  putint(p, (int)(b.yaw * DAF));
  putint(p, (int)(b.pitch * DAF));
  putint(p, (int)(b.roll * DAF));
  putint(p, 0);
  putint(p, 0);
  putint(p, 0);
  putint(p, (b.state << 5));
  putint(p, SV_SHOT);
  putint(p, b.gunselect);
  putint(p, (int)(b.x * DMF));
  putint(p, (int)(b.y * DMF));
  putint(p, (int)(b.z * DMF));
  putint(p, (int)(tx * DMF));
  putint(p, (int)(ty * DMF));
  putint(p, (int)(tz * DMF));
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  loopv(clients) {
    if (clients[i].type == ST_TCPIP)
      enet_peer_send(clients[i].peer, 0, packet);
  }
}

static bool shot_hits_player(float fromx, float fromy, float fromz, float tox,
                             float toy, float toz, float px, float py,
                             float pz) {
  float vx = tox - fromx;
  float vy = toy - fromy;
  float vz = toz - fromz;
  float wx = px - fromx;
  float wy = py - fromy;
  float wz = pz - fromz;
  float c1 = wx * vx + wy * vy + wz * vz;
  float c2 = vx * vx + vy * vy + vz * vz;
  float hx, hy, hz;
  if (c1 <= 0) {
    hx = fromx;
    hy = fromy;
    hz = fromz;
  } else if (c2 <= c1) {
    hx = tox;
    hy = toy;
    hz = toz;
  } else {
    float f = c1 / c2;
    hx = fromx + vx * f;
    hy = fromy + vy * f;
    hz = fromz + vz * f;
  }
  const float pr = 1.1f;
  const float peh = 3.2f;
  const float pae = 0.7f;
  return hx >= px - pr && hx <= px + pr && hy >= py - pr && hy <= py + pr &&
         hz >= pz - peh && hz <= pz + pae;
}

static void send_bot_damage_player(serverbot &b, int target, int damage,
                                   int lifeseq) {
  ENetPacket *packet = enet_packet_create(NULL, 80, 0);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_POS);
  putint(p, b.cn);
  putint(p, (int)(b.x * DMF));
  putint(p, (int)(b.y * DMF));
  putint(p, (int)(b.z * DMF));
  putint(p, (int)(b.yaw * DAF));
  putint(p, (int)(b.pitch * DAF));
  putint(p, (int)(b.roll * DAF));
  putint(p, 0);
  putint(p, 0);
  putint(p, 0);
  putint(p, (b.state << 5));
  putint(p, SV_DAMAGE);
  putint(p, target);
  putint(p, damage);
  putint(p, lifeseq);
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  loopv(clients) {
    if (clients[i].type == ST_TCPIP)
      enet_peer_send(clients[i].peer, 0, packet);
  }
}

static bool shot_hits_bot(float fromx, float fromy, float fromz, float tox,
                          float toy, float toz, serverbot &target) {
  vec f, t;
  f.x = fromx;
  f.y = fromy;
  f.z = fromz;
  t.x = tox;
  t.y = toy;
  t.z = toz;
  return intersect_bot(target, f, t);
}

static bool try_move_bot(serverbot &b, int diff, int move, int strafe) {
  if (!walkdata || mapsize <= 0)
    return false;
  float dt = diff / 1000.0f;
  if (move == 0 && strafe == 0) {
    int cx = (int)b.x, cy = (int)b.y;
    if (cx >= 0 && cy >= 0 && cx < mapsize && cy < mapsize) {
      float floor = (float)walkdata[cy * mapsize + cx].floor;
      float targetZ = floor + BOT_EYEHEIGHT;
      if (targetZ < b.z - 0.01f) {
        b.fallvelocity += 600.0f * dt;
        b.z -= b.fallvelocity * dt;
        if (b.z < targetZ) {
          b.z = targetZ;
          b.fallvelocity = 0;
        }
      } else if (targetZ > b.z + 0.01f) {
        b.z = targetZ;
        b.fallvelocity = 0;
      }
      b.onfloor = (b.z <= targetZ + 0.1f);
    }
    return true;
  }
  float step = diff * 0.015f;
  if (step > 0.9f)
    step = 0.9f;
  float rad = b.yaw / 180.0f * PI;
  float nx = b.x + sinf(rad) * step * (float)move;
  float ny = b.y + cosf(rad) * step * (float)move;
  if (strafe != 0) {
    float st = diff * 0.01f;
    if (st > 0.6f)
      st = 0.6f;
    float srad = (b.yaw + 90.0f * (float)strafe) / 180.0f * PI;
    nx += sinf(srad) * st;
    ny += cosf(srad) * st;
  }
  int x1 = (int)(nx - BOT_RADIUS);
  int y1 = (int)(ny - BOT_RADIUS);
  int x2 = (int)(nx + BOT_RADIUS);
  int y2 = (int)(ny + BOT_RADIUS);
  bool blocked = false;
  char bestfloor = -128;
  for (int cx = x1; cx <= x2 && !blocked; cx++) {
    for (int cy = y1; cy <= y2; cy++) {
      if (cx < 0 || cy < 0 || cx >= mapsize || cy >= mapsize) {
        blocked = true;
        break;
      }
      walkinfo &wi = walkdata[cy * mapsize + cx];
      if (!wi.walkable) {
        blocked = true;
        break;
      }
      if (wi.floor > bestfloor)
        bestfloor = wi.floor;
    }
  }
  if (blocked)
    return false;
  float curfloor = b.z - BOT_EYEHEIGHT;
  float targetfloor = (float)bestfloor;
  if (targetfloor > curfloor + BOT_MAXSTEP)
    return false;
  b.x = nx;
  b.y = ny;
  float targetZ = targetfloor + BOT_EYEHEIGHT;
  if (targetZ < b.z - 0.01f) {
    b.fallvelocity += 600.0f * dt;
    b.z -= b.fallvelocity * dt;
    if (b.z < targetZ) {
      b.z = targetZ;
      b.fallvelocity = 0;
    }
  } else if (targetZ > b.z + 0.01f) {
    b.z = targetZ;
    b.fallvelocity = 0;
  }
  b.onfloor = (b.z <= targetZ + 0.1f);
  return true;
}

void serverbot_update() {
  enet_uint32 now = enet_time_get();
  if (now - lastthink < 50)
    return;
  int diff = (int)(now - lastthink);
  if (diff > 200)
    diff = 200;
  lastthink = now;

  static enet_uint32 lastammorefill = 0;
  if (now - lastammorefill > 15000) {
    lastammorefill = now;
    loopi(numsbots) {
      if (sbot[i].state == CS_ALIVE) {
        if (sbot[i].ammo[GUN_SG] < 10)
          sbot[i].ammo[GUN_SG] = 10;
        if (sbot[i].ammo[GUN_NAILGUN] < 40)
          sbot[i].ammo[GUN_NAILGUN] = 40;
        if (sbot[i].ammo[GUN_CG] < 40)
          sbot[i].ammo[GUN_CG] = 40;
        if (sbot[i].ammo[GUN_RL] < 8)
          sbot[i].ammo[GUN_RL] = 8;
        if (sbot[i].ammo[GUN_RIFLE] < 8)
          sbot[i].ammo[GUN_RIFLE] = 8;
        sbot[i].ammo[GUN_CSAW] = 1;
      }
    }
  }

  loopi(numsbots) {
    serverbot &b = sbot[i];
    b.onfloor = false;
    b.movemode = 0;
    b.strafemode = 0;
    if (b.state == CS_DEAD) {
      if (now - b.lastaction > 5000) {
        loadspawns();
        if (numspawns == 0) {
          spawnx[0] = spawny[0] = 64;
          spawnz[0] = 0;
          numspawns = 1;
        };
        int si = rnd(numspawns);
        b.x = spawnx[si];
        b.y = spawny[si];
        b.z = spawnz[si] + BOT_EYEHEIGHT;
        b.health = 100;
        b.state = CS_ALIVE;
        b.lastmove = 0;
        b.lastattack = 0;
        b.gunselect = 1 + rnd(6);
        loopk(NUMGUNS) b.ammo[k] = 100;
        b.ammo[GUN_CSAW] = 1;
        b.movemode = 1;
        b.strafemode = 0;
        b.fallvelocity = 0;
        b.onfloor = true;
        b.blocktime = 0;
        b.lastjump = 0;
      }
      continue;
    }

    bool target_is_player = false;
    int targetidx = -1;
    float bestdist = BOT_ATTACK_RANGE_SQ;

    loopj(MAXCLIENTS) {
      if (!playerpos[j].active)
        continue;
      if (playerpos[j].state != CS_ALIVE)
        continue;
      float dx = playerpos[j].x - b.x;
      float dy = playerpos[j].y - b.y;
      float dz = playerpos[j].z - b.z;
      float dist = dx * dx + dy * dy + dz * dz;
      if (dist < bestdist) {
        bestdist = dist;
        targetidx = j;
        target_is_player = true;
      }
    }

    loopj(numsbots) {
      if (j == i)
        continue;
      serverbot &other = sbot[j];
      if (other.state != CS_ALIVE)
        continue;
      float dx = other.x - b.x;
      float dy = other.y - b.y;
      float dz = other.z - b.z;
      float dist = dx * dx + dy * dy + dz * dz;
      if (dist < bestdist) {
        bestdist = dist;
        targetidx = j;
        target_is_player = false;
      }
    }

    if (targetidx >= 0 && bestdist < BOT_ATTACK_RANGE_SQ) {
      float tx, ty, tz;
      int lifeseq = 0;
      if (target_is_player) {
        tx = playerpos[targetidx].x;
        ty = playerpos[targetidx].y;
        tz = playerpos[targetidx].z;
        lifeseq = playerpos[targetidx].lifesequence;
      } else {
        tx = sbot[targetidx].x;
        ty = sbot[targetidx].y;
        tz = sbot[targetidx].z;
      }
      float realdist = sqrtf(bestdist);

      if (bestdist < 64.0f && b.gunselect != GUN_CSAW) {
        b.gunselect = GUN_CSAW;
      } else if (b.gunselect == GUN_CSAW && realdist > 15.0f) {
        if (b.ammo[GUN_RL])
          b.gunselect = GUN_RL;
        else if (b.ammo[GUN_CG])
          b.gunselect = GUN_CG;
        else if (b.ammo[GUN_LIGHTGUN])
          b.gunselect = GUN_LIGHTGUN;
        else if (b.ammo[GUN_NAILGUN])
          b.gunselect = GUN_NAILGUN;
        else if (b.ammo[GUN_SG])
          b.gunselect = GUN_SG;
        else if (b.ammo[GUN_RIFLE])
          b.gunselect = GUN_RIFLE;
      } else if (!b.ammo[b.gunselect] && b.gunselect != GUN_CSAW) {
        if (b.ammo[GUN_RL])
          b.gunselect = GUN_RL;
        else if (b.ammo[GUN_CG])
          b.gunselect = GUN_CG;
        else if (b.ammo[GUN_LIGHTGUN])
          b.gunselect = GUN_LIGHTGUN;
        else if (b.ammo[GUN_NAILGUN])
          b.gunselect = GUN_NAILGUN;
        else if (b.ammo[GUN_SG])
          b.gunselect = GUN_SG;
        else if (b.ammo[GUN_RIFLE])
          b.gunselect = GUN_RIFLE;
        else
          b.gunselect = GUN_CSAW;
      }

      float enemyyaw = -(float)atan2(tx - b.x, ty - b.y) / PI * 180 + 180;
      float enemypitch = atan2(tz - b.z, realdist) * 180 / PI;
      {
        float turnrate = diff * 0.3f;
        float yd = enemyyaw - b.yaw;
        if (fabs(yd) > 135.0f)
          turnrate *= 3.0f;
        else if (fabs(yd) > 90.0f)
          turnrate *= 2.0f;
        if (fabs(yd) < turnrate)
          b.yaw = enemyyaw;
        else if (yd > 0)
          b.yaw += turnrate;
        else
          b.yaw -= turnrate;
        b.targetyaw = enemyyaw;
      }
      b.pitch = enemypitch;

      float yaw_to_target = enemyyaw - b.yaw;
      if (yaw_to_target > 180.0f)
        yaw_to_target -= 360.0f;
      if (yaw_to_target < -180.0f)
        yaw_to_target += 360.0f;

      int move = 1, strafe = 0;
      float abs_yaw = fabs(yaw_to_target);
      if (abs_yaw > 90.0f) {
        move = 0;
      }
      if (realdist < 8 && b.gunselect != GUN_CSAW) {
        if (now - b.lastmove > 300) {
          strafe = rnd(2) ? 1 : -1;
          b.lastmove = now;
        }
      }
      b.movemode = strafe ? 0 : move;
      b.strafemode = strafe;
      if (!try_move_bot(b, diff, move, strafe)) {
        float base = b.targetyaw;
        float angles[] = {60, -60, 80, -80, 110, -110, 135, -135, 180};
        bool escaped = false;
        int escapedir = 0;
        float ox = b.x, oy = b.y, oz = b.z;
        float ov = b.fallvelocity;
        bool of = b.onfloor;
        if (b.onfloor && of && now - b.lastjump > 400) {
          b.lastjump = now;
          b.fallvelocity = -40.0f;
          b.onfloor = false;
          b.z += 0.01f;
          if (try_move_bot(b, diff, move, 0)) {
            escaped = true;
            escapedir = move;
          } else {
            b.x = ox;
            b.y = oy;
            b.z = oz;
            b.fallvelocity = ov;
            b.onfloor = of;
          }
        }
        for (int t = 0; t < 9 && !escaped; t++) {
          b.yaw = base + angles[t];
          b.x = ox;
          b.y = oy;
          b.z = oz;
          b.fallvelocity = ov;
          b.onfloor = of;
          if (try_move_bot(b, diff, 1, 0)) {
            b.targetyaw = b.yaw;
            escaped = true;
            escapedir = 1;
            break;
          }
        }
        if (!escaped) {
          b.yaw = base;
          b.x = ox;
          b.y = oy;
          b.z = oz;
          b.fallvelocity = ov;
          b.onfloor = of;
          if (try_move_bot(b, diff, -1, 0)) {
            escaped = true;
            escapedir = -1;
          }
          b.targetyaw = base;
        }
        if (!escaped) {
          b.x = ox;
          b.y = oy;
          b.z = oz;
          b.fallvelocity = ov;
          b.onfloor = of;
          b.yaw = base + 180;
          b.targetyaw = b.yaw;
        }
        b.movemode = escaped ? escapedir : 0;
        b.strafemode = 0;
        if (!b.onfloor)
          b.movemode = 0;
        continue;
      }
      if (!b.onfloor)
        b.movemode = 0;

      int weapondelay = BOT_WEAPON_DELAYS[b.gunselect];
      int reactdelay = 100 + rnd(150);
      if (weapondelay > reactdelay)
        reactdelay = weapondelay;
      if (fabs(yaw_to_target) < 60.0f &&
          now - b.lastattack > (enet_uint32)reactdelay) {

        if (b.gunselect == GUN_CSAW && realdist < 2.5f) {
          b.lastattack = now;
          b.lastaction = now;
          if (b.ammo[GUN_CSAW])
            b.ammo[GUN_CSAW]--;
          float ttx = b.x + sinf(b.yaw / 180.0f * PI) * 2.0f;
          float tty = b.y + cosf(b.yaw / 180.0f * PI) * 2.0f;
          send_bot_shot(b, ttx, tty, b.z - 0.2f);
          int dmg = BOT_WEAPON_DAMAGES[GUN_CSAW];
          if (target_is_player)
            send_bot_damage_player(b, targetidx, dmg, lifeseq);
          else
            serverbot_damage(sbot[targetidx].cn, dmg, b.cn);
        } else if (b.gunselect != GUN_CSAW &&
                   server_los(b.x, b.y, b.z - 0.2f, tx, ty, tz)) {
          b.lastattack = now;
          b.lastaction = now;
          if (b.ammo[b.gunselect])
            b.ammo[b.gunselect]--;
          float inaccuracy = realdist * 0.03f;
          if (b.gunselect == GUN_RIFLE)
            inaccuracy *= 0.3f;
          float shot_tx = tx + (rnd(101) - 50) / 50.0f * inaccuracy;
          float shot_ty = ty + (rnd(101) - 50) / 50.0f * inaccuracy;
          float shot_tz = tz + (rnd(101) - 50) / 50.0f * inaccuracy * 0.5f;
          float fromx = b.x, fromy = b.y, fromz = b.z - 0.2f;
          send_bot_shot(b, shot_tx, shot_ty, shot_tz);
          int dmg = BOT_WEAPON_DAMAGES[b.gunselect];
          if (target_is_player) {
            if (shot_hits_player(fromx, fromy, fromz, shot_tx, shot_ty, shot_tz,
                                 tx, ty, tz))
              send_bot_damage_player(b, targetidx, dmg, lifeseq);
          } else {
            if (shot_hits_bot(fromx, fromy, fromz, shot_tx, shot_ty, shot_tz,
                              sbot[targetidx]))
              serverbot_damage(sbot[targetidx].cn, dmg, b.cn);
          }
        }
      }
      continue;
    }

    if (now - b.lastmove > 300 + rnd(1000)) {
      b.targetyaw = (float)rnd(360);
      b.lastmove = now;
    }
    float turnrate = diff * 0.3f;
    float yd = b.targetyaw - b.yaw;
    if (fabs(yd) < turnrate)
      b.yaw = b.targetyaw;
    else if (yd > 0)
      b.yaw += turnrate;
    else
      b.yaw -= turnrate;
    b.movemode = 1;
    b.strafemode = 0;
    if (!try_move_bot(b, diff, 1, 0)) {
      float base = b.targetyaw;
      float angles[] = {60, -60, 80, -80, 110, -110, 135, -135, 180};
      bool escaped = false;
      int escapedir = 0;
      float ox = b.x, oy = b.y, oz = b.z;
      float ov = b.fallvelocity;
      bool of = b.onfloor;
      if (b.onfloor && now - b.lastjump > 400) {
        b.lastjump = now;
        b.fallvelocity = -40.0f;
        b.onfloor = false;
        b.z += 0.01f;
        if (try_move_bot(b, diff, 1, 0)) {
          escaped = true;
          escapedir = 1;
        } else {
          b.x = ox;
          b.y = oy;
          b.z = oz;
          b.fallvelocity = ov;
          b.onfloor = of;
        }
      }
      for (int t = 0; t < 9 && !escaped; t++) {
        b.yaw = base + angles[t];
        b.x = ox;
        b.y = oy;
        b.z = oz;
        b.fallvelocity = ov;
        b.onfloor = of;
        if (try_move_bot(b, diff, 1, 0)) {
          b.targetyaw = b.yaw;
          escaped = true;
          escapedir = 1;
          break;
        }
      }
      if (!escaped) {
        b.yaw = base;
        b.x = ox;
        b.y = oy;
        b.z = oz;
        b.fallvelocity = ov;
        b.onfloor = of;
        if (try_move_bot(b, diff, -1, 0)) {
          escaped = true;
          escapedir = -1;
        }
        b.targetyaw = base;
      }
      if (!escaped) {
        b.x = ox;
        b.y = oy;
        b.z = oz;
        b.fallvelocity = ov;
        b.onfloor = of;
        b.yaw = base + 180;
        b.targetyaw = b.yaw;
      }
      b.movemode = escaped ? escapedir : 0;
    }
    if (!b.onfloor)
      b.movemode = 0;
  }
}
