// command
extern int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
extern void setvar(char *name, int i);
extern int getvar(char *name);
extern bool identexists(char *name);
extern bool addcommand(char *name, void (*fun)(), int narg);
extern int execute(char *p, bool down = true);
extern void exec(const char *cfgfile);
extern bool execfile(const char *cfgfile);
extern void resetcomplete();
extern void complete(char *s);
extern void alias(char *name, char *action);
extern char *getalias(char *name);
extern void writecfg();

// console
extern void keypress(int code, bool isdown, bool textinput, char text[32]);
extern void renderconsole();
extern void conoutf(const char *s, ...);
extern char *getcurcommand();
extern void writebinds(FILE *f);

// menus
extern bool rendermenu();
extern void menuset(int menu);
extern void menumanual(int m, int n, char *text);
extern void sortmenu(int start, int num);
extern bool menukey(int code, bool isdown);
extern void newmenu(const char *name);
extern void menuitem_text(char *text, char *varname);

// serverbrowser
extern void addserver(char *servername);
extern char *getservername(int n);
extern void writeservercfg();

// rendergl
extern void gl_init(int w, int h);
extern void cleangl();
extern void gl_drawframe(int w, int h, float curfps);
extern void getcamerapos(float &vx, float &vy, float &vz);
extern bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp = false);
extern void mipstats(int a, int b, int c);
extern void vertf(float v1, float v2, float v3, sqr *ls, float t1, float t2);
extern void addstrip(int tex, int start, int n);
extern int lookuptexture(int tex, int &xs, int &ys);

// rendercubes
extern void resetcubes();
extern void render_flat(int tex, int x, int y, int size, int h, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2, float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool topleft);
extern void render_tris(int x, int y, int size, bool topleft, sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v);
extern void addwaterquad(int x, int y, int size);
extern int renderwater(float hf);
extern void finishstrips();
extern void setarraypointers();

// client
extern void localservertoclient(uchar *buf, int len);
extern void connects(char *servername);
extern void disconnect(int onlyclean = 0, int async = 0);
extern void toserver(char *text);
extern void addmsg(int rel, int num, int type, ...);
extern bool multiplayer();
extern bool allowedittoggle();
extern bool noteditmode();
extern void pruneundos(int maxremain);
extern void toggleedit();
extern void togglescreenshotmode();
extern void edittypexy(int type, block &sel);
extern void cursorupdate();
extern void settagareas();
extern void resettagareas();
extern void sendpackettoserv(void *packet);
extern void gets2c();
extern void c2sinfo(dynent *d);
extern void neterr(char *s);
extern void initclientnet();
extern bool netmapstart();
extern int getclientnum();
extern void changemapserv(char *name, int mode);
extern void writeclientinfo(FILE *f);

// clientgame
extern int thirdperson;
extern void infected_checkpick();
extern bool spectator;
extern bool speclook;
extern dynent *getspectarget();
extern void spectate_next();
extern void spectate_prev();
extern void togglespectate();
extern void togglespeclook();
extern void mousemove(int dx, int dy);
extern void updateworld(int millis);
extern void startmap(char *name);
extern void changemap(const char *name);
extern void initclient();
extern void spawnplayer(dynent *d);
extern void spawnstate(dynent *d);
extern void selfdamage(int damage, int actor, dynent *act);
extern dynent *newdynent();
extern char *getclientmap();
extern const char *modestr(int n);
extern void zapdynent(dynent *&d);
extern dynent *getclient(int cn);
extern void timeupdate(int timeremain);
extern void resetmovement(dynent *d);
extern void fixplayer1range();

// clientextras
extern void renderclients();
extern void renderclient(dynent *d, bool team, const char *mdlname, bool hellpig, float scale);
extern bool scoreson;
void showscores(bool on);
extern void renderscores();
extern int scoreboard_scroll;

// world
extern void setupworld(int factor);
extern void empty_world(int factor, bool force = true);
extern void remip(block &b, int level = 0);
extern void remipmore(block &b, int level = 0);
extern int findentity(int type, int index = 0);
extern void trigger(int tag, int type, bool savegame);
extern void resettagareas();
extern void settagareas();
extern int findtype(char *what);
extern entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4);

// worldlight
extern void calclight();
extern void dodynlight(vec &vold, vec &v, int reach, int strength, dynent *owner);
extern void cleardlights();
extern block *blockcopy(block &b);
extern void blockpaste(block &b);

// worldrender
extern void render_world(float vx, float vy, float vh, int yaw, int pitch, float widef, int w, int h);

// worldocull
extern void computeraytable(float vx, float vy);
extern int isoccluded(float vx, float vy, float cx, float cy, float csize);

// main
extern void fatal(const char *s, const char *o = "");
extern void *alloc(int s);
extern void keyrepeat(bool on);

// rendertext
extern void init_font();
extern void draw_text(char *str, int left, int top, int gl_num, int alpha = 255, float scale = 1.0f);
extern void draw_textf(char *fstr, int left, int top, int gl_num, ...);
extern int text_width(char *str);
extern void draw_envbox(int t, int fogdist);

// editing
extern void edittypexy(int type, block &sel);
extern void editheightxy(bool isfloor, int amount, block &sel);

// renderextras
extern void line(int x1, int y1, float z1, int x2, int y2, float z2);
extern void box(block &b, float z1, float z2, float z3, float z4);
extern void dot(int x, int y, float z);
extern void linestyle(float width, int r, int g, int b);
extern void newsphere(vec &o, float max, int type);
extern void renderspheres(int time);
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void readdepth(int w, int h);
extern void drawicon(float tx, float ty, int x, int y, int s);
extern void blendbox(int x1, int y1, int x2, int y2, bool border);
extern void overlay(int a);
extern void roundedbox(int x1, int y1, int x2, int y2, int r);
extern void gradientbox(int x1, int y1, int x2, int y2, int r1, int g1, int b1, int r2, int g2, int b2);
extern void damageblend(int n);
extern void newbeam(vec &from, vec &to, int duration, int color = 0);
extern void renderbeams(int time);

// renderparticles
extern void setorient(vec &r, vec &u);
extern void newparticlecol(vec &o, vec &d, int fade, int type, uchar r, uchar g, uchar b);
extern void particle_splash(int type, int num, int fade, vec &p);
extern void particle_trail(int type, int fade, vec &from, vec &to);
extern void render_particles(int time);

// worldio
extern void save_world(char *fname);
extern void load_world(char *mname);
extern void writemap(char *mname, int msize, uchar *mdata);
extern uchar *readmap(char *mname, int *msize);
extern void loadgamerest();
extern void incomingdemodata(uchar *buf, int len, bool extras = false);
extern void demoplaybackstep();
extern void stop();
extern void stopifrecording();
extern void demodamage(int damage, vec &o);
extern void demoblend(int damage);

// physics
extern float rad(float x);
extern void moveplayer(dynent *pl, int moveres, bool local);
extern bool collide(dynent *d, bool spawn, float drop, float rise);
extern void entinmap(dynent *d);
extern void setentphysics(int mml, int mmr);
extern void physicsframe();
extern void stoprunsound();

// sound
extern void playsound(int n, vec *loc = 0);
extern void preloadweaponsounds();
extern void playsoundc(int n);
extern void playsoundmax(int n);
extern int playsoundloop(int n, vec *loc = 0);
extern void stopchan(int chan);
extern void stopsounds();
extern void initsound();
extern void cleansound();

// rendermd2
extern void preloadhudmodels();
extern void rendermodel(const char *mdl, int frame, int range, int tex, float rad, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int snap = 0, int basetime = 0, float glow = 0);
extern mapmodelinfo &getmminfo(int i);
extern int nummapmodels();

// rendermd3
extern void preloadhudmodel_md3();
extern void rendermodel_md3(char *mdl, int frame, int range, int tex, float rad, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int snap = 0, int basetime = 0);

// server
extern void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master, char *passwd, int maxcl);
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void localclienttoserver(struct _ENetPacket *);
extern void serverslice(int seconds, unsigned int timeout);
extern void putint(uchar *&p, int n);
extern int getint(uchar *&p);
extern void sendstring(char *t, uchar *&p);
extern void startintermission();
extern void restoreserverstate(vector<entity> &ents);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern char msgsizelookup(int msg);
extern void serverms(int mode, int numplayers, int minremain, char *smapname, int seconds, bool isfull);
extern void servermsinit(const char *master, char *sdesc, bool listen);
extern void sendmaps(int n, string mapname, int mapsize, uchar *mapdata);
extern ENetPacket *recvmap(int n);

// weapon
extern void selectgun(int a = -1, int b = -1, int c =-1);
extern void nextweapon();
extern void prevweapon();
extern void shoot(dynent *d, vec &to);
extern void shootv(int gun, vec &from, vec &to, dynent *d = 0, bool local = false);
extern void createrays(vec &from, vec &to, bool ads = false);
extern void moveprojectiles(float time);
extern void projreset();
extern char *playerincrosshair();
extern int reloadtime(int gun);

// monster
extern void monsterclear();
extern void restoremonsterstate();
extern void monsterthink();
extern void monsterrender();
extern dvector &getmonsters();
extern void monsterpain(dynent *m, int damage, dynent *d);
extern void endsp(bool allkilled);

// bot
extern void botclear();
extern void botthink();
extern void botrender();
extern dvector &getbots();
extern void botpain(dynent *m, int damage, dynent *d);
extern void spawngibs(vec &pos, int count);

// server bots
extern int serverbot_count();
extern int serverbot_getid(int i);
extern const char *serverbot_name(int i);
extern void serverbot_damage(int cn, int damage, int attacker);
extern void serverbot_spawn(int count);
extern void serverbot_clear();
extern void serverbot_update_weapons();
extern bool serverbot_kick(const char *name);
extern void serverbot_update();
extern void serverbot_broadcast();
extern void serverbot_sendinit(int cn);
extern void serverbot_trackplayer(int cn, float x, float y, float z, float yaw,
                                  float pitch, int state);
extern void serverbot_clearplayer(int cn);
extern void serverbot_player_died(int cn);
extern void serverbot_setlifeseq(int cn, int ls);
extern void serverbot_hitscan(int gun, vec &from, vec &to, int sender);

// entities
extern void putitems(uchar *&p);
extern void checkquad(int time);
extern void checkitems();
extern void realpickup(int n, dynent *d);
extern void renderentities();
extern void resetspawns();
extern void setspawn(uint i, bool on);
extern void teleport(int n, dynent *d);
extern void baseammo(int gun);

// menus
extern void showentities();

// rndmap
extern void perlinarea(block &b, int scale, int seed, int psize);

