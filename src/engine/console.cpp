#include "cube.h"
#include <ctype.h>

struct cline {
  char *cref;
  int outtime;
};
vector<cline> conlines;

const int ndraw = 5;
const int WORDWRAP = 80;
int conskip = 0;

bool saycommandon = false;
int saycommand_start = 0;
int saycommand_end = 0;
string commandbuf;

void setconskip(int n) {
  conskip += n;
  if (conskip < 0)
    conskip = 0;
};

COMMANDN(conskip, setconskip, ARG_1INT);

void conline(const char *sf, bool highlight) // add a line to the console buffer
{
  cline cl;
  cl.cref = conlines.length() > 100
                ? conlines.pop().cref
                : newstringbuf(""); // constrain the buffer size
  cl.outtime = lastmillis;          // for how long to keep line on screen
  conlines.insert(0, cl);
  if (highlight) // show line in a different colour, for chat etc.
  {
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
    string t;
    strn0cpy(t, s, WORDWRAP + 1);
    conline(t, n++ != 0);
    s += WORDWRAP;
  };
  conline(s, n != 0);
};

void renderconsole() // render buffer taking into account time & scrolling
{
  int nd = 0;
  struct {
    char *text;
    int age;
  } refs[ndraw];
  loopv(conlines) if (conskip
                          ? i >= conskip - 1 || i >= conlines.length() - ndraw
                          : lastmillis - conlines[i].outtime < 20000) {
    refs[nd].text = conlines[i].cref;
    refs[nd].age = lastmillis - conlines[i].outtime;
    nd++;
    if (nd == ndraw)
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
};

void mapmsg(char *s) { strn0cpy(hdr.maptitle, s, 128); };

COMMAND(saycommand, ARG_VARI);
COMMAND(mapmsg, ARG_1STR);

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

void history(int n) {
  static bool rec = false;
  if (!rec && n >= 0 && n < vhistory.length()) {
    rec = true;
    execute(vhistory[vhistory.length() - n - 1]);
    rec = false;
  };
};

COMMAND(history, ARG_1INT);

void keypress(int code, bool isdown, bool textinput, char text[32]) {
  if (textinput && saycommandon) {
    if (dont_query_next_key)
      dont_query_next_key = false;
    else {
      resetcomplete();
      char buf[] = {text[0], 0}; // please forgive me, encoding gods
      strcat_s(commandbuf, buf);
      return;
    }
  }

  if (saycommandon) // keystrokes go to commandline
  {
    if (isdown) {
      switch (code) {
      case SDLK_RETURN:
        break;

      case SDLK_BACKSPACE: {
        for (int i = 0; commandbuf[i]; i++)
          if (!commandbuf[i + 1])
            commandbuf[i] = 0;
        resetcomplete();
        break;
      };

      case SDLK_UP:
        if (histpos)
          strcpy_s(commandbuf, vhistory[--histpos]);
        break;

      case SDLK_DOWN:
        if (histpos < vhistory.length())
          strcpy_s(commandbuf, vhistory[histpos++]);
        break;

      case SDLK_TAB:
        complete(commandbuf);
        break;

#if 0
				case SDLK_v:
					if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) { pasteconsole(); return; };
#endif
      default:
        resetcomplete();
        break;
      };
    } else {
      if (code == SDLK_RETURN) {
        if (commandbuf[0]) {
          if (vhistory.empty() || strcmp(vhistory.last(), commandbuf)) {
            vhistory.add(newstring(commandbuf)); // cap this?
          };
          histpos = vhistory.length();
          if (commandbuf[0] == '/')
            execute(commandbuf, true);
          else
            toserver(commandbuf);
        };
        saycommand(NULL);
      } else if (code == SDLK_ESCAPE) {
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
