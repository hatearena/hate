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
};

static serverbot sbot[MAXBOTS];
static int numsbots = 0;
static int nextcn = BOT_CLIENT_BASE;
static enet_uint32 lastthink = 0;
static float spawnx[MAXBOTS], spawny[MAXBOTS], spawnz[MAXBOTS];
static int numspawns = 0;
static bool *walkable = NULL;
static int mapsize = 256;

static const char *bnames[] = {
    "Cerelo","Diaso","Ceria","Deathly","Ra","Va","Never","Abu",
    "Re","Why","Lucky","Lano","Cliff","Cobra","Liner","Chiba",
    "Dragon","Sabre","Xor","Snuff","Tux","Moon","Risco","Disco"};

static void loadspawns() {
  numspawns = 0;
  mapsize = 256;
  free(walkable);
  walkable = NULL;
  if (!smapname[0]) return;
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
  if (!f) { printf("serverbot: could not open %s\n", cgzname); return; }
  header hdr;
  gzread(f, &hdr, sizeof(header) - sizeof(int) * 16);
  endianswap(&hdr.version, sizeof(int), 4);
  if (strncmp(hdr.head, "CUBE", 4) != 0 || hdr.version > MAPVERSION) {
    gzclose(f); return;
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
      spawnz[numspawns] = 4.0f;
      numspawns++;
    };
  };
  printf("serverbot: found %d spawns, mapsize=%d\n", numspawns, mapsize);
  int total = mapsize * mapsize;
  walkable = (bool *)calloc(total, 1);
  if (!walkable) { gzclose(f); return; }
  int prevtype = -1;
  int k = 0;
  while (k < total) {
    int type = gzgetc(f);
    if (type < 0) break;
    if (type == 255) {
      int n = gzgetc(f);
      if (n < 0) break;
      for (int r = 0; r < n && k < total; r++, k++)
        walkable[k] = (prevtype != 0);
      continue;
    }
    prevtype = type;
    walkable[k] = (type != 0);
    k++;
    if (type == 0) {
      gzgetc(f); gzgetc(f);
      if (hdr.version <= 2) { gzgetc(f); gzgetc(f); }
    } else {
      gzgetc(f); gzgetc(f); gzgetc(f);
      gzgetc(f); gzgetc(f);
      if (hdr.version <= 2) { gzgetc(f); gzgetc(f); }
      gzgetc(f);
      if (hdr.version >= 2) gzgetc(f);
      if (hdr.version >= 5) gzgetc(f);
    }
  }
  gzclose(f);
}

int serverbot_count() { return numsbots; }

void serverbot_spawn(int count) {
  if (count < 1) count = 1;
  loadspawns();
  if (numspawns == 0) {
    spawnx[0] = spawny[0] = 64; spawnz[0] = 4;
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
    b.z = spawnz[si];
    b.yaw = (float)rnd(360);
    b.pitch = 0;
    b.roll = 0;
    b.state = CS_ALIVE;
    b.health = 100;
    b.gunselect = 1 + rnd(6);
    b.frags = 0;
    b.deaths = 0;
    b.lastaction = 0;
    b.lastmove = 0;
    b.lastattack = 0;
    b.targetyaw = b.yaw;
    numsbots++;
    n++;
  }
}

void serverbot_clear() { numsbots = 0; numspawns = 0; free(walkable); walkable = NULL; }

bool serverbot_kick(const char *name) {
  loopi(numsbots) if (!strcmp(sbot[i].name, name)) {
    memmove(&sbot[i], &sbot[i+1], (numsbots-i-1)*sizeof(serverbot));
    numsbots--;
    return true;
  }
  return false;
}

int serverbot_getid(int i) {
  if (i < 0 || i >= numsbots) return -1;
  return sbot[i].cn;
}

const char *serverbot_name(int i) {
  if (i < 0 || i >= numsbots) return NULL;
  return sbot[i].name;
}

void serverbot_damage(int cn, int damage) {
  loopi(numsbots) if (sbot[i].cn == cn) {
    sbot[i].health -= damage;
    if (sbot[i].health <= 0) {
      sbot[i].state = CS_DEAD;
      sbot[i].lastaction = enet_time_get();
    }
    return;
  }
}

void serverbot_broadcast() {
  if (!serverhost) return;
  static enet_uint32 lastbc = 0;
  if (enet_time_get() - lastbc < 40) return;
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
    putint(p, 0); putint(p, 0); putint(p, 0);
    putint(p, 0);
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
    putint(p, 0); putint(p, 0); putint(p, 0);
    putint(p, 0);
    putint(p, SV_INITC2S);
    sendstring(b.name, p);
    sendstring("", p);
    putint(p, 0);
    *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
    enet_packet_resize(pkt, p - start);
    enet_peer_send(clients[cn].peer, 0, pkt);
  }
}

void serverbot_update() {
  enet_uint32 now = enet_time_get();
  if (now - lastthink < 50) return;
  int diff = (int)(now - lastthink);
  if (diff > 200) diff = 200;
  lastthink = now;
  loopi(numsbots) {
    serverbot &b = sbot[i];
    if (b.state == CS_DEAD) {
      if (now - b.lastaction > 5000) {
        if (numspawns == 0) loadspawns();
        if (numspawns == 0) {
          spawnx[0] = spawny[0] = 64; spawnz[0] = 4; numspawns = 1;
        };
        int si = rnd(numspawns);
        b.x = spawnx[si];
        b.y = spawny[si];
        b.z = spawnz[si];
        b.health = 100;
        b.state = CS_ALIVE;
        b.lastmove = 0;
      }
      continue;
    }
    if (now - b.lastmove > 1000 + rnd(3000)) {
      b.targetyaw = (float)rnd(360);
      b.lastmove = now;
    }
    float turnrate = diff * 0.3f;
    float yd = b.targetyaw - b.yaw;
    if (fabs(yd) < turnrate) b.yaw = b.targetyaw;
    else if (yd > 0) b.yaw += turnrate;
    else b.yaw -= turnrate;
    float rad = b.yaw / 180.0f * PI;
    float nx = b.x + sinf(rad) * diff * 0.05f;
    float ny = b.y + cosf(rad) * diff * 0.05f;
    int ix = (int)nx, iy = (int)ny;
    if (walkable && ix >= 0 && iy >= 0 && ix < mapsize && iy < mapsize) {
      int idx = iy * mapsize + ix;
      if (walkable[idx]) {
        b.x = nx;
        b.y = ny;
      } else {
        b.targetyaw += 90.0f + rnd(90);
      }
    } else {
      b.targetyaw += 90.0f + rnd(90);
    }
  }
}
