#include "../include/cube.h"
#include <cstdint>

vector<client> clients;
int maxclients = 8;
string smapname;

struct server_entity {
  bool spawned;
  int spawnsecs;
};

vector<server_entity> sents;
bool notgotitems = true;
int mode = 0;
string rconpass;
vector<char *> blacklist;
bool allowvotes = true;
float votethreshold = 0.6f;
bool allowmapvotes = true;
bool allowmodevotes = true;
bool allowkickvotes = true;
int maxping = 0;
int botcount = 0;
int botskill = 50;
char logfile_str[_MAXDEFSTR] = "";
int cfg_gamemode = -1;
vector<char *> maprotation;
vector<int> moderotation;
int map_rotation_index = 0;

void restoreserverstate(vector<entity> &ents) {
  loopv(sents) {
    sents[i].spawned = ents[i].spawned;
    sents[i].spawnsecs = 0;
  };
};

int interm = 0, minremain = 0, mapend = 0;
int timelimit = 10;
bool mapreload = false;
char *serverpassword = "";
string servername;

bool isdedicated;
ENetHost *serverhost = NULL;
int bsend = 0, brec = 0, laststatus = 0, lastsec = 0;
static int solovotetime = 0;
static string solovotemap;
static int solovotemode = 0;

#define MAXOBUF 100000

void process(ENetPacket *packet, int sender);
void multicast(ENetPacket *packet, int sender);
void disconnect_client(int n, char *reason);

void serverlog(const char *fmt, ...) {
  va_list v;
  va_start(v, fmt);
  vprintf(fmt, v);
  va_end(v);
  if (logfile_str[0]) {
    FILE *f = fopen(logfile_str, "a");
    if (f) {
      va_start(v, fmt);
      vfprintf(f, fmt, v);
      va_end(v);
      fclose(f);
    };
  };
};

void genuuid(client &c, int cn) {
  uint h = (uint)c.peer->address.host;
  uint p = c.peer->address.port;
  uint t = (uint)time(NULL);
  uint id = h ^ (p << 16) ^ (t & 0xFFFF) ^ (cn * 0x9E3779B9);
  sprintf_s(c.uuid)("%04x", id & 0xFFFF);
}

void loadblacklist() {
  FILE *f = fopen("blacklist.cfg", "r");
  if (!f)
    return;
  char line[_MAXDEFSTR];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n' || line[0] == '#')
      continue;
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = 0;
    blacklist.add(newstring(line));
  }
  fclose(f);
}

void saveblacklist() {
  FILE *f = fopen("blacklist.cfg", "w");
  if (!f)
    return;
  loopv(blacklist) fprintf(f, "%s\n", blacklist[i]);
  fclose(f);
}

bool isbanned(const char *ip) {
  loopv(blacklist) if (strcmp(blacklist[i], ip) == 0) return true;
  return false;
}

void loadserverconf() {
  FILE *f = fopen("serverconf.cfg", "r");
  if (!f) {
    f = fopen("serverconf.cfg", "w");
    if (!f)
      return;
    fprintf(f, "name \"HATE Server\"\n");
    fprintf(f, "timelimit 10\n");
    fprintf(f, "maxplayers 16\n");
    fprintf(f, "allowvotes 1\n");
    fprintf(f, "votethreshold 0.6\n");
    fprintf(f, "allowmapvotes 1\n");
    fprintf(f, "allowmodevotes 1\n");
    fprintf(f, "allowkickvotes 1\n");
    fprintf(f, "maxping 0\n");
    fprintf(f, "botskill 50\n");
    fprintf(f, "botcount 0\n");
    fprintf(f, "port 28765\n");
    fprintf(f, "serverpass \"\"\n");
    fprintf(f, "rconpass \"\"\n");
    fprintf(f, "logfile \"\"\n");
    fprintf(f, "gamemode 0\n");
    fprintf(f, "# maprotation \"flux\", \"drowned\"\n");
    fclose(f);
    return;
  }
  char line[_MAXDEFSTR];
  int lineno = 0;
  while (fgets(line, sizeof(line), f)) {
    lineno++;
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '#' || *p == '\n' || *p == '\r')
      continue;
    char *nl = strchr(p, '\n');
    if (nl)
      *nl = 0;
    nl = strchr(p, '\r');
    if (nl)
      *nl = 0;
    char key[_MAXDEFSTR], val[_MAXDEFSTR];
    if (sscanf(p, " %s %[^\n]", key, val) < 1)
      continue;
    char *vstart = p + strlen(key);
    while (*vstart == ' ' || *vstart == '\t')
      vstart++;
    char vbuf[_MAXDEFSTR];
    if (*vstart == '"') {
      int pos = 0;
      vstart++;
      while (*vstart && *vstart != '"' && pos < _MAXDEFSTR - 1)
        vbuf[pos++] = *vstart++;
      vbuf[pos] = 0;
    } else {
      strcpy_s(vbuf, vstart);
    }
    if (strcmp(key, "timelimit") == 0)
      timelimit = atoi(vbuf);
    else if (strcmp(key, "maxplayers") == 0)
      maxclients = atoi(vbuf);
    else if (strcmp(key, "rconpass") == 0)
      strcpy_s(rconpass, vbuf);
    else if (strcmp(key, "serverpass") == 0)
      serverpassword = newstring(vbuf);
    else if (strcmp(key, "allowvotes") == 0)
      allowvotes = atoi(vbuf) != 0;
    else if (strcmp(key, "votethreshold") == 0)
      votethreshold = atof(vbuf);
    else if (strcmp(key, "allowmapvotes") == 0)
      allowmapvotes = atoi(vbuf) != 0;
    else if (strcmp(key, "allowmodevotes") == 0)
      allowmodevotes = atoi(vbuf) != 0;
    else if (strcmp(key, "allowkickvotes") == 0)
      allowkickvotes = atoi(vbuf) != 0;
    else if (strcmp(key, "maxping") == 0)
      maxping = atoi(vbuf);
    else if (strcmp(key, "botskill") == 0)
      botskill = atoi(vbuf);
    else if (strcmp(key, "botcount") == 0)
      botcount = atoi(vbuf);
    else if (strcmp(key, "logfile") == 0)
      strcpy_s(logfile_str, vbuf);
    else if (strcmp(key, "gamemode") == 0)
      cfg_gamemode = atoi(vbuf);
    else if (strcmp(key, "name") == 0)
      strcpy_s(servername, vbuf);
    else if (strcmp(key, "maprotation") == 0) {
      char temp[_MAXDEFSTR];
      char *src = vstart;
      if (*src == '"')
        src++;
      int ti = 0;
      while (*src && ti < _MAXDEFSTR - 1) {
        temp[ti++] = *src;
        src++;
      }
      temp[ti] = 0;
      char *tok = temp;
      while (*tok) {
        while (*tok == ' ' || *tok == ',' || *tok == '"')
          tok++;
        if (!*tok)
          break;
        char *end = tok;
        while (*end && *end != ',' && *end != '"' && *end != ' ')
          end++;
        char saved = *end;
        *end = 0;
        if (*tok)
          maprotation.add(newstring(tok));
        *end = saved;
        tok = end;
        if (*tok == ',' || *tok == '"')
          tok++;
      }
    } else if (strcmp(key, "moderotation") == 0) {
      char temp[_MAXDEFSTR];
      strcpy_s(temp, vbuf);
      char *tok = temp;
      while (*tok) {
        while (*tok == ' ' || *tok == ',')
          tok++;
        if (!*tok)
          break;
        char *end = tok;
        while (*end && *end != ',')
          end++;
        char saved = *end;
        *end = 0;
        if (*tok)
          moderotation.add(atoi(tok));
        *end = saved;
        tok = end;
        if (*tok == ',')
          tok++;
      }
    }
  }
  fclose(f);
}

void send(int n, ENetPacket *packet) {
  if (!packet)
    return;
  switch (clients[n].type) {
  case ST_TCPIP: {
    enet_peer_send(clients[n].peer, 0, packet);
    bsend += packet->dataLength;
    break;
  };
  case ST_LOCAL:
    localservertoclient(packet->data, packet->dataLength);
    break;
  };
};

void send2(bool rel, int cn, int a, int b) {
  ENetPacket *packet =
      enet_packet_create(NULL, 32, rel ? ENET_PACKET_FLAG_RELIABLE : 0);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, a);
  putint(p, b);
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  if (cn < 0)
    process(packet, -1);
  else
    send(cn, packet);
  if (packet->referenceCount == 0)
    enet_packet_destroy(packet);
};

void sendservmsg(char *msg) {
  ENetPacket *packet =
      enet_packet_create(NULL, _MAXDEFSTR + 10, ENET_PACKET_FLAG_RELIABLE);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_SERVMSG);
  sendstring(msg, p);
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  multicast(packet, -1);
  if (packet->referenceCount == 0)
    enet_packet_destroy(packet);
  if (logfile_str[0]) {
    FILE *f = fopen(logfile_str, "a");
    if (f) {
      fprintf(f, "[%ld] %s\n", time(NULL), msg);
      fclose(f);
    }
  };
};

void disconnect_client(int n, char *reason) {
  serverlog("disconnecting client (%s) [%s]\n", clients[n].hostname, reason);
  enet_peer_disconnect(clients[n].peer, 0);
  clients[n].type = ST_EMPTY;
  serverbot_clearplayer(n);
  send2(true, -1, SV_CDIS, n);
};

void resetitems() {
  sents.setsize(0);
  notgotitems = true;
};

void pickup(uint i, int sec, int sender) {
  if (i >= (uint)sents.length())
    return;
  if (sents[i].spawned) {
    sents[i].spawned = false;
    sents[i].spawnsecs = sec;
    send2(true, sender, SV_ITEMACC, i);
  };
};

void resetvotes() { loopv(clients) clients[i].mapvote[0] = 0; };

bool vote(char *map, int reqmode, int sender) {
  strcpy_s(clients[sender].mapvote, map);
  clients[sender].modevote = reqmode;
  int total = 0, yes = 0;
  loopv(clients) if (clients[i].type != ST_EMPTY) {
    total++;
    if (clients[i].mapvote[0]) {
      if (strcmp(clients[i].mapvote, map) == 0 &&
          clients[i].modevote == reqmode)
        yes++;
    }
  };
  sprintf_sd(msg)("%s started a vote for %s on map %s (set map to vote)",
                  clients[sender].name, modestr(reqmode), map);
  sendservmsg(msg);
  if (yes > total / 2) {
    sendservmsg("Vote passed");
    resetvotes();
    return true;
  }
  return false;
};

void process(ENetPacket *packet, int sender) {
  if (ENET_NET_TO_HOST_16(*(ushort *)packet->data) != packet->dataLength) {
    disconnect_client(sender, "packet length");
    return;
  };

  uchar *end = packet->data + packet->dataLength;
  uchar *p = packet->data + 2;
  char text[MAXTRANS];
  int cn = -1, type;

  while (p < end)
    switch (type = getint(p)) {
    case SV_TEXT: {
      sgetstr();
      if (text[0] == '/') {
        if (strcmp(text + 1, "kick_all_bots") == 0) {
          if (!clients[sender].rcon) {
            sendservmsg("You are not authorized.");
            return;
          };
          extern void serverbot_clear();
          serverbot_clear();
          sendservmsg("All bots have been kicked.");
          return;
        } else if (strcmp(text + 1, "list") == 0) {
          if (!clients[sender].rcon) {
            sendservmsg("You are not authorized.");
            return;
          };
          string msg;
          int n = 0;
          loopv(clients) if (clients[i].type != ST_EMPTY) {
            sprintf_s(msg)("%s (%s) [%s]", clients[i].name, clients[i].hostname,
                           clients[i].uuid);
            sendservmsg(msg);
            n++;
          };
          sprintf_sd(count)("%d player(s) connected.", n);
          sendservmsg(count);
          return;
        } else if (strcmp(text + 1, "time") == 0) {
          sprintf_sd(msg)("Time remaining: %d minute(s)", minremain);
          sendservmsg(msg);
          return;
        } else if (strncmp(text + 1, "rcon ", 5) == 0) {
          char *pass = text + 6;
          if (rconpass[0] && strcmp(pass, rconpass) == 0) {
            clients[sender].rcon = true;
            sendservmsg("Authenticated as administrator.");
          } else {
            sendservmsg("Invalid RCON password.");
          };
          return;
        } else if (strncmp(text + 1, "ban ", 4) == 0) {
          if (!clients[sender].rcon) {
            sendservmsg("You are not authorized.");
            return;
          }
          char *target = text + 5;
          loopv(clients) if (clients[i].type != ST_EMPTY &&
                             strcmp(clients[i].uuid, target) == 0) {
            blacklist.add(newstring(clients[i].hostname));
            saveblacklist();
            sprintf_sd(msg)("Banned %s (%s).", clients[i].name,
                            clients[i].hostname);
            sendservmsg(msg);
            disconnect_client(i, "banned");
            return;
          };
          sprintf_sd(msg)("Could not find player with UUID \"%s\"", target);
          sendservmsg(msg);
          return;
        } else if (strncmp(text + 1, "addbot ", 7) == 0) {
          if (!clients[sender].rcon) {
            sendservmsg("You are not authorized.");
            return;
          };
          int n = atoi(text + 8);
          if (n < 1)
            n = 1;
          serverbot_spawn(n);
          sprintf_sd(msg)("Spawned %d bot(s).", n);
          sendservmsg(msg);
          loopv(clients) if (clients[i].type == ST_TCPIP) serverbot_sendinit(i);
          return;
        } else if (strncmp(text + 1, "kick ", 5) == 0) {
          if (!clients[sender].rcon) {
            sendservmsg("You are not authorized.");
            return;
          };
          char *target = text + 6;
          if (!target[0]) {
            sendservmsg("Usage: /kick <name or uuid>");
            return;
          }
          loopv(clients) if (clients[i].type != ST_EMPTY &&
                             strcmp(clients[i].uuid, target) == 0) {
            sprintf_sd(msg)("%s has been kicked.", clients[i].name);
            disconnect_client(i, "kicked");
            sendservmsg(msg);
            return;
          };
          loopv(clients) if (clients[i].type != ST_EMPTY &&
                             strcmp(clients[i].name, target) == 0) {
            sprintf_sd(msg)("%s has been kicked.", target);
            disconnect_client(i, "kicked");
            sendservmsg(msg);
            return;
          };
          if (serverbot_kick(target)) {
            sprintf_sd(msg)("Bot %s kicked.", target);
            sendservmsg(msg);
            return;
          };
          extern dvector &getbots();
          extern int numbots;
          dvector &bv = getbots();
          loopv(bv) if (strcmp(bv[i]->name, target) == 0) {
            gp()->dealloc(bv[i], sizeof(dynent));
            bv.remove(i);
            numbots--;
            sprintf_sd(msg)("Bot %s kicked.", target);
            sendservmsg(msg);
            return;
          };
          sprintf_sd(msg)("Could not find player or bot \"%s\"", target);
          sendservmsg(msg);
          return;
        }
      }
      break;
    }

    case SV_INITC2S:
      sgetstr();
      if (strcmp(text, GAME_VERSION)) {
        disconnect_client(sender, "version mismatch");
        break;
      }
      sgetstr();
      strcpy_s(clients[cn].name, text);
      sgetstr();
      strcpy_s(clients[cn].team, text);
      clients[cn].lifesequence = getint(p);
      clients[cn].state = CS_ALIVE;
      serverbot_setlifeseq(cn, clients[cn].lifesequence);
      break;

    case SV_MAPCHANGE: {
      sgetstr();
      int reqmode = getint(p);
      if (reqmode < 0)
        reqmode = 0;
      if (smapname[0] && !mapreload) {
        if (clients[sender].rcon) {
          resetvotes();
          solovotetime = 0;
        } else {
          int total = 0;
          loopv(clients) if (clients[i].type != ST_EMPTY) total++;
          if (total == 1) {
            if (!solovotetime) {
              solovotetime = lastsec + 3;
              strcpy_s(solovotemap, text);
              solovotemode = reqmode;
              sprintf_sd(msg)("Changing to %s on %s in 3 seconds...",
                              modestr(reqmode), text);
              sendservmsg(msg);
              return;
            }
            if (lastsec < solovotetime)
              return;
            if (strcmp(solovotemap, text) != 0 || solovotemode != reqmode) {
              solovotetime = lastsec + 3;
              strcpy_s(solovotemap, text);
              solovotemode = reqmode;
              sprintf_sd(msg)("Changing to %s on %s in 3 seconds...",
                              modestr(reqmode), text);
              sendservmsg(msg);
              return;
            }
            solovotetime = 0;
          } else if (!vote(text, reqmode, sender)) {
            return;
          }
        }
      }
      mapreload = false;
      if (maprotation.length() > 0 && smapname[0]) {
        map_rotation_index = (map_rotation_index + 1) % maprotation.length();
        strcpy_s(smapname, maprotation[map_rotation_index]);
        if (moderotation.length() > map_rotation_index) {
          mode = moderotation[map_rotation_index];
        } else if (cfg_gamemode == 6) {
          int modes[] = {0, 3, 4, 5};
          mode = modes[rand() % 4];
        } else if (cfg_gamemode >= 0) {
          mode = cfg_gamemode;
        } else {
          mode = reqmode;
        }
      } else {
        mode = reqmode;
        strcpy_s(smapname, text);
      }
      minremain = timelimit ? timelimit : 10;
      mapend = lastsec + minremain * 60;
      interm = 0;
      resetitems();
      if (botcount > 0) {
        serverbot_clear();
        serverbot_spawn(botcount);
      }
      loopv(clients) if (clients[i].type != ST_EMPTY) {
        ENetPacket *mappkt =
            enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        uchar *ms = mappkt->data, *mp = ms + 2;
        putint(mp, SV_MAPCHANGE);
        sendstring(smapname, mp);
        putint(mp, mode);
        *(ushort *)ms = ENET_HOST_TO_NET_16(mp - ms);
        enet_packet_resize(mappkt, mp - ms);
        send(i, mappkt);
        if (mappkt->referenceCount == 0)
          enet_packet_destroy(mappkt);
      }
      loopv(clients) if (clients[i].type == ST_TCPIP) serverbot_sendinit(i);
      return;
      break;
    };

    case SV_ITEMLIST: {
      int n;
      while ((n = getint(p)) != -1)
        if (notgotitems) {
          server_entity se = {false, 0};
          while (sents.length() <= n)
            sents.add(se);
          sents[n].spawned = true;
        };
      notgotitems = false;
      break;
    };

    case SV_ITEMPICKUP: {
      int n = getint(p);
      pickup(n, getint(p), sender);
      break;
    };

    case SV_PING:
      send2(false, cn, SV_PONG, getint(p));
      break;

    case SV_POS: {
      cn = getint(p);
      if (cn < 0 || cn >= clients.length() || clients[cn].type == ST_EMPTY) {
        disconnect_client(sender, "client num");
        return;
      };
      float px = getint(p) / DMF;
      float py = getint(p) / DMF;
      float pz = getint(p) / DMF;
      float pyaw = getint(p) / DAF;
      float ppitch = getint(p) / DAF;
      getint(p); // roll
      getint(p);
      getint(p);
      getint(p); // velocity
      int flags = getint(p);
      int pstate = (flags >> 5) & 3;
      clients[cn].o.x = px;
      clients[cn].o.y = py;
      clients[cn].o.z = pz;
      clients[cn].yaw = pyaw;
      clients[cn].pitch = ppitch;
      clients[cn].state = pstate;
      serverbot_trackplayer(cn, px, py, pz, pyaw, ppitch, pstate);
      break;
    };

    case SV_FRAGS:
      clients[sender].frags = getint(p);
      break;

    case SV_SENDMAP: {
      sgetstr();
      int mapsize = getint(p);
      sendmaps(sender, text, mapsize, p);
      return;
    }

    case SV_RECVMAP:
      send(sender, recvmap(sender));
      return;

    case SV_EXT: {
      for (int n = getint(p); n; n--)
        getint(p);
      break;
    };

    case SV_SHOT: {
      int gun = getint(p);
      vec from, to;
      from.x = getint(p) / DMF;
      from.y = getint(p) / DMF;
      from.z = getint(p) / DMF;
      to.x = getint(p) / DMF;
      to.y = getint(p) / DMF;
      to.z = getint(p) / DMF;
      serverbot_hitscan(gun, from, to, sender);

      if (gun == GUN_SG || gun == GUN_CG || gun == GUN_RIFLE ||
          gun == GUN_NAILGUN || gun == GUN_LIGHTGUN) {
        int qdam = gun == GUN_RIFLE     ? 100
                   : gun == GUN_SG      ? 10
                   : gun == GUN_CG      ? 30
                   : gun == GUN_NAILGUN ? 25
                                        : 15;
        vec sg[20];
        int numrays = 1;
        if (gun == GUN_SG) {
          vec dvec = to;
          vsub(dvec, from);
          float sdist = sqrtf(dotprod(dvec, dvec));
          float f = sdist * 2.0f / 1000;
          numrays = 20;
          loopi(numrays) {
            float rx = (rnd(101) - 50) * f;
            float ry = (rnd(101) - 50) * f;
            float rz = (rnd(101) - 50) * f;
            sg[i] = to;
            sg[i].x += rx;
            sg[i].y += ry;
            sg[i].z += rz;
          };
        };
        loopv(clients) {
          if (i == sender || clients[i].type == ST_EMPTY)
            continue;
          if (clients[i].state != CS_ALIVE)
            continue;
          if (i >= BOT_CLIENT_BASE)
            continue;

          float radius = 1.1f, eyeheight = 3.2f, aboveeye = 0.7f;
          vec &o = clients[i].o;
          int damage = 0;

          if (gun == GUN_SG) {
            loop(r, numrays) {
              vec ray = sg[r];
              bool hit = false;
              {
                vec v = ray, w = o;
                vsub(v, from);
                vsub(w, from);
                float c1 = dotprod(w, v);
                vec *pp;
                if (c1 <= 0)
                  pp = &from;
                else {
                  float c2 = dotprod(v, v);
                  if (c2 <= c1)
                    pp = &ray;
                  else {
                    float ff = c1 / c2;
                    vmul(v, ff);
                    vadd(v, from);
                    pp = &v;
                  };
                };
                hit = pp->x <= o.x + radius && pp->x >= o.x - radius &&
                      pp->y <= o.y + radius && pp->y >= o.y - radius &&
                      pp->z <= o.z + aboveeye && pp->z >= o.z - eyeheight;
              };
              if (hit)
                damage += qdam;
            };
          } else {
            vec v = to, w = o;
            vsub(v, from);
            vsub(w, from);
            float c1 = dotprod(w, v);
            vec *pp;
            if (c1 <= 0)
              pp = &from;
            else {
              float c2 = dotprod(v, v);
              if (c2 <= c1)
                pp = &to;
              else {
                float ff = c1 / c2;
                vmul(v, ff);
                vadd(v, from);
                pp = &v;
              };
            };
            if (pp->x <= o.x + radius && pp->x >= o.x - radius &&
                pp->y <= o.y + radius && pp->y >= o.y - radius &&
                pp->z <= o.z + aboveeye && pp->z >= o.z - eyeheight)
              damage = qdam;
          };
          if (damage > 0) {
            if (clients[sender].team[0] && clients[i].team[0] &&
                !strcmp(clients[sender].team, clients[i].team))
              continue;
            ENetPacket *dmgpkt =
                enet_packet_create(NULL, 64, ENET_PACKET_FLAG_RELIABLE);
            uchar *dpkt = dmgpkt->data;
            uchar *dp = dpkt + 2;
            putint(dp, SV_POS);
            putint(dp, sender);
            putint(dp, (int)(clients[sender].o.x * DMF));
            putint(dp, (int)(clients[sender].o.y * DMF));
            putint(dp, (int)(clients[sender].o.z * DMF));
            putint(dp, (int)(clients[sender].yaw * DAF));
            putint(dp, (int)(clients[sender].pitch * DAF));
            putint(dp, 0);
            putint(dp, 0);
            putint(dp, 0);
            putint(dp, 0);
            putint(dp, CS_ALIVE << 5);
            putint(dp, SV_DAMAGE);
            putint(dp, i);
            putint(dp, damage);
            putint(dp, clients[i].lifesequence);
            *(ushort *)dpkt = ENET_HOST_TO_NET_16(dp - dpkt);
            enet_packet_resize(dmgpkt, dp - dpkt);
            multicast(dmgpkt, -1);
            if (dmgpkt->referenceCount == 0)
              enet_packet_destroy(dmgpkt);
          };
        };
      };
      break;
    };

    case SV_DIED: {
      int actor = getint(p);
      clients[cn].lifesequence++;
      clients[cn].state = CS_DEAD;
      extern void serverbot_fragged(int);
      serverbot_fragged(actor);
      serverbot_player_died(cn);
      break;
    };

    default: {
      int size = msgsizelookup(type);
      if (size == -1) {
        disconnect_client(sender, "tag type");
        return;
      };
      loopi(size - 1) getint(p);
    };
    };

  if (p > end) {
    disconnect_client(sender, "end of packet");
    return;
  };
  multicast(packet, sender);
};

void send_welcome(int n) {
  ENetPacket *packet =
      enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
  uchar *start = packet->data;
  uchar *p = start + 2;
  putint(p, SV_INITS2C);
  putint(p, n);
  putint(p, PROTOCOL_VERSION);
  putint(p, smapname[0]);
  sendstring(serverpassword, p);
  putint(p, clients.length() > maxclients);
  sendstring(GAME_VERSION, p);
  if (smapname[0]) {
    putint(p, SV_MAPCHANGE);
    sendstring(smapname, p);
    putint(p, mode);
    putint(p, SV_ITEMLIST);
    loopv(sents) if (sents[i].spawned) putint(p, i);
    putint(p, -1);
  };
  *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
  enet_packet_resize(packet, p - start);
  send(n, packet);
  if (serverbot_count())
    serverbot_sendinit(n);
};

void multicast(ENetPacket *packet, int sender) {
  loopv(clients) {
    if (i == sender)
      continue;
    send(i, packet);
  };
};

void localclienttoserver(ENetPacket *packet) {
  process(packet, 0);
  if (!packet->referenceCount)
    enet_packet_destroy(packet);
};

client &addclient() {
  loopv(clients) if (clients[i].type == ST_EMPTY) return clients[i];
  return clients.add();
};

void checkintermission() {
  if (!minremain) {
    interm = lastsec + 15;
    mapend = lastsec + 1000;
    if (mode & 1 && mode > 2) {
      int red = 0, blue = 0;
      loopv(clients) if (clients[i].type != ST_EMPTY) {
        if (!strcmp(clients[i].team, "BLUE") || !strcmp(clients[i].team, "RES"))
          blue += clients[i].frags;
        else
          red += clients[i].frags;
      }
      extern int serverbot_teamscore(const char *);
      blue += serverbot_teamscore("BLUE") + serverbot_teamscore("RES");
      red += serverbot_teamscore("RED") + serverbot_teamscore("INFD");
      sendservmsg((char *)(blue > red ? "BLUE team wins" : "RED team wins"));
    }
  };
  send2(true, -1, SV_TIMEUP, minremain--);
};

void startintermission() {
  minremain = 0;
  checkintermission();
};

void resetserverifempty() {
  loopv(clients) if (clients[i].type != ST_EMPTY) return;
  clients.setsize(0);
  smapname[0] = 0;
  resetvotes();
  resetitems();
  mode = 0;
  mapreload = false;
  lastsec = (int)time(NULL);
  minremain = timelimit ? timelimit : 10;
  mapend = lastsec + minremain * 60;
  interm = 0;
};

int nonlocalclients = 0;
int lastconnect = 0;

void serverslice(int seconds, unsigned int timeout) {
  loopv(sents) {
    if (sents[i].spawnsecs && (sents[i].spawnsecs -= seconds - lastsec) <= 0) {
      sents[i].spawnsecs = 0;
      sents[i].spawned = true;
      send2(true, -1, SV_ITEMSPAWN, i);
    };
  };

  lastsec = seconds;

  if (timelimit && mode != 1 && seconds > mapend - minremain * 60)
    checkintermission();
  if (interm && seconds > interm) {
    interm = 0;
    if (maprotation.length() > 0) {
      map_rotation_index = (map_rotation_index + 1) % maprotation.length();
      strcpy_s(smapname, maprotation[map_rotation_index]);
      if (moderotation.length() > map_rotation_index)
        mode = moderotation[map_rotation_index];
    }
    fprintf(stderr,
            "INTERMISSION: rotating to map=%s mode=%d index=%d rotlen=%d\n",
            smapname, mode, map_rotation_index, maprotation.length());
    loopv(clients) if (clients[i].type != ST_EMPTY) {
      ENetPacket *mappkt =
          enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
      uchar *ms = mappkt->data, *mp = ms + 2;
      putint(mp, SV_MAPCHANGE);
      sendstring(smapname, mp);
      putint(mp, mode);
      *(ushort *)ms = ENET_HOST_TO_NET_16(mp - ms);
      enet_packet_resize(mappkt, mp - ms);
      for (int k = 0; k < clients.length(); k++)
        if (clients[k].type != ST_EMPTY)
          send(k, mappkt);
      if (mappkt->referenceCount == 0)
        enet_packet_destroy(mappkt);
      break;
    };
    mapreload = true;
    if (botcount > 0) {
      serverbot_clear();
      serverbot_spawn(botcount);
      loopv(clients) if (clients[i].type == ST_TCPIP) serverbot_sendinit(i);
    }
  };

  resetserverifempty();

  if (solovotetime && seconds >= solovotetime) {
    solovotetime = 0;
    mode = solovotemode;
    strcpy_s(smapname, solovotemap);
    minremain = timelimit ? timelimit : 10;
    mapend = lastsec + minremain * 60;
    interm = 0;
    resetitems();
    if (botcount > 0) {
      serverbot_clear();
      serverbot_spawn(botcount);
      loopv(clients) if (clients[i].type == ST_TCPIP) serverbot_sendinit(i);
    }
    ENetPacket *packet =
        enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start + 2;
    putint(p, SV_MAPCHANGE);
    sendstring(smapname, p);
    putint(p, mode);
    *(ushort *)start = ENET_HOST_TO_NET_16(p - start);
    enet_packet_resize(packet, p - start);
    loopv(clients) if (clients[i].type != ST_EMPTY) send(i, packet);
    if (packet->referenceCount == 0)
      enet_packet_destroy(packet);
    mapreload = false;
  }

  serverbot_update();

  if (!isdedicated)
    return;

  int numplayers = 0;
  loopv(clients) if (clients[i].type != ST_EMPTY)++ numplayers;

  if (maxping > 0) {
    loopv(clients) if (clients[i].type == ST_TCPIP && !clients[i].rcon) {
      int ping = clients[i].peer->roundTripTime;
      if (ping > maxping) {
        sprintf_sd(msg)("Kicked for high ping (%d > %d)", ping, maxping);
        disconnect_client(i, msg);
      };
    };
  };

  serverms(mode, numplayers, minremain, smapname, seconds,
           clients.length() >= maxclients);

  if (seconds - laststatus > 60) {
    nonlocalclients = 0;
    loopv(clients) if (clients[i].type == ST_TCPIP) nonlocalclients++;
    laststatus = seconds;
    if (nonlocalclients || bsend || brec)
      serverlog("status: %d remote clients, %.1f send, %.1f rec (K/sec)\n",
                nonlocalclients, bsend / 60.0f / 1024, brec / 60.0f / 1024);
    bsend = brec = 0;
  };

  ENetEvent event;
  if (enet_host_service(serverhost, &event, timeout) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT: {
      client &c = addclient();
      c.type = ST_TCPIP;
      c.peer = event.peer;
      c.peer->data = (void *)(&c - &clients[0]);
      c.rcon = false;
      c.frags = 0;
      char hn[1024];
      strcpy_s(c.hostname,
               (enet_address_get_host(&c.peer->address, hn, sizeof(hn)) == 0)
                   ? hn
                   : "localhost");
      if (isbanned(c.hostname)) {
        sprintf_sd(msg)("Your IP (%s) is banned.", c.hostname);
        enet_peer_disconnect(c.peer, 0);
        c.type = ST_EMPTY;
        serverlog("rejected banned client (%s)\n", c.hostname);
        break;
      }
      genuuid(c, &c - &clients[0]);
      serverlog("client connected (%s) uuid %s\n", c.hostname, c.uuid);
      send_welcome(lastconnect = &c - &clients[0]);
      break;
    }
    case ENET_EVENT_TYPE_RECEIVE:
      brec += event.packet->dataLength;
      process(event.packet, (intptr_t)event.peer->data);
      if (event.packet->referenceCount == 0)
        enet_packet_destroy(event.packet);
      break;

    case ENET_EVENT_TYPE_DISCONNECT:
      if ((intptr_t)event.peer->data < 0)
        break;
      serverlog("Disconnected client (%s)\n",
                clients[(intptr_t)event.peer->data].hostname);
      clients[(intptr_t)event.peer->data].type = ST_EMPTY;
      serverbot_clearplayer((intptr_t)event.peer->data);
      send2(true, -1, SV_CDIS, (intptr_t)event.peer->data);
      event.peer->data = (void *)-1;
      break;
    };

    if (numplayers > maxclients) {
      disconnect_client(lastconnect, "Max no. of clients reached");
    };
  };
#ifndef WIN32
  fflush(stdout);
#endif
};

void cleanupserver() {
  if (serverhost)
    enet_host_destroy(serverhost);
};

void localdisconnect() {
  loopv(clients) if (clients[i].type == ST_LOCAL) clients[i].type = ST_EMPTY;
};

void localconnect() {
  client &c = addclient();
  c.type = ST_LOCAL;
  c.rcon = true;
  strcpy_s(c.hostname, "local");
  send_welcome(&c - &clients[0]);
};

void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master,
                char *passwd, int maxcl) {
  loadserverconf();
  loadblacklist();

  serverpassword = passwd;
  maxclients = maxcl;

  if (servername[0])
    sdesc = servername;
  servermsinit(master ? master : "217.154.51.60/", sdesc, dedicated);

  if ((isdedicated = dedicated)) {
    ENetAddress address = {ENET_HOST_ANY, CUBE_SERVER_PORT};
    if (*ip && enet_address_set_host(&address, ip) < 0)
      printf("WARNING: server ip not resolved");
    serverhost = enet_host_create(&address, MAXCLIENTS, 0, 0, uprate);
    if (!serverhost)
      fatal("could not create server host\n");
    loopi(MAXCLIENTS) serverhost->peers[i].data = (void *)-1;
  };

  resetserverifempty();

  if (isdedicated) {
#ifdef WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
    printf(
        "Dedicated server started, waiting for clients...\nCtrl-C to exit\n\n");
    atexit(cleanupserver);
    atexit(enet_deinitialize);
    for (;;)
      serverslice(/*enet_time_get_sec()*/ time(NULL), 5);
  };
};
