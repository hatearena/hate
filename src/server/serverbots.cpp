#include "../include/cube.h"

extern int mode;
extern vector<client> clients;
extern ENetHost *serverhost;

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

static const char *bnames[] = {
    "Cerelo","Diaso","Ceria","Deathly","Ra","Va","Never","Abu",
    "Re","Why","Lucky","Lano","Cliff","Cobra","Liner","Chiba",
    "Dragon","Sabre","Xor","Snuff","Tux","Moon","Risco","Disco"};

int serverbot_count() { return numsbots; }

void serverbot_spawn(int count) {
  if (count < 1) count = 1;
  int n = 0;
  while (n < count && numsbots < MAXBOTS) {
    int i = numsbots;
    serverbot &b = sbot[i];
    b.cn = nextcn++;
    strcpy_s(b.name, bnames[rnd(24)]);
    b.x = (float)(rnd(401) - 200);
    b.y = (float)(rnd(401) - 200);
    b.z = 20;
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

void serverbot_clear() { numsbots = 0; }

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
    putint(p, 0);
    putint(p, 0);
    putint(p, 0);
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
    ENetPacket *pkt = enet_packet_create(NULL, 64, ENET_PACKET_FLAG_RELIABLE);
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
        b.x = (float)(rnd(401) - 200);
        b.y = (float)(rnd(401) - 200);
        b.z = 20;
        b.health = 100;
        b.state = CS_ALIVE;
        b.lastmove = 0;
      }
      continue;
    }
    if (now - b.lastmove > 3000 + rnd(4500)) {
      b.targetyaw = (float)rnd(360);
      b.lastmove = now;
    }
    float turnrate = diff * 0.3f;
    float yd = b.targetyaw - b.yaw;
    if (fabs(yd) < turnrate) b.yaw = b.targetyaw;
    else if (yd > 0) b.yaw += turnrate;
    else b.yaw -= turnrate;
    float rad = b.yaw / 180.0f * PI;
    b.x += sinf(rad) * diff * 0.05f;
    b.y += cosf(rad) * diff * 0.05f;
  }
}
