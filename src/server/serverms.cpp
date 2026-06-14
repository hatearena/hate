#include "../include/cube.h"

ENetSocket mssock = ENET_SOCKET_NULL;

void httpgetsend(ENetAddress &ad, char *hostname, char *req, char *ref,
                 char *agent) {
  if (ad.host == ENET_HOST_ANY) {
    printf("Looking up %s...\n", hostname);
    enet_address_set_host(&ad, hostname);
    if (ad.host == ENET_HOST_ANY)
      return;
  };
  if (mssock != ENET_SOCKET_NULL)
    enet_socket_destroy(mssock);
  mssock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
  if (mssock == ENET_SOCKET_NULL) {
    printf("Could not open socket\n");
    return;
  };
  if (enet_socket_connect(mssock, &ad) < 0) {
    printf("Could not connect to %s\n", hostname);
    enet_socket_destroy(mssock);
    mssock = ENET_SOCKET_NULL;
    return;
  };
  ENetBuffer buf;
  sprintf_sd(httpget)(
      "GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req,
      hostname, ref, agent);
  buf.data = httpget;
  buf.dataLength = strlen((char *)buf.data);
  printf("Sending request to %s...\n", hostname);
  enet_socket_send(mssock, NULL, &buf, 1);
};

void httpgetrecieve(ENetBuffer &buf) {
  if (mssock == ENET_SOCKET_NULL)
    return;
  enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
  if (enet_socket_wait(mssock, &events, 0) >= 0 && events) {
    int len = enet_socket_receive(mssock, NULL, &buf, 1);
    if (len <= 0) {
      enet_socket_destroy(mssock);
      mssock = ENET_SOCKET_NULL;
      return;
    };
    buf.data = ((char *)buf.data) + len;
    ((char *)buf.data)[0] = 0;
    buf.dataLength -= len;
  };
};

uchar *stripheader(uchar *b) {
  char *s = strstr((char *)b, "\n\r\n");
  if (!s)
    s = strstr((char *)b, "\n\n");
  return s ? (uchar *)s : b;
};

ENetAddress masterserver = {ENET_HOST_ANY, 28780};
int updmaster = 0;
string masterbase;
string masterpath;
uchar masterrep[MAXTRANS];
ENetBuffer masterb;

void updatemasterserver(int seconds) {
  if (seconds >
      updmaster) // send alive signal to masterserver every hour of uptime
  {
    sprintf_sd(path)("%sregister.do?action=add", masterpath);
    httpgetsend(masterserver, masterbase, path, "hateserver", "HATE Server");
    masterrep[0] = 0;
    masterb.data = masterrep;
    masterb.dataLength = MAXTRANS - 1;
    updmaster = seconds + 60;
  };
};

void checkmasterreply() {
  bool busy = mssock != ENET_SOCKET_NULL;
  httpgetrecieve(masterb);
  if (busy && mssock == ENET_SOCKET_NULL)
    printf("Masterserver replied. %s\n", stripheader(masterrep));
};

uchar *retrieveservers(uchar *buf, int buflen) {
  sprintf_sd(path)("%sretrieve.do?item=list", masterpath);
  httpgetsend(masterserver, masterbase, path, "hateserver", "HATE Server");
  if (mssock == ENET_SOCKET_NULL) {
    printf("Failed to connect to master server\n");
    buf[0] = 0;
    return buf;
  };
  ENetBuffer eb;
  buf[0] = 0;
  eb.data = buf;
  eb.dataLength = buflen - 1;
  enet_uint32 timeout = enet_time_get() + 10000;
  while (mssock != ENET_SOCKET_NULL) {
    httpgetrecieve(eb);
    if (enet_time_get() > timeout) {
      printf("retrieveservers timed out\n");
      enet_socket_destroy(mssock);
      mssock = ENET_SOCKET_NULL;
      break;
    };
  };
  if (buf[0])
    printf("received server list from master server (%d bytes)\n",
           strlen((char *)buf));
  else
    printf("master server returned empty reply\n");
  return stripheader(buf);
};

ENetSocket pongsock = ENET_SOCKET_NULL;
string serverdesc;

void serverms(int mode, int numplayers, int minremain, char *smapname,
              int seconds, bool isfull) {
  checkmasterreply();
  updatemasterserver(seconds);

  ENetBuffer buf;
  ENetAddress addr;
  uchar pong[MAXTRANS], *p;
  int len;
  enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
  buf.data = pong;
  while (enet_socket_wait(pongsock, &events, 0) >= 0 && events) {
    buf.dataLength = sizeof(pong);
    len = enet_socket_receive(pongsock, &addr, &buf, 1);
    if (len < 0)
      return;
    p = &pong[len];
    putint(p, PROTOCOL_VERSION);
    putint(p, mode);
    putint(p, numplayers);
    putint(p, minremain);
    string mname;
    strcpy_s(mname, isfull ? "[FULL] " : "");
    strcat_s(mname, smapname);
    sendstring(mname, p);
    sendstring(serverdesc, p);
    buf.dataLength = p - pong;
    enet_socket_send(pongsock, &addr, &buf, 1);
  };
};

void servermsinit(const char *master, char *sdesc, bool listen) {
  const char *mid = strstr(master, "/");
  if (!mid)
    mid = master;
  strcpy_s(masterpath, mid);
  strn0cpy(masterbase, master, mid - master + 1);
  strcpy_s(serverdesc, sdesc);

  if (listen) {
    ENetAddress address = {ENET_HOST_ANY, CUBE_SERVINFO_PORT};
    pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (pongsock == ENET_SOCKET_NULL)
      fatal("Could not create server info socket\n");
    if (enet_socket_bind(pongsock, &address) < 0)
      fatal("Could not bind server info socket\n");
  };
};
