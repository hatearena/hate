#include "cube.h"

struct mitem {
  char *text, *action;
};

struct gmenu {
  char *name;
  vector<mitem> items;
  int mwidth;
  int menusel;
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
  int x = atoi(a->text);
  int y = atoi(b->text);
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
    gradientbox(x0, y0, x0 + tablew, y0 + tableh, 20, 20, 50, 8, 8, 20);

    int headery = y0 + border;
    int sep_y = headery + rowstep;

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3ub(50, 55, 85);
    glBegin(GL_QUADS);
    glVertex2i(x0 + border, headery);
    glVertex2i(x0 + tablew - border, headery);
    glVertex2i(x0 + tablew - border, sep_y);
    glVertex2i(x0 + border, sep_y);
    glEnd();
    glEnable(GL_TEXTURE_2D);

    loopi(ncols) draw_text(hdr[i], colx[i], headery, 2);

    glDisable(GL_TEXTURE_2D);
    glColor3ub(90, 95, 140);
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
        glColor4ub(35, 35, 60, 100);
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
      glColor4ub(55, 55, 85, 120);
      glBegin(GL_LINES);
      glVertex2i(x0 + border, datay + rowstep);
      glVertex2i(x0 + tablew - border, datay + rowstep);
      glEnd();
      glEnable(GL_TEXTURE_2D);
      datay += rowstep;
    };

    glDisable(GL_TEXTURE_2D);
    glColor4ub(65, 65, 100, 150);
    glBegin(GL_LINES);
    cx = x0 + border;
    loopi(ncols - 1) {
      cx += colw[i] + 2 * colpad + gap / 2;
      glVertex2i(cx, y0 + border + 1);
      glVertex2i(cx, y0 + tableh - border - 1);
      cx += gap / 2;
    };
    glEnd();

    glColor3ub(75, 80, 120);
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
    int x = text_width(m.items[i].text);
    if (x > w)
      w = x;
  };
  int tw = text_width(title);
  if (tw > w)
    w = tw;
  int step = FONTH / 4 * 5;
  int h = (mdisp + 2) * step;
  int y = (VIRTH - h) / 2;
  int x = (VIRTW - w) / 2;
  overlay(160);
  int pad = FONTH / 2 * 3;
  gradientbox(x - pad, y - FONTH, x + w + pad, y + h + FONTH, 20, 20, 50, 8, 8,
              20);
  draw_text(title, x, y, 2);
  y += FONTH * 2;
  if (vmenu) {
    int bh = y + m.menusel * step;
    gradientbox(x - FONTH, bh - 10, x + w + FONTH, bh + FONTH + 10, 50, 100,
                200, 30, 60, 150);
  };
  loopj(mdisp) {
    draw_text(m.items[j].text, x, y, 2);
    y += step;
  };
  return true;
};

void newmenu(const char *name) {
  gmenu &menu = menus.add();
  menu.name = newstring(name);
  menu.menusel = 0;
};

void menumanual(int m, int n, char *text) {
  if (!n)
    menus[m].items.setsize(0);
  mitem &mitem = menus[m].items.add();
  mitem.text = text;
  mitem.action = "";
}

void menuitem(char *text, char *action) {
  gmenu &menu = menus.last();
  mitem &mi = menu.items.add();
  mi.text = newstring(text);
  mi.action = action[0] ? newstring(action) : mi.text;
};

COMMAND(menuitem, ARG_2STR);
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
    int n = menus[vmenu].items.length();
    if (menusel < 0)
      menusel = n - 1;
    else if (menusel >= n)
      menusel = 0;
    menus[vmenu].menusel = menusel;
  } else {
    if (code == SDLK_RETURN || code == -2) {
      char *action = menus[vmenu].items[menusel].action;
      if (vmenu == 1)
        connects(getservername(menusel));
      menustack.add(vmenu);
      menuset(-1);
      execute(action, true);
    };
  };
  return true;
};
