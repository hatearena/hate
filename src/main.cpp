#define SDL_MAIN_HANDLED
#include "include/cube.h"
#include <sys/stat.h>
#ifdef WIN32
#include <shellapi.h>
#endif

SDL_Window *window = NULL;
SDL_GLContext glcontext = NULL;
bool shiftheld = false;
bool rshiftheld = false;

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
    addcommand((char *)"exit", (void (*)())quit, ARG_NONE);
static int __ad_Exit = (addcommanddetail("exit", "Quits the game"), 0);

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
static int __ad_scr_w =
    (addcommanddetail("scr_w", "Screen width in pixels"), 0);
VARFP(scr_h, 240, 768, 4320, setresolution());
static int __ad_scr_h =
    (addcommanddetail("scr_h", "Screen height in pixels"), 0);
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
static int __ad_fullscreen =
    (addcommanddetail("fullscreen", "Toggles fullscreen mode"), 0);

void screenshot() {
  SDL_Surface *image;
  SDL_Surface *temp;
  int idx;
  if ((image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF,
                                    0x00FF00, 0xFF0000, 0))) {
    if ((temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF,
                                     0x00FF00, 0xFF0000, 0))) {
      glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
      for (idx = 0; idx < scr_h; idx++) {
        char *dest = (char *)temp->pixels + 3 * scr_w * idx;
        memcpy(dest, (char *)image->pixels + 3 * scr_w * (scr_h - 1 - idx),
               3 * scr_w);
        endianswap(dest, 3, scr_w);
      };
      char *home = getenv("HOME");
      if (home) {
        sprintf_sd(hatedir)("%s/.hatearena", home);
        path(hatedir);
#ifdef _WIN32
        mkdir(hatedir);
#else
        mkdir(hatedir, 0755);
#endif

        sprintf_sd(dir)("%s/.hatearena/screenshots", home);
        path(dir);
#ifdef _WIN32
        mkdir(dir);
#else
        mkdir(dir, 0755);
#endif

        sprintf_sd(buf)("%s/.hatearena/screenshots/screenshot_%d.png", home,
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
static int __ad_screenshot =
    (addcommanddetail("screenshot", "Takes a screenshot"), 0);

void openurl(char *url) {
#ifdef WIN32
  ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
  sprintf_sd(cmd)("open '%s'", url);
  system(cmd);
#else
  sprintf_sd(cmd)("xdg-open '%s'", url);
  system(cmd);
#endif
}
COMMAND(openurl, ARG_1STR);
static int __ad_openurl =
    (addcommanddetail("openurl", "Opens a URL in the default web browser"), 0);

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

enum { LOADING = 0, TITLE = 1, PLAYING = 2 };
int gamestate = LOADING;
#define LOADICON 13

static void render_loading_frame(bool done) {
  int time = SDL_GetTicks();
  static int lasttime = 0;
  static int animtime = 0;
  if (!lasttime)
    lasttime = time;
  int dt = time - lasttime;
  if (dt > 50)
    dt = 50;
  lasttime = time;
  animtime += dt;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  {
    float bp = 0.5f + 0.5f * sinf(animtime / 800.0f);
    float a = 0.04f + 0.9 * bp;
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.15f, 0.01f, 0.01f, a);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(VIRTW, 0);
    glVertex2i(VIRTW, VIRTH);
    glVertex2i(0, VIRTH);
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }
  glColor4ub(255, 255, 255, 255);
  {
    int vs = VIRTH / 3;
    int vw = vs * scr_h * VIRTW / max(scr_w * VIRTH, 1);
    int ix = (VIRTW - vw) / 2;
    int iy = (VIRTH - vs) / 2 - VIRTH / 10;
    glBindTexture(GL_TEXTURE_2D, LOADICON);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(ix, iy);
    glTexCoord2f(1, 0);
    glVertex2i(ix + vw, iy);
    glTexCoord2f(1, 1);
    glVertex2i(ix + vw, iy + vs);
    glTexCoord2f(0, 1);
    glVertex2i(ix, iy + vs);
    glEnd();
    xtraverts += 4;
  }
  {
    sprintf_sd(ver)("v%s", GAME_VERSION);
    int w = text_width(ver);
    draw_text(ver, (VIRTW - w) / 2, 1050, 2, 150, 1.0f);
  }
  if (!done) {
    int n = (time / 500) % 4;
    sprintf_sd(loadtext)("Loading%s", n == 1   ? "."
                                      : n == 2 ? ".."
                                      : n == 3 ? "..."
                                               : "");
    int w = text_width(loadtext);
    draw_text(loadtext, (VIRTW - w) / 2, 1150, 2, 180, 1.0f);
  } else {
    static int start = 0;
    if (!start)
      start = time;
    float t = min((time - start) / 400.0f, 1.0f);
    float ease = 1.0f - (1.0f - t) * (1.0f - t);
    int yoff = 30 - (int)(30 * ease);
    float pulse = 0.6f + 0.4f * sinf(time / 400.0f);
    const char *msg = "Press ENTER";
    int w = text_width((char *)msg);
    draw_text((char *)msg, (VIRTW - w) / 2, 1150 + yoff, 2,
              (int)(255 * ease * pulse), 1.0f);
  }
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  {
    float va = 0.2f;
    int vs = min(VIRTW, VIRTH) / 7;
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, va);
    glVertex2i(0, 0);
    glColor4f(0, 0, 0, va);
    glVertex2i(VIRTW, 0);
    glColor4f(0, 0, 0, 0);
    glVertex2i(VIRTW, vs);
    glColor4f(0, 0, 0, 0);
    glVertex2i(0, vs);

    glColor4f(0, 0, 0, 0);
    glVertex2i(0, VIRTH - vs);
    glColor4f(0, 0, 0, 0);
    glVertex2i(VIRTW, VIRTH - vs);
    glColor4f(0, 0, 0, va);
    glVertex2i(VIRTW, VIRTH);
    glColor4f(0, 0, 0, va);
    glVertex2i(0, VIRTH);

    glColor4f(0, 0, 0, va);
    glVertex2i(0, 0);
    glColor4f(0, 0, 0, 0);
    glVertex2i(vs, 0);
    glColor4f(0, 0, 0, 0);
    glVertex2i(vs, VIRTH);
    glColor4f(0, 0, 0, va);
    glVertex2i(0, VIRTH);

    glColor4f(0, 0, 0, 0);
    glVertex2i(VIRTW - vs, 0);
    glColor4f(0, 0, 0, va);
    glVertex2i(VIRTW, 0);
    glColor4f(0, 0, 0, va);
    glVertex2i(VIRTW, VIRTH);
    glColor4f(0, 0, 0, 0);
    glVertex2i(VIRTW - vs, VIRTH);
    glEnd();
  }
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  SDL_GL_SwapWindow(window);
}

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
    {
      const char *hatedir = configdir();
#ifndef _WIN32
      mkdir(hatedir, 0755);
#else
      mkdir(hatedir);
#endif
    };
    FILE *cfg = fopen(configpath(), "r");
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
    FILE *cfg = fopen(configpath(), "r");
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
    FILE *cfg = fopen(configpath(), "r");
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
  window = SDL_CreateWindow("HATE v0.0.7", SDL_WINDOWPOS_CENTERED,
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

  if (maxfps <= 0) {
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.refresh_rate > 0)
      maxfps = mode.refresh_rate;
    else
      maxfps = 144;
  }

  log("video: misc");
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_ShowCursor(0);

  log("gl");
  gl_init(scr_w, scr_h);

  log("font");
  init_font();
  {
    int icon_xs, icon_ys;
    installtex(LOADICON, path(newstring("data/icon.png")), icon_xs, icon_ys);
  }
  render_loading_frame(false);

  log("basetex");
  int xs, ys;
  if (!installtex(3, path(newstring("data/martin/base.png")), xs, ys) ||
      !installtex(6, path(newstring("data/martin/ball1.png")), xs, ys) ||
      !installtex(7, path(newstring("data/martin/smoke.png")), xs, ys) ||
      !installtex(8, path(newstring("data/martin/ball2.png")), xs, ys) ||
      !installtex(9, path(newstring("data/martin/ball3.png")), xs, ys) ||
      !installtex(4, path(newstring("data/explosion.jpg")), xs, ys) ||
      !installtex(5, path(newstring("data/items.png")), xs, ys))
    fatal(
        "Could not find core textures\n(Hint: Run HATE from the parent of the "
        "binary directory)");
  installtex(10, path(newstring("data/boost.png")), xs, ys);
  installtex(11, path(newstring("data/nailgun.png")), xs, ys);
  installtex(12, path(newstring("data/energy.png")), xs, ys);
  render_loading_frame(false);

  log("preload weapons");
  preloadhudmodels();
  preloadhudmodel_md3();
  render_loading_frame(false);

  log("sound");

  initsound();
  render_loading_frame(false);

  log("cfg");

  newmenu("Name\tKD\tFrags\tDeaths\tSuicides\tPing");
  newmenu("Ping\tPlr\tServer");

  exec("data/keymap.cfg");
  exec("data/menus.cfg");
  exec("data/sounds.cfg");
  preloadweaponsounds();
  exec("servers.cfg");

  bool shouldsayit = false;
  static bool firstlaunch = false;

  if (!execfile(configpath())) {
    execfile("data/default.cfg");
    writecfg();
    firstlaunch = true;
  }

  execute("bind F3 togglespectate");
  execute("bind J togglespeclook");

  log("localconnect");
  localconnect();
  changemap("horizon");
  render_loading_frame(true);
  execute("music music/hate_menu.wav");
  gamestate = TITLE;
  log("main");

  int ignore = 5;
  int delay = 500;
  bool ignore_enter_up = false;

  for (;;) {
    delay--;
    int millis = SDL_GetTicks() * gamespeed / 100;
    if (millis - lastmillis > 200)
      lastmillis = millis - 200;
    else if (millis - lastmillis < 1)
      lastmillis = millis - 1;
    {
      int maxfpsdelay = 1000 / max(maxfps, 250);
      if (millis - lastmillis < maxfpsdelay)
        SDL_Delay(maxfpsdelay - (millis - lastmillis));
    }
    if (gamestate == PLAYING) {
      cleardlights();
      updateworld(millis);
      if (!demoplayback)
        serverslice((int)time(NULL), 0);
      static float fps = 30.0f;
      fps = (1000.0f / curtime + fps * 50) / 51;
      float vx, vy, vz;
      getcamerapos(vx, vy, vz);
      {
        static float last_vx = -1e10f, last_vy = -1e10f;
        static float last_yaw = -1e10f, last_pitch = -1e10f;
        float yaw = player1->yaw, pitch = player1->pitch;
        if (fabs(vx - last_vx) > 1.5f || fabs(vy - last_vy) > 1.5f ||
            fabs(yaw - last_yaw) > 3.0f || fabs(pitch - last_pitch) > 3.0f) {
          computeraytable(vx, vy);
          last_vx = vx;
          last_vy = vy;
          last_yaw = yaw;
          last_pitch = pitch;
        }
      }
      readdepth(scr_w, scr_h);
      SDL_GL_SwapWindow(window);
      extern void updatevol();
      updatevol();
      framesinmap++;
      gl_drawframe(scr_w, scr_h, fps);
      if (firstlaunch) {
        firstlaunch = false;
        execute("showmenu welcome");
      }
    } else {
      render_loading_frame(true);
    }
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
        if (gamestate == TITLE && event.key.state == SDL_PRESSED) {
          if (event.key.keysym.sym == SDLK_RETURN ||
              event.key.keysym.sym == SDLK_KP_ENTER) {
            gamestate = PLAYING;
            ignore = 5;
            ignore_enter_up = true;
            musicfadeout(10000);
          }
          break;
        }
        if (ignore_enter_up && event.key.state == SDL_RELEASED &&
            (event.key.keysym.sym == SDLK_RETURN ||
             event.key.keysym.sym == SDLK_KP_ENTER)) {
          ignore_enter_up = false;
          break;
        }
        ignore_enter_up = false;
        extern bool saycommandon;
        if (event.key.keysym.sym == SDLK_LSHIFT)
          shiftheld = event.key.state == SDL_PRESSED;
        if (event.key.keysym.sym == SDLK_RSHIFT)
          rshiftheld = event.key.state == SDL_PRESSED;
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
          case SDLK_q:
            if (event.key.keysym.mod & KMOD_SHIFT)
              execute("togglementselect");
            else
              execute("cycleent");
            break;
          case SDLK_v:
            if (event.key.keysym.mod & KMOD_SHIFT) {
              extern int showentoverlay;
              if (showentoverlay)
                execute("showentoverlay 0");
              else
                execute("showentoverlay 1");
            } else
              handled = false;
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
              execute("solid 1");
            break;
          case SDLK_h:
            execute("heightfield 0");
            break;
          case SDLK_i:
            if ((event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_SHIFT))
              execute("eyedropper");
            else
              execute("heightfield 1");
            break;
          case SDLK_8:
            execute("vdelta 1");
            break;
          case SDLK_9:
            execute("vdelta -1");
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
          shiftheld = event.key.state == SDL_PRESSED;
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
          if (k[SDL_SCANCODE_Q]) {
            if (event.wheel.y > 0)
              execute("vdelta 1");
            else if (event.wheel.y < 0)
              execute("vdelta -1");
          } else if (k[SDL_SCANCODE_X]) {
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
        } else if (scoreson) {
          const Uint8 *k = SDL_GetKeyboardState(NULL);
          if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) {
            if (event.wheel.y < 0)
              scoreboard_scroll++;
            else if (event.wheel.y > 0 && scoreboard_scroll > 0)
              scoreboard_scroll--;
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
