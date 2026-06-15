#include "../include/cube.h"
#include <ctype.h>

struct cline {
  char *cref;
  int outtime;
};
vector<cline> conlines;

const int WORDWRAP = 256;
VARP(msglimit, 1, 8, 50);
static int __ad_msglimit = (addcommanddetail("msglimit", "Number of visible console messages"), 0);
int conskip = 0;

bool saycommandon = false;
int saycommand_start = 0;
int saycommand_end = 0;
int commandpos = 0;
string commandbuf;

void setconskip(int n) {
  conskip += n;
  if (conskip < 0)
    conskip = 0;
};

COMMANDN(conskip, setconskip, ARG_1INT);
static int __ad_conskip = (addcommanddetail("conskip", "Scrolls console messages"), 0);

void cls() { conlines.setsize(0); };
COMMAND(cls, ARG_NONE);
static int __ad_cls = (addcommanddetail("cls", "Clears the console"), 0);

void conline(const char *sf, bool highlight) // add a line to the console buffer
{
  cline cl;
  cl.cref = conlines.length() > 100 ? conlines.pop().cref : newstringbuf("");
  cl.outtime = lastmillis;
  conlines.insert(0, cl);
  if (highlight) {
    cl.cref[0] = '\f';
    cl.cref[1] = 0;
    strcat_s(cl.cref, sf);
  } else {
    strcpy_s(cl.cref, sf);
  };
  puts(cl.cref);
#ifndef WIN32
  fflush(stdout);
#endif
};

void conoutf(const char *s, ...) {
  sprintf_sdv(sf, s);
  s = sf;
  int n = 0;
  while (strlen(s) > WORDWRAP) // cut strings to fit on screen
  {
    int wrap = WORDWRAP;
    while (wrap > 0 && s[wrap] != ' ') wrap--;
    if (wrap <= 0) wrap = WORDWRAP;
    string t;
    strn0cpy(t, s, wrap + 1);
    conline(t, n++ != 0);
    s += wrap;
    while (*s == ' ') s++;
  };
  conline(s, n != 0);
};

void renderconsole() // render buffer taking into account time & scrolling
{
  int nd = 0;
  struct {
    char *text;
    int age;
  } refs[50];
  loopv(conlines) if (conskip ? i >= conskip - 1 ||
                                    i >= conlines.length() - msglimit
                              : lastmillis - conlines[i].outtime < 20000) {
    refs[nd].text = conlines[i].cref;
    refs[nd].age = lastmillis - conlines[i].outtime;
    nd++;
    if (nd >= msglimit || nd >= 50)
      break;
  };
  loopj(nd) {
    int alpha = 255;
    int yoff = 0;
    int fade = 300;
    if (refs[j].age < fade) {
      float t = (float)refs[j].age / fade;
      alpha = (int)(t * 255);
      yoff = (int)((1.0f - t) * 20);
    };
    int xpos = FONTH / 3;
    int ypos = (FONTH / 4 * 6) * (nd - j - 1) + FONTH / 3 + yoff;
    int tw = text_width(refs[j].text);
    if (tw > 0 && alpha > 0) {
      int pad = FONTH / 8;
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4ub(0, 0, 0, alpha / 2);
      roundedbox(xpos - pad, ypos - pad, xpos + tw + pad,
                 ypos + FONTH + pad + 7, 6);
      glEnable(GL_TEXTURE_2D);
    };
    draw_text(refs[j].text, xpos, ypos, 2, alpha);
  };
};

// keymap is defined externally in keymap.cfg

struct keym {
  int code;
  char *name;
  char *action;
} keyms[256];
int numkm = 0;

void keymap(char *code, char *key) {
  keyms[numkm].code = atoi(code);
  keyms[numkm].name = newstring(key);
  keyms[numkm++].action = newstringbuf("");
};

COMMAND(keymap, ARG_2STR);
static int __ad_keymap = (addcommanddetail("keymap", "Maps a key code to a name"), 0);

void bindkey(char *key, char *action) {
  for (char *x = key; *x; x++)
    *x = toupper(*x);
  loopi(numkm) if (strcmp(keyms[i].name, key) == 0) {
    strcpy_s(keyms[i].action, action);
    return;
  };
  conoutf("Unknown key: \"%s\"", key);
};

COMMANDN(bind, bindkey, ARG_2STR);
static int __ad_bind = (addcommanddetail("bind", "Binds an action to a key"), 0);

bool dont_query_next_key = false;

void saycommand(char *init) // turns input to the command line on or off
{
  if (init != NULL) {
    dont_query_next_key = true;
    if (!saycommandon)
      saycommand_start = lastmillis;
    saycommandon = true;
  } else {
    if (saycommandon)
      saycommand_end = lastmillis;
    saycommandon = false;
    init = "";
  }
  strcpy_s(commandbuf, init);
  commandpos = strlen(commandbuf);
};

void mapmsg(char *s) { strn0cpy(hdr.maptitle, s, 128); };

COMMAND(saycommand, ARG_VARI);
static int __ad_saycommand = (addcommanddetail("saycommand", "Opens the command input console"), 0);
COMMAND(mapmsg, ARG_1STR);
static int __ad_mapmsg = (addcommanddetail("mapmsg", "Sets the map description message"), 0);

#if 0
#ifndef WIN32
#include <SDL_syswm.h>
#include <X11/Xlib.h>
#endif

void pasteconsole()
{
#ifdef WIN32
	if(!IsClipboardFormatAvailable(CF_TEXT)) return;
	if(!OpenClipboard(NULL)) return;
	char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
	strcat_s(commandbuf, cb);
	GlobalUnlock(cb);
	CloseClipboard();
#else
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	wminfo.subsystem = SDL_SYSWM_X11;
	if(!SDL_GetWMInfo(&wminfo)) return;
	int cbsize;
	char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
	if(!cb || !cbsize) return;
	int commandlen = strlen(commandbuf);
	for(char *cbline = cb, *cbend; commandlen + 1 < _MAXDEFSTR && cbline < &cb[cbsize]; cbline = cbend + 1)
	{
		cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
		if(!cbend) cbend = &cb[cbsize];
		if(commandlen + cbend - cbline + 1 > _MAXDEFSTR) cbend = cbline + _MAXDEFSTR - commandlen - 1;
		memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
		commandlen += cbend - cbline;
		commandbuf[commandlen] = '\n';
		if(commandlen + 1 < _MAXDEFSTR && cbend < &cb[cbsize]) ++commandlen;
		commandbuf[commandlen] = '\0';
	};
	XFree(cb);
#endif
};
#endif

cvector vhistory;
int histpos = 0;

static char *suggestions[MAX_SUGGESTIONS];
static int numsuggestions = 0;
static int sel_suggestion = -1;

int get_numsuggestions() { return numsuggestions; }
const char *get_suggestion(int i) { return (i >= 0 && i < numsuggestions) ? suggestions[i] : NULL; }
int get_sel_suggestion() { return sel_suggestion; }

static void clear_suggestions() {
  numsuggestions = 0;
  sel_suggestion = -1;
}

static void build_suggestions() {
  clear_suggestions();
  if (!saycommandon || commandbuf[0] != '/' || !commandbuf[1]) return;

  const char *prefix = commandbuf + 1;
  int plen = strlen(prefix);
  if (plen == 0) return;

  enumerate(idents, ident *, id,
    if (strncmp(id->name, prefix, plen) == 0) {
      if (numsuggestions < MAX_SUGGESTIONS) {
        suggestions[numsuggestions++] = id->name;
      }
    }
  );

  if (numsuggestions > 1) {
    loopi(numsuggestions - 1) loopj(numsuggestions - i - 1) {
      if (strcmp(suggestions[j], suggestions[j + 1]) > 0) {
        char *tmp = suggestions[j];
        suggestions[j] = suggestions[j + 1];
        suggestions[j + 1] = tmp;
      }
    }
  }

  if (numsuggestions > 0) sel_suggestion = 0;
}

const char *get_command_word() {
  static string word;
  if (!saycommandon || commandbuf[0] != '/') return NULL;
  const char *p = commandbuf + 1;
  int len = strcspn(p, " \t\r\n\0");
  if (len == 0) return NULL;
  strn0cpy(word, p, len + 1);
  return word;
}

void history(int n) {
  static bool rec = false;
  if (!rec && n >= 0 && n < vhistory.length()) {
    rec = true;
    execute(vhistory[vhistory.length() - n - 1]);
    rec = false;
  };
};

COMMAND(history, ARG_1INT);
static int __ad_history = (addcommanddetail("history", "Shows command history"), 0);

extern bool menutextinput;
extern char menutextbuf[];
extern char *menutextcmd;

void keypress(int code, bool isdown, bool textinput, char text[32]) {
  if (textinput && saycommandon) {
    if (dont_query_next_key)
      dont_query_next_key = false;
    else {
      int len = strlen(commandbuf);
      if (len + 1 < _MAXDEFSTR) {
        memmove(commandbuf + commandpos + 1, commandbuf + commandpos,
                len - commandpos + 1);
        commandbuf[commandpos] = text[0];
        commandpos++;
      }
      build_suggestions();
      return;
    }
  }

  if (textinput && menutextinput) {
    if (strlen(menutextbuf) < 16) {
      char buf[] = {text[0], 0};
      strcat_s(menutextbuf, buf);
    }
    return;
  }

  if (menutextinput) {
    if (isdown) {
      switch (code) {
      case SDLK_ESCAPE:
        menutextinput = false;
        break;
      case SDLK_BACKSPACE: {
        for (int i = 0; menutextbuf[i]; i++)
          if (!menutextbuf[i + 1])
            menutextbuf[i] = 0;
        break;
      }
      };
    } else {
      switch (code) {
      case SDLK_RETURN:
      case -2: {
        if (menutextbuf[0]) {
          string cmd;
          sprintf_s(cmd)("%s \"%s\"", menutextcmd, menutextbuf);
          execute(cmd, true);
          conoutf("Name changed to \"%s\"", menutextbuf);
        }
        menutextinput = false;
        break;
      }
      };
    };
    return;
  }

  if (saycommandon) {
    if (isdown) {
      switch (code) {
      case SDLK_RETURN:
        break;

      case SDLK_BACKSPACE: {
        if (commandpos > 0) {
          memmove(commandbuf + commandpos - 1, commandbuf + commandpos,
                  strlen(commandbuf) - commandpos + 1);
          commandpos--;
        }
        build_suggestions();
        break;
      };

      case SDLK_DELETE: {
        if (commandbuf[commandpos]) {
          memmove(commandbuf + commandpos, commandbuf + commandpos + 1,
                  strlen(commandbuf) - commandpos);
        }
        build_suggestions();
        break;
      };

      case SDLK_LEFT:
        if (commandpos > 0)
          commandpos--;
        break;

      case SDLK_RIGHT:
        if (commandbuf[commandpos])
          commandpos++;
        break;

      case SDLK_HOME:
        commandpos = 0;
        break;

      case SDLK_END:
        commandpos = strlen(commandbuf);
        break;

      case SDLK_UP:
        if (numsuggestions > 0) {
          sel_suggestion--;
          if (sel_suggestion < 0) sel_suggestion = numsuggestions - 1;
        } else if (histpos) {
          strcpy_s(commandbuf, vhistory[--histpos]);
          commandpos = strlen(commandbuf);
        }
        break;

      case SDLK_DOWN:
        if (numsuggestions > 0) {
          sel_suggestion++;
          if (sel_suggestion >= numsuggestions) sel_suggestion = 0;
        } else if (histpos < vhistory.length()) {
          strcpy_s(commandbuf, vhistory[histpos++]);
          commandpos = strlen(commandbuf);
        }
        break;

      case SDLK_TAB:
        if (numsuggestions > 0 && sel_suggestion >= 0) {
          strcpy_s(commandbuf, "/");
          strcat_s(commandbuf, suggestions[sel_suggestion]);
          commandpos = strlen(commandbuf);
          clear_suggestions();
        } else {
          complete(commandbuf);
          commandpos = strlen(commandbuf);
        }
        break;

#if 0
				case SDLK_v:
					if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) { pasteconsole(); return; };
#endif
      default:
        build_suggestions();
        break;
      };
    } else {
      if (code == SDLK_RETURN) {
        clear_suggestions();
        if (commandbuf[0]) {
          if (vhistory.empty() || strcmp(vhistory.last(), commandbuf)) {
            vhistory.add(newstring(commandbuf));
          };
          histpos = vhistory.length();
          if (commandbuf[0] == '/')
            execute(commandbuf, true);
          else
            toserver(commandbuf);
        };
        saycommand(NULL);
      } else if (code == SDLK_ESCAPE) {
        clear_suggestions();
        saycommand(NULL);
      };
    };
  } else if (!menukey(code, isdown)) // keystrokes go to menu
  {
    loopi(
        numkm) if (keyms[i].code ==
                   code) // keystrokes go to game, lookup in keymap and execute
    {
      string temp;
      strcpy_s(temp, keyms[i].action);
      execute(temp, isdown);
      return;
    };
  };
};

char *getcurcommand() { return saycommandon ? commandbuf : NULL; };

void writebinds(FILE *f) {
  loopi(numkm) {
    if (*keyms[i].action)
      fprintf(f, "bind \"%s\" [%s]\n", keyms[i].name, keyms[i].action);
  };
};
