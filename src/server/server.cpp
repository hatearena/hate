#include "../include/cube.h"
#include <cstdint>

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct client {
  int type;
  ENetPeer *peer;
  string hostname;
  string mapvote;
  string name;
  int modevote;
  string uuid;
  bool rcon;
};

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
int mapRotationIndex = 0;

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

bool isdedicated;
ENetHost *serverhost = NULL;
int bsend = 0, brec = 0, laststatus = 0, lastsec = 0;

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
    else if (strcmp(key, "maprotation") == 0) {
      char temp[_MAXDEFSTR];
      strcpy_s(temp, vbuf);
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
  int yes = 0, no = 0;
  loopv(clients) if (clients[i].type != ST_EMPTY) {
    if (clients[i].mapvote[0]) {
      if (strcmp(clients[i].mapvote, map) == 0 &&
          clients[i].modevote == reqmode)
        yes++;
      else
        no++;
    } else
      no++;
  };
  if (yes == 1 && no == 0)
    return true;
  sprintf_sd(msg)("%s started a vote for %s on map %s (set map to vote)",
                  clients[sender].name, modestr(reqmode), map);
  sendservmsg(msg);
  if (yes / (float)(yes + no) <= 0.5f)
    return false;
  sendservmsg("Vote passed");
  resetvotes();
  return true;
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
          extern void botclear();
          botclear();
          sendservmsg("All bots have been kicked.");
          return;
        } else if (strcmp(text + 1, "list") == 0) {
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
        } else if (strncmp(text + 1, "rcon ", 5) == 0) {
          char *pass = text + 6;
          if (rconpass[0] && strcmp(pass, rconpass) == 0) {
            clients[sender].rcon = true;
            sendservmsg("Authenticated as administrator.");
          } else {
            sendservmsg("Invalid rcon password.");
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
        } else if (strncmp(text + 1, "kick ", 5) == 0) {
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
      strcpy_s(clients[cn].name, text);
      sgetstr();
      getint(p);
      break;

    case SV_MAPCHANGE: {
      sgetstr();
      int reqmode = getint(p);
      if (reqmode < 0)
        reqmode = 0;
      if (smapname[0] && !mapreload && !vote(text, reqmode, sender))
        return;
      mapreload = false;
      if (maprotation.length() > 0) {
        mapRotationIndex = (mapRotationIndex + 1) % maprotation.length();
        strcpy_s(smapname, maprotation[mapRotationIndex]);
        if (cfg_gamemode == 6) {
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
      sender = -1;
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
      int size = msgsizelookup(type);
      assert(size != -1);
      loopi(size - 2) getint(p);
      break;
    };

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
    loopv(clients) if (clients[i].type != ST_EMPTY) {
      send2(true, i, SV_MAPRELOAD, 0);
      mapreload = true;
      break;
    };
  };

  resetserverifempty();

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
  strcpy_s(c.hostname, "local");
  send_welcome(&c - &clients[0]);
};

void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master,
                char *passwd, int maxcl) {
  loadserverconf();
  loadblacklist();

  serverpassword = passwd;
  maxclients = maxcl;

  servermsinit(master ? master : "localhost/", sdesc, dedicated);

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
