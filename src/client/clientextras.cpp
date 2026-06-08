#include "../include/cube.h"
#include <cstdint>

//              D    D    D    D'   D    D    D    D'   A   A'  P   P'  I   I'
//              R,  R'  E    L    J   J'
int frame[] = {178, 184, 190, 137, 183, 189, 197, 164, 46, 51,
               54,  32,  0,   0,   40,  1,   162, 162, 67, 168};
int range[] = {6, 6, 8, 28, 1, 1, 1, 1, 8, 19, 4, 18, 40, 1, 6, 15, 1, 1, 1, 1};

void renderclient(dynent *d, bool team, const char *mdlname, bool hellpig,
                  float scale) {
  if (d->state == CS_SPECTATOR)
    return;
  int n = 3;
  float speed = 100.0f;
  float mz = d->o.z - d->eyeheight + 1.55f * scale;
  int basetime = -((intptr_t)d & 0xFFF);
  if (d->state == CS_DEAD) {
    int r;
    if (hellpig) {
      n = 2;
      r = range[3];
    } else {
      n = (intptr_t)d % 3;
      r = range[n];
    };
    basetime = d->lastaction;
    int t = lastmillis - d->lastaction;
    if (t < 0 || t > 20000)
      return;
    if (t > (r - 1) * 100) {
      n += 4;
      if (t > (r + 10) * 100) {
        t -= (r + 10) * 100;
        mz -= t * t / 10000000000.0f * t;
      };
    };
    if (mz < -1000)
      return;
  } else if (d->state == CS_LAGGED) {
    n = 17;
  } else if (d->monsterstate == M_ATTACKING) {
    n = 8;
  } else if (d->monsterstate == M_PAIN) {
    n = 10;
  } else if ((!d->move && !d->strafe) || !d->moving) {
    n = 12;
  } else if (!d->onfloor && d->timeinair > 100) {
    n = 18;
  } else {
    n = 14;
    speed = 5000 / d->maxspeed * scale;
    if (hellpig)
      speed = 300 / d->maxspeed;
  };
  if (hellpig) {
    n++;
    scale *= 32;
    mz -= 1.9f;
  };
  rendermodel(mdlname, frame[n], range[n], 0, 1.5f, d->o.x, mz, d->o.y,
              d->yaw + 90, d->pitch / 2, team, scale, speed, 0, basetime,
              0.25f);
};

extern int democlientnum;

void renderclients() {
  dynent *d;
  static int once = 0;
  if (!once) {
    loopv(players) if (players[i] && i >= BOT_CLIENT_BASE)
      printf("renderclients found bot %d at %.1f %.1f %.1f name=%s\n", i, players[i]->o.x, players[i]->o.y, players[i]->o.z, players[i]->name);
    once = 1;
  }
  loopv(players) if ((d = players[i]) &&
                     (!demoplayback || i != democlientnum)) {
    const char *mdl = "monster/player";
    if (m_teammode || m_infected) {
      if (d->team[0] && !strcmp(d->team, m_infected ? "RES" : "BLUE"))
        mdl = "monster/blueplayer";
      else
        mdl = "monster/redplayer";
    }
    renderclient(d, isteam(player1->team, d->team), mdl, false, 1.25f);
  }
};

bool scoreson = false;

int scoreboard_scroll = 0;

void showscores(bool on) {
  scoreson = on;
  menuset(((int)on) - 1);
  if (on) scoreboard_scroll = 0;
};

struct sline {
  string s;
};
vector<sline> scorelines;

void renderscore(dynent *d) {
  sprintf_sd(fbuf)("%3d", d->frags);
  sprintf_sd(dbuf)("%3d", d->deaths);
  sprintf_sd(sbuf)("%3d", d->suicides);
  sprintf_sd(pbuf)("%4d", d->ping);
  sprintf_sd(name)("%s [Dead]", d->name);
  float kd = d->deaths > 0 ? (float)d->frags / d->deaths : (float)d->frags;
  sprintf_sd(kdbuf)("%.2f", kd);
  sprintf_s(scorelines.add().s)("%s\t%s\t%s\t%s\t%s\t%s",
                                d->state == CS_DEAD ? name : d->name, kdbuf,
                                fbuf, dbuf, sbuf, pbuf);
  menumanual(0, scorelines.length() - 1, scorelines.last().s);
};

int teamscore_split = 0;

const int maxteams = 4;
char *teamname[maxteams];
int teamscore[maxteams], teamsused;
string teamscores;
int timeremain = 0;

void addteamscore(dynent *d) {
  if (!d)
    return;
  loopi(teamsused) if (strcmp(teamname[i], d->team) == 0) {
    teamscore[i] += d->frags;
    return;
  };
  if (teamsused == maxteams)
    return;
  teamname[teamsused] = d->team;
  teamscore[teamsused++] = d->frags;
};

void renderscores() {
  if (!scoreson)
    return;
  scorelines.setsize(0);
  if (m_teammode || m_infected) {
    vector<dynent *> blue, red;
#define addtoteam(d)                                                           \
  do {                                                                         \
    dynent *e = (d);                                                           \
    if (e && e->state != CS_SPECTATOR) {                                       \
      if (e->team[0] && !strcmp(e->team, m_infected ? "RES" : "BLUE"))         \
        blue.add(e);                                                           \
      else                                                                     \
        red.add(e);                                                            \
    }                                                                          \
  } while (0)
    if (!demoplayback)
      addtoteam(player1);
    loopv(players) addtoteam(players[i]);
    dvector &bs = getbots();
    loopv(bs) addtoteam(bs[i]);
#undef addtoteam
    loopv(blue) renderscore(blue[i]);
    if (blue.length())
      sortmenu(0, scorelines.length());
    loopv(red) renderscore(red[i]);
    if (red.length())
      sortmenu(scorelines.length() - red.length(), red.length());
    teamscore_split = blue.length();
  } else {
    teamscore_split = 0;
    if (!demoplayback)
      renderscore(player1);
    loopv(players) if (players[i]) renderscore(players[i]);
    dvector &bs = getbots();
    loopv(bs) if (bs[i]) renderscore(bs[i]);
    sortmenu(0, scorelines.length());
  };
};

// sendmap/getmap commands, should be replaced by more intuitive map downloading

void sendmap(char *mapname) {
  if (*mapname)
    save_world(mapname);
  changemap(mapname);
  mapname = getclientmap();
  int mapsize;
  uchar *mapdata = readmap(mapname, &mapsize);
  if (!mapdata)
    return;
  ENetPacket *packet =
      enet_packet_create(NULL, MAXTRANS + mapsize, ENET_PACKET_FLAG_RELIABLE);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_SENDMAP);
  sendstring(mapname, p);
  putint(p, mapsize);
  if (65535 - (p - start) < mapsize) {
    conoutf("Map %s is too large to send", mapname);
    free(mapdata);
    enet_packet_destroy(packet);
    return;
  };
  memcpy(p, mapdata, mapsize);
  p += mapsize;
  free(mapdata);
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  sendpackettoserv(packet);
  conoutf("Sending map %s to server...", mapname);
  sprintf_sd(msg)("[Map %s uploaded to server, \"getmap\" to receive it]",
                  mapname);
  toserver(msg);
}

void getmap() {
  ENetPacket *packet =
      enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_RECVMAP);
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  sendpackettoserv(packet);
  conoutf("requesting map from server...");
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);
