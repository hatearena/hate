#include "cube.h"

extern int botdifficulty, botamount;
extern char *entnames[];
extern int timelimit;
static int timelimitreg = variable("timelimit", 0, 10, 60, &timelimit, NULL, true);

enum { ID_VAR, ID_COMMAND, ID_ALIAS };
struct ident {
  int type;
  char *name;
  int min, max;
  int *storage;
  void (*fun)();
  int narg;
  char *action;
  bool persist;
};
extern hashtable<ident> *idents;

struct mitem {
  char *text, *action;
  bool checkbox;
  bool slider;
  char *slidervar;
};

struct gmenu {
  char *name;
  vector<mitem> items;
  int mwidth;
  int menusel;
  float menuselvis;
  int scrolloff;
};

vector<gmenu> menus;

int vmenu = -1;

ivector menustack;

void menuset(int menu) {
  if ((vmenu = menu) >= 1)
    resetmovement(player1);
  if (vmenu == 1)
    menus[1].menusel = 0;
};

void showmenu(char *name) {
  loopv(menus) if (i > 1 && strcmp(menus[i].name, name) == 0) {
    menuset(i);
    return;
  };
};

int menucompare(mitem *a, mitem *b) {
  const char *pa = a->text, *pb = b->text;
  while (*pa && *pa != '\t') pa++;
  while (*pb && *pb != '\t') pb++;
  if (*pa) pa++;
  if (*pb) pb++;
  float x = atof(pa);
  float y = atof(pb);
  if (x > y)
    return -1;
  if (x < y)
    return 1;
  return 0;
};

void sortmenu(int start, int num) {
  qsort(&menus[0].items[start], num, sizeof(mitem),
        (int(__cdecl *)(const void *, const void *))menucompare);
};

void refreshservers();

bool rendermenu() {
  if (vmenu < 0) {
    menustack.setsize(0);
    return false;
  };
  if (vmenu == 1)
    refreshservers();
  gmenu &m = menus[vmenu];

  if (vmenu == 0) {
    int numrows = m.items.length();
    if (numrows < 1)
      return true;
    int numcols = 1;
    for (char *p = m.name; *p; p++)
      if (*p == '\t')
        numcols++;

    enum { MAXCOLS = 16, MAXROWS = 128 };
    string hdr[MAXCOLS];
    string rows_text[MAXROWS][MAXCOLS];
    int ncols = min(numcols, MAXCOLS);
    int nrows = min(numrows, MAXROWS);

    {
      string buf;
      strcpy_s(buf, m.name);
      char *start = buf;
      loopi(ncols) {
        char *end = start;
        while (*end && *end != '\t')
          end++;
        char saved = *end;
        *end = '\0';
        strcpy_s(hdr[i], start);
        if (!saved) {
          ncols = i + 1;
          break;
        };
        start = end + 1;
      };
    };

    loopi(nrows) {
      string buf;
      strcpy_s(buf, m.items[i].text);
      char *start = buf;
      loopj(ncols) {
        char *end = start;
        while (*end && *end != '\t')
          end++;
        char saved = *end;
        *end = '\0';
        strcpy_s(rows_text[i][j], start);
        if (!saved)
          break;
        start = end + 1;
      };
    };

    int colpad = FONTH / 3;
    int gap = FONTH / 4;
    int rowstep = FONTH / 4 * 5;
    int border = FONTH / 2;

    int colw[MAXCOLS];
    loopi(ncols) {
      colw[i] = text_width(hdr[i]);
      loopj(nrows) {
        int w = text_width(rows_text[j][i]);
        if (w > colw[i])
          colw[i] = w;
      };
    };

    int tablew = border * 2;
    loopi(ncols) tablew += colw[i] + 2 * colpad;
    tablew += (ncols - 1) * gap;

    int tableh = (nrows + 1) * rowstep + border * 2;
    int x0 = (VIRTW - tablew) / 2;
    int y0 = (VIRTH - tableh) / 2;

    int colx[MAXCOLS];
    int cx = x0 + border;
    loopi(ncols) {
      colx[i] = cx + colpad;
      cx += colw[i] + 2 * colpad + gap;
    };

    overlay(160);
    {
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4ub(50, 15, 15, 217);
      roundedbox(x0, y0, x0 + tablew, y0 + tableh, FONTH / 2);
      glEnable(GL_TEXTURE_2D);
    }

    int headery = y0 + border;
    int sep_y = headery + rowstep;

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3ub(85, 50, 50);
    glBegin(GL_QUADS);
    glVertex2i(x0 + border, headery);
    glVertex2i(x0 + tablew - border, headery);
    glVertex2i(x0 + tablew - border, sep_y);
    glVertex2i(x0 + border, sep_y);
    glEnd();
    glEnable(GL_TEXTURE_2D);

    loopi(ncols) draw_text(hdr[i], colx[i], headery, 2);

    glDisable(GL_TEXTURE_2D);
    glColor3ub(140, 90, 90);
    glBegin(GL_LINES);
    glVertex2i(x0 + border, sep_y);
    glVertex2i(x0 + tablew - border, sep_y);
    glEnd();
    glEnable(GL_TEXTURE_2D);

    int datay = sep_y + 1;
    loopi(nrows) {
      if (i % 2 == 1) {
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(60, 35, 35, 100);
        glBegin(GL_QUADS);
        glVertex2i(x0 + border, datay);
        glVertex2i(x0 + tablew - border, datay);
        glVertex2i(x0 + tablew - border, datay + rowstep);
        glVertex2i(x0 + border, datay + rowstep);
        glEnd();
        glEnable(GL_TEXTURE_2D);
      };
      loopj(ncols) draw_text(rows_text[i][j], colx[j], datay, 2);

      glDisable(GL_TEXTURE_2D);
      glColor4ub(85, 55, 55, 120);
      glBegin(GL_LINES);
      glVertex2i(x0 + border, datay + rowstep);
      glVertex2i(x0 + tablew - border, datay + rowstep);
      glEnd();
      glEnable(GL_TEXTURE_2D);
      datay += rowstep;
    };

    glDisable(GL_TEXTURE_2D);
    glColor4ub(100, 65, 65, 150);
    glBegin(GL_LINES);
    cx = x0 + border;
    loopi(ncols - 1) {
      cx += colw[i] + 2 * colpad + gap / 2;
      glVertex2i(cx, y0 + border + 1);
      glVertex2i(cx, y0 + tableh - border - 1);
      cx += gap / 2;
    };
    glEnd();

    glColor3ub(120, 75, 75);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x0 + border, y0 + border);
    glVertex2i(x0 + tablew - border, y0 + border);
    glVertex2i(x0 + tablew - border, y0 + tableh - border);
    glVertex2i(x0 + border, y0 + tableh - border);
    glEnd();
    glEnable(GL_TEXTURE_2D);
    return true;
  };

  sprintf_sd(title)(vmenu > 1 ? "%s" : "%s", m.name);
  int mdisp = m.items.length();
  int w = 0;
  loopi(mdisp) {
    int tw = text_width(m.items[i].text);
    if (m.items[i].slider) {
      string buf;
      sprintf_s(buf)("%s: 999", m.items[i].text);
      tw = text_width(buf);
    };
    if (tw > w)
      w = tw;
  };
  int tw = text_width(title);
  if (tw > w)
    w = tw;
  if (w > VIRTW * 2 / 3) w = VIRTW * 2 / 3;
  int step = FONTH / 4 * 5;
  int pad = FONTH / 2 * 3;
  int maxvis = (VIRTH - 3 * step) / step;
  if (maxvis < 1) maxvis = 1;
  if (m.scrolloff > mdisp - maxvis) m.scrolloff = max(0, mdisp - maxvis);
  if (m.menusel < m.scrolloff) m.menusel = m.scrolloff;
  if (m.menusel >= m.scrolloff + maxvis) m.menusel = m.scrolloff + maxvis - 1;
  int vis = min(mdisp - m.scrolloff, maxvis);
  int h = (vis + 2) * step;
  int y = (VIRTH - h) / 2;
  int x = (VIRTW - w) / 2;
  overlay(160);
  {
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(50, 15, 15, 217);
    roundedbox(x - pad, y - FONTH, x + w + pad, y + h + FONTH, FONTH / 2);
    glEnable(GL_TEXTURE_2D);
  }
  draw_text(title, x, y, 2);
  y += FONTH * 2;
  if (vmenu) {
    float target = (float)(m.menusel - m.scrolloff);
    m.menuselvis += (target - m.menuselvis) * 0.2f;
    int bh = y + (int)(m.menuselvis * step);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glColor4ub(200, 50, 50, 217);
    glVertex2i(x - FONTH, bh - 10);
    glVertex2i(x + w + FONTH, bh - 10);
    glColor4ub(150, 30, 30, 217);
    glVertex2i(x + w + FONTH, bh + FONTH + 10);
    glVertex2i(x - FONTH, bh + FONTH + 10);
    glEnd();
    glEnable(GL_TEXTURE_2D);
  };
  loopj(vis) {
    int idx = m.scrolloff + j;
    if (m.items[idx].checkbox) {
      const char *action = m.items[idx].action;
      int check = 0;
      if (!strncmp(action, "botdifficulty ", 14) && botdifficulty == atoi(action + 14))
        check = 1;
      else if (!strncmp(action, "botamount ", 10) && botamount == atoi(action + 10))
        check = 1;
      string buf;
      sprintf_s(buf)("[%c] %s", check ? 'X' : ' ', m.items[idx].text);
      draw_text(buf, x, y, 2);
    } else if (m.items[idx].slider) {
      int val = getvar(m.items[idx].slidervar);
      string buf;
      sprintf_s(buf)("%s: %d", m.items[idx].text, val);
      draw_text(buf, x, y, 2);
    } else {
      draw_text(m.items[idx].text, x, y, 2);
    };
    y += step;
  };
  if (m.scrolloff > 0)
    draw_text("/\\", x + w / 2 - 16, y - step, 2);
  if (m.scrolloff + vis < mdisp)
    draw_text("\\/", x + w / 2 - 16, y, 2);
  return true;
};

void newmenu(const char *name) {
  gmenu &menu = menus.add();
  menu.name = newstring(name);
  menu.menusel = 0;
  menu.menuselvis = 0.0f;
  menu.scrolloff = 0;
};

void menumanual(int m, int n, char *text) {
  if (!n)
    menus[m].items.setsize(0);
  mitem &mitem = menus[m].items.add();
  mitem.text = text;
  mitem.action = (char *)"";
}

void menuitem(char *text, char *action) {
  gmenu &menu = menus.last();
  mitem &mi = menu.items.add();
  mi.text = newstring(text);
  mi.action = action[0] ? newstring(action) : mi.text;
  mi.checkbox = false;
  mi.slider = false;
};

void menuitem_checkbox(char *text, char *action) {
  gmenu &menu = menus.last();
  mitem &mi = menu.items.add();
  mi.text = newstring(text);
  mi.action = action[0] ? newstring(action) : mi.text;
  mi.checkbox = true;
  mi.slider = false;
};

void menuitem_slider(char *text, char *varname) {
  gmenu &menu = menus.last();
  mitem &mi = menu.items.add();
  mi.text = newstring(text);
  mi.action = newstring(varname);
  mi.checkbox = false;
  mi.slider = true;
  mi.slidervar = newstring(varname);
};

void showmapmodels() {
  if (!editmode && multiplayer()) return;
  if (menustack.empty() && vmenu > 0) menustack.add(vmenu);
  string mname = "mapmodelpreview";
  int mi = -1;
  loopv(menus) if (i > 1 && strcmp(menus[i].name, mname) == 0) { mi = i; break; };
  if (mi < 0) { newmenu(mname); mi = menus.length() - 1; };
  gmenu &m = menus[mi];
  m.items.setsize(0);
  m.menusel = 0;
  m.menuselvis = 0.0f;
  m.scrolloff = 0;
  int n = nummapmodels();
  loopi(n) {
    mapmodelinfo &mmi = getmminfo(i);
    string buf;
    sprintf_s(buf)("%d: %s [r:%d h:%d z:%d]", i, mmi.name, mmi.rad, mmi.h, mmi.zoff);
    mitem &mi2 = m.items.add();
    mi2.text = newstring(buf);
    mi2.action = newstring(" ");
    mi2.checkbox = false;
    mi2.slider = false;
  };
  if (menustack.empty()) menustack.add(mi);
  menuset(mi);
};

void showentities() {
  if (!editmode && multiplayer()) return;
  if (menustack.empty() && vmenu > 0) menustack.add(vmenu);
  string mname = "entitypreview";
  int mi = -1;
  loopv(menus) if (i > 1 && strcmp(menus[i].name, mname) == 0) { mi = i; break; };
  if (mi < 0) { newmenu(mname); mi = menus.length() - 1; };
  gmenu &m = menus[mi];
  m.items.setsize(0);
  m.menusel = 0;
  m.menuselvis = 0.0f;
  m.scrolloff = 0;
  loopi(MAXENTTYPES) {
    if (i == NOTUSED || i >= MAXENTTYPES) continue;
    string buf;
    const char *desc = "";
    switch (i) {
      case LIGHT:       desc = "radius"; break;
      case PLAYERSTART: desc = "angle"; break;
      case I_SHELLS:    desc = "ammo"; break;
      case I_BULLETS:   desc = "ammo"; break;
      case I_ROCKETS:   desc = "ammo"; break;
      case I_ROUNDS:    desc = "ammo"; break;
      case I_HEALTH:    desc = "health"; break;
      case I_BOOST:     desc = "health"; break;
      case I_GREENARMOUR: desc = "armour"; break;
      case I_YELLOWARMOUR: desc = "armour"; break;
      case I_QUAD:      desc = "powerup"; break;
      case TELEPORT:    desc = "idx"; break;
      case TELEDEST:    desc = "angle, idx"; break;
      case MAPMODEL:    desc = "angle, idx"; break;
      case MONSTER:     desc = "angle, type"; break;
      case CARROT:      desc = "tag, type"; break;
      case JUMPPAD:     desc = "zpush, ypush, xpush"; break;
    };
    sprintf_s(buf)("%d: %s (%s)", i, entnames[i], desc);
    mitem &mi2 = m.items.add();
    mi2.text = newstring(buf);
    mi2.action = newstring(" ");
    mi2.checkbox = false;
    mi2.slider = false;
  };
  if (menustack.empty()) menustack.add(mi);
  menuset(mi);
};

COMMAND(showmapmodels, ARG_NONE);
COMMAND(showentities, ARG_NONE);
COMMAND(menuitem, ARG_2STR);
COMMANDN(menuitem_checkbox, menuitem_checkbox, ARG_2STR);
COMMANDN(menuitem_slider, menuitem_slider, ARG_2STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);

bool menukey(int code, bool isdown) {
  if (vmenu <= 0)
    return false;
  int menusel = menus[vmenu].menusel;
  if (isdown) {
    if (code == SDLK_ESCAPE) {
      menuset(-1);
      if (!menustack.empty())
        menuset(menustack.pop());
      return true;
    } else if (code == SDLK_UP || code == -4)
      menusel--;
    else if (code == SDLK_DOWN || code == -5)
      menusel++;
    else if ((code == SDLK_LEFT || code == -1) &&
             menus[vmenu].items[menusel].slider) {
      mitem &mi = menus[vmenu].items[menusel];
      ident *id = idents->access(mi.slidervar);
      if (id && id->type == ID_VAR && *id->storage > id->min) {
        (*id->storage)--;
        if (id->fun) id->fun();
      };
      return true;
    } else if ((code == SDLK_RIGHT || code == -3) &&
               menus[vmenu].items[menusel].slider) {
      mitem &mi = menus[vmenu].items[menusel];
      ident *id = idents->access(mi.slidervar);
      if (id && id->type == ID_VAR && *id->storage < id->max) {
        (*id->storage)++;
        if (id->fun) id->fun();
      };
      return true;
    };
    gmenu &gm = menus[vmenu];
    int n = gm.items.length();
    int maxvis = (VIRTH - 3 * (FONTH / 4 * 5)) / (FONTH / 4 * 5);
    if (maxvis < 1) maxvis = 1;
    if (menusel < 0) {
      menusel = 0;
      if (gm.scrolloff > 0) gm.scrolloff--;
    } else if (menusel >= n) {
      menusel = n - 1;
      if (gm.scrolloff + maxvis < n) gm.scrolloff++;
    } else if (menusel < gm.scrolloff) {
      gm.scrolloff = menusel;
    } else if (menusel >= gm.scrolloff + maxvis) {
      gm.scrolloff = menusel - maxvis + 1;
    };
    menus[vmenu].menusel = menusel;
  } else {
    if (code == SDLK_RETURN || code == -2) {
      char *action = menus[vmenu].items[menusel].action;
      if (vmenu == 1)
        connects(getservername(menusel));
      if (menus[vmenu].items[menusel].checkbox ||
          menus[vmenu].items[menusel].slider) {
        execute(action, true);
      } else {
        menustack.add(vmenu);
        menuset(-1);
        execute(action, true);
      };
    };
  };
  return true;
};
