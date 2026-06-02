#include "include/cube.h"
#include <sys/stat.h>

SDL_Window *window = NULL;
SDL_GLContext glcontext = NULL;

void cleanup(char *msg) {
  stop();
  disconnect(true);
  writecfg();
  cleangl();
  cleansound();
  cleanupserver();
  TTF_Quit();
  SDL_ShowCursor(1);
  if (msg) {
#ifdef WIN32
    MessageBox(NULL, msg, "CE fatal error", MB_OK | MB_SYSTEMMODAL);
#else
    printf("%s", msg);
#endif
  };
  if (window)
    SDL_DestroyWindow(window);
  if (glcontext)
    SDL_GL_DeleteContext(glcontext);
  SDL_Quit();
  exit(1);
};

void quit() {
  writeservercfg();
  cleanup(NULL);
};

static bool __dummy_quit =
    addcommand((char *)"Exit", (void (*)())quit, ARG_NONE);

void fatal(const char *s, const char *o) {
  sprintf_sd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
  cleanup(msg);
};

void keyrepeat(bool on) {};

void *alloc(int s) {
  void *b = calloc(1, s);
  if (!b)
    fatal("Out of memory.");
  return b;
};

void setresolution();
VARFP(scr_w, 320, 1366, 7680, setresolution());
VARFP(scr_h, 240, 768, 4320, setresolution());
void setresolution() {
  if (window) {
    SDL_SetWindowSize(window, scr_w, scr_h);
    gl_init(scr_w, scr_h);
  }
};

VARFP(fullscreen, 0, 0, 1, {
  conoutf("Restart the game for fullscreen change to take effect");
  writecfg();
});

void screenshot() {
  SDL_Surface *image;
  SDL_Surface *temp;
  int idx;
  if (image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF,
                                   0x00FF00, 0xFF0000, 0)) {
    if (temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF,
                                    0x00FF00, 0xFF0000, 0)) {
      glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
      for (idx = 0; idx < scr_h; idx++) {
        char *dest = (char *)temp->pixels + 3 * scr_w * idx;
        memcpy(dest, (char *)image->pixels + 3 * scr_w * (scr_h - 1 - idx),
               3 * scr_w);
        endianswap(dest, 3, scr_w);
      };
      char *home = getenv("HOME");
      if (home) {
        sprintf_sd(hatedir)("%s/.hate", home);
        path(hatedir);
        mkdir(hatedir, 0755);
        sprintf_sd(dir)("%s/.hate/screenshots", home);
        path(dir);
        mkdir(dir, 0755);
        sprintf_sd(buf)("%s/.hate/screenshots/screenshot_%d.png", home,
                        lastmillis);
        path(buf);
        if (IMG_SavePNG(temp, buf) == 0)
          conoutf("Screenshot saved: %s", buf);
        else
          conoutf("Screenshot failed: %s", IMG_GetError());
      };
      SDL_FreeSurface(temp);
    };
    SDL_FreeSurface(image);
  };
};
COMMAND(screenshot, ARG_NONE);

void var_gamespeed();
static int gamespeed = variable((char *)"gamespeed", 10, 100, 1000, &gamespeed,
                                var_gamespeed, false);
void var_gamespeed() {
  if (multiplayer())
    gamespeed = 100;
};

int minmillis =
    variable((char *)"minmillis", 0, 7, 1000, &minmillis, __null, true);

int maxfps = variable((char *)"maxfps", 0, 144, 1000, &maxfps, __null, true);

int islittleendian = 1;
int framesinmap = 0;

int main(int argc, char **argv) {
  bool dedicated = false;
  int uprate = 0, maxcl = 4;
  char *sdesc = (char *)"", *ip = (char *)"", *master = NULL,
       *passwd = (char *)"";
  islittleendian = *((char *)&islittleendian);

#define log(s) conoutf("Initialization: %s", s)
  log("sdl");

  for (int i = 1; i < argc; i++) {
    char *a = &argv[i][2];
    if (argv[i][0] == '-')
      switch (argv[i][1]) {
      case 'd':
        dedicated = true;
        break;
      case 't':
        fullscreen = 0;
        break;
      case 'w':
        scr_w = atoi(a);
        break;
      case 'h':
        scr_h = atoi(a);
        break;
      case 'u':
        uprate = atoi(a);
        break;
      case 'n':
        sdesc = a;
        break;
      case 'i':
        ip = a;
        break;
      case 'm':
        master = a;
        break;
      case 'p':
        passwd = a;
        break;
      case 'c':
        maxcl = atoi(a);
        break;
      default:
        conoutf("Unknown commandline option");
      }
    else
      conoutf("Unknown commandline argument");
  }

  if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO) < 0)
    fatal("Unable to initialize SDL");

  log("ttf");
  if (TTF_Init() < 0)
    fatal("Unable to initialize SDL2_ttf");

  log("net");
  if (enet_initialize() < 0)
    fatal("Unable to initialize network module");

  initclient();
  initserver(dedicated, uprate, sdesc, ip, master, passwd, maxcl);

  log("world");
  empty_world(7, true);

  log("video: sdl");
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    fatal("Unable to initialize SDL Video");

  log("video: creating window");
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  {
    FILE *cfg = fopen("config.cfg", "r");
    if (cfg) {
      char line[256];
      while (fgets(line, sizeof(line), cfg)) {
        int v;
        if (sscanf(line, " fsaa %d", &v) == 1 ||
            sscanf(line, "fsaa %d", &v) == 1) {
          if (v >= 0 && v <= 16) {
            setvar("fsaa", v);
            if (v > 0) {
              SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
              SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, v);
            }
          }
          break;
        }
      }
      fclose(cfg);
    }
  }
  {
    FILE *cfg = fopen("config.cfg", "r");
    if (!cfg)
      cfg = fopen("data/default.cfg", "r");
    if (cfg) {
      char line[256];
      while (fgets(line, sizeof(line), cfg)) {
        int v;
        if (sscanf(line, " fullscreen %d", &v) == 1 ||
            sscanf(line, "fullscreen %d", &v) == 1) {
          if (v >= 0 && v <= 1)
            fullscreen = v;
          break;
        }
      }
      fclose(cfg);
    }
  }
  {
    FILE *cfg = fopen("config.cfg", "r");
    if (!cfg) {
      SDL_DisplayMode mode;
      if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        scr_w = mode.w;
        scr_h = mode.h;
      }
    } else {
      fclose(cfg);
    }
  }
  window = SDL_CreateWindow("HATE v0.0.2", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, scr_w, scr_h,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (window == NULL)
    fatal("Unable to create OpenGL screen");

  if (fullscreen) {
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_GetWindowSize(window, &scr_w, &scr_h);
  }

  SDL_GLContext glcontext = SDL_GL_CreateContext(window);
  if (glcontext == NULL)
    fatal("Unable to create OpenGL context");

  SDL_GL_SetSwapInterval(0);

  log("video: misc");
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_ShowCursor(0);

  log("gl");
  gl_init(scr_w, scr_h);

  log("basetex");
  int xs, ys;
  if (!installtex(3, path(newstring("data/martin/base.png")), xs, ys) ||
      !installtex(6, path(newstring("data/martin/ball1.png")), xs, ys) ||
      !installtex(7, path(newstring("data/martin/smoke.png")), xs, ys) ||
      !installtex(8, path(newstring("data/martin/ball2.png")), xs, ys) ||
      !installtex(9, path(newstring("data/martin/ball3.png")), xs, ys) ||
      !installtex(4, path(newstring("data/explosion.jpg")), xs, ys) ||
      !installtex(5, path(newstring("data/items.png")), xs, ys) ||
      !installtex(1, path(newstring("data/crosshair.png")), xs, ys))
    fatal("Could not find core textures (hint: run HATE from the parent of the "
          "bin directory)");
  installtex(10, path(newstring("data/boost.png")), xs, ys);

  log("preload weapons");
  preloadhudmodels();

  log("font");
  init_font();

  log("sound");

  initsound();

  log("cfg");

  newmenu("Name\tKD\tFrags\tDeaths\tSuicides\tPing\tTeam");
  newmenu("Ping\tPlr\tServer");

  exec("data/keymap.cfg");
  exec("data/menus.cfg");
  exec("data/sounds.cfg");
  preloadweaponsounds();
  exec("servers.cfg");

  if (!execfile("config.cfg")) {
    execfile("data/default.cfg");
    writecfg();
  }

  execute("bind F3 togglespectate");
  execute("bind J togglespeclook");

  log("localconnect");
  localconnect();
  changemap("flux");
  log("mainloop");

  int ignore = 5;
  int delay = 500;

  for (;;) {
    delay--;
    int millis = SDL_GetTicks() * gamespeed / 100;
    if (millis - lastmillis > 200)
      lastmillis = millis - 200;
    else if (millis - lastmillis < 1)
      lastmillis = millis - 1;
    if (maxfps > 0) {
      int maxfpsdelay = 1000 / maxfps;
      if (millis - lastmillis < maxfpsdelay)
        SDL_Delay(maxfpsdelay - (millis - lastmillis));
    }
    cleardlights();
    updateworld(millis);
    if (!demoplayback)
      serverslice((int)time(NULL), 0);
    static float fps = 30.0f;
    fps = (1000.0f / curtime + fps * 50) / 51;
    float vx, vy, vz;
    getcamerapos(vx, vy, vz);
    computeraytable(vx, vy);
    readdepth(scr_w, scr_h);
    SDL_GL_SwapWindow(window);
    extern void updatevol();
    updatevol();
    if (framesinmap++ < 5) {
      player1->yaw += 5;
      gl_drawframe(scr_w, scr_h, fps);
      player1->yaw -= 5;
    };
    gl_drawframe(scr_w, scr_h, fps);
    SDL_Event event;
    int lasttype = 0, lastbut = 0;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        quit();
        break;

      case SDL_TEXTINPUT:
        keypress(0, false, true, event.text.text);
        break;

      case SDL_KEYDOWN:
      case SDL_KEYUP:
        extern bool saycommandon;
        if (editmode && event.key.state == SDL_PRESSED && !saycommandon) {
          bool handled = true;
          switch (event.key.keysym.sym) {
          case SDLK_INSERT:
            execute("edittex 0 1");
            break;
          case SDLK_DELETE:
            execute("edittex 0 -1");
            break;
          case SDLK_PAGEUP:
            execute("edittex 2 1");
            break;
          case SDLK_PAGEDOWN:
            execute("edittex 2 -1");
            break;
          case SDLK_HOME:
            execute("edittex 1 1");
            break;
          case SDLK_END:
            execute("edittex 1 -1");
            break;
          case SDLK_u:
            execute("undo");
            break;
          case SDLK_z:
            execute("delent");
            break;
          case SDLK_LEFTBRACKET:
            execute("editheight $flrceil 1");
            break;
          case SDLK_RIGHTBRACKET:
            execute("editheight $flrceil -1");
            break;
          case SDLK_x:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("edittex 1 1");
            else
              handled = false;
            break;
          case SDLK_r:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("edittex 2 1");
            else
              handled = false;
            break;
          case SDLK_g:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("edittex 0 1");
            else
              handled = false;
            break;
          case SDLK_i:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("eyedropper");
            else
              handled = false;
            break;
          case SDLK_o:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("eyedropperpaste");
            else
              handled = false;
            break;
          default:
            handled = false;
            break;
          };
          if (!handled)
            keypress(event.key.keysym.sym, true, false, 0);
        } else if (event.key.keysym.sym == SDLK_s &&
                   (event.key.keysym.mod & KMOD_CTRL) &&
                   event.key.state == SDL_PRESSED) {
          screenshot();
        } else if (event.key.keysym.sym == SDLK_LSHIFT) {
          extern void boostn(bool);
          boostn(event.key.state == SDL_PRESSED);
        } else {
          keypress(event.key.keysym.sym, event.key.state == SDL_PRESSED, false,
                   0);
        };
        break;

      case SDL_MOUSEMOTION:
        if (ignore) {
          ignore--;
          break;
        };
        mousemove(event.motion.xrel, event.motion.yrel);
        break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (lasttype == event.type && lastbut == event.button.button)
          break;
        if (editmode && event.button.button == 1) {
          extern void editdrag(bool);
          editdrag(event.button.state != 0);
        } else {
          keypress(-event.button.button, event.button.state != 0, false, 0);
        };
        lasttype = event.type;
        lastbut = event.button.button;
        break;

      case SDL_MOUSEWHEEL:
        if (editmode) {
          const Uint8 *k = SDL_GetKeyboardState(NULL);
          if (k[SDL_SCANCODE_X]) {
            if (event.wheel.y > 0)
              execute("edittex 1 1");
            else if (event.wheel.y < 0)
              execute("edittex 1 -1");
          } else {
            extern int flrceil;
            if (event.wheel.y > 0)
              execute("editheight $flrceil 1");
            else if (event.wheel.y < 0)
              execute("editheight $flrceil -1");
          };
        } else if (spectator) {
          if (event.wheel.y > 0)
            spectate_prev();
          else if (event.wheel.y < 0)
            spectate_next();
        } else {
          if (event.wheel.y > 0)
            prevweapon();
          else if (event.wheel.y < 0)
            nextweapon();
        };
        break;
      };
    };
  };
  quit();
  return 1;
};
