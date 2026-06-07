#include "../include/cube.h"

#define USE_MIXER

VARP(soundvol, 0, 255, 255);
VARP(musicvol, 0, 128, 255);
bool nosound = false;

#define MAXCHAN 32
#define SOUNDFREQ 22050

struct soundloc {
  vec loc;
  bool inuse;
} soundlocs[MAXCHAN];

int soundchan[MAXCHAN];
int chanprio[MAXCHAN];
vector<int> soundmax;
VAR(maxsamesound, 0, 0, 8);

static bool isgunshot(int n) {
  switch (n) {
  case S_RIFLE:
  case S_SG:
  case S_CG:
  case S_RLFIRE:
  case S_NAILGUN:
  case S_CSAW:
  case S_FLAUNCH:
  case S_ICEBALL:
  case S_SLIMEBALL:
    return true;
  default:
    return false;
  }
}

#ifdef USE_MIXER
#include "SDL_mixer.h"
#define MAXVOL MIX_MAX_VOLUME
Mix_Music *mod = NULL;
void *stream = NULL;
#else
#include "fmod.h"
#define MAXVOL 255
FMUSIC_MODULE *mod = NULL;
FSOUND_STREAM *stream = NULL;
#endif

void stopsound() {
  if (nosound)
    return;
  if (mod) {
#ifdef USE_MIXER
    Mix_HaltMusic();
    Mix_FreeMusic(mod);
#else
    FMUSIC_FreeSong(mod);
#endif
    mod = NULL;
  };
  if (stream) {
#ifndef USE_MIXER
    FSOUND_Stream_Close(stream);
#endif
    stream = NULL;
  };
};

VAR(soundbufferlen, 128, 1024, 4096);

void initsound() {
  memset(soundlocs, 0, sizeof(soundloc) * MAXCHAN);
  loopi(MAXCHAN) {
    soundchan[i] = -1;
    chanprio[i] = 0;
  }
#ifdef USE_MIXER
  if (Mix_OpenAudio(SOUNDFREQ, MIX_DEFAULT_FORMAT, 2, soundbufferlen) < 0) {
    conoutf("Sound init failed (SDL_Mixer): %s", (size_t)Mix_GetError());
    nosound = true;
  };
  Mix_AllocateChannels(MAXCHAN);
#else
  if (FSOUND_GetVersion() < FMOD_VERSION)
    fatal("You're using an old FMOD dll");
  if (!FSOUND_Init(SOUNDFREQ, MAXCHAN, FSOUND_INIT_GLOBALFOCUS)) {
    conoutf("Sound init failed (FMOD): %d", FSOUND_GetError());
    nosound = true;
  };
#endif
};

void music(char *name) {
  if (nosound)
    return;
  stopsound();
  if (soundvol && musicvol) {
    string sn;
    strcpy_s(sn, "packages/");
    strcat_s(sn, name);
#ifdef USE_MIXER
    if (mod = Mix_LoadMUS(path(sn))) {
      Mix_PlayMusic(mod, -1);
      Mix_VolumeMusic((musicvol * MAXVOL) / 255);
    };
#else
    if (mod = FMUSIC_LoadSong(path(sn))) {
      FMUSIC_PlaySong(mod);
      FMUSIC_SetMasterVolume(mod, musicvol);
    } else if (stream =
                   FSOUND_Stream_Open(path(sn), FSOUND_LOOP_NORMAL, 0, 0)) {
      int chan = FSOUND_Stream_Play(FSOUND_FREE, stream);
      if (chan >= 0) {
        FSOUND_SetVolume(chan, (musicvol * MAXVOL) / 255);
        FSOUND_SetPaused(chan, false);
      };
    } else {
      conoutf("Could not play music: %s", sn);
    };
#endif
  };
};

COMMAND(music, ARG_1STR);

#ifdef USE_MIXER
vector<Mix_Chunk *> samples;
#else
vector<FSOUND_SAMPLE *> samples;
#endif

cvector snames;

int registersound(char *name) {
  loopv(snames) if (strcmp(snames[i], name) == 0) return i;
  snames.add(newstring(name));
  samples.add(NULL);
  soundmax.add(0);
  return samples.length() - 1;
};

void setsoundmax(int n, int max) {
  if (n >= 0 && n < soundmax.length())
    soundmax[n] = max;
};

COMMAND(setsoundmax, ARG_2INT);

COMMAND(registersound, ARG_1EST);

void cleansound() {
  if (nosound)
    return;
  stopsound();
#ifdef USE_MIXER
  Mix_CloseAudio();
#else
  FSOUND_Close();
#endif
};

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, vec *loc) {
  int vol = soundvol, pan = 255 / 2;
  if (loc) {
    vdist(dist, v, *loc, player1->o);
    vol -= (int)(dist * 3 * soundvol / 255); // simple mono distance attenuation
    if (stereo && (v.x != 0 || v.y != 0)) {
      float yaw = -atan2(v.x, v.y) -
                  player1->yaw *
                      (PI / 180.0f); // relative angle of sound along X-Y axis
      pan = int(255.9f * (0.5 * sin(yaw) +
                          0.5f)); // range is from 0 (left) to 255 (right)
    };
  };
  vol = (vol * MAXVOL) / 255;
#ifdef USE_MIXER
  Mix_Volume(chan, vol);
  Mix_SetPanning(chan, 255 - pan, pan);
#else
  FSOUND_SetVolume(chan, vol);
  FSOUND_SetPan(chan, pan);
#endif
};

void newsoundloc(int chan, vec *loc) {
  assert(chan >= 0 && chan < MAXCHAN);
  soundlocs[chan].loc = *loc;
  soundlocs[chan].inuse = true;
};

void updatevol() {
  if (nosound)
    return;
  loopi(MAXCHAN) if (soundlocs[i].inuse) {
#ifdef USE_MIXER
    if (Mix_Playing(i))
#else
    if (FSOUND_IsPlaying(i))
#endif
      updatechanvol(i, &soundlocs[i].loc);
    else {
      soundlocs[i].inuse = false;
      soundchan[i] = -1;
    };
  };
};

void playsoundc(int n) {
  if (editmode)
    return;
  addmsg(0, 2, SV_SOUND, n);
  playsound(n);
};

void playsoundmax(int n) {
  if (nosound)
    return;
  if (editmode)
    return;
  if (n < 0 || n >= samples.length())
    return;
  if (!samples[n]) {
    sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);
    samples[n] = Mix_LoadWAV(path(buf));
    if (!samples[n]) {
      conoutf("Failed to load sample: %s", buf);
      return;
    }
  }
  int chan = Mix_PlayChannel(-1, samples[n], 0);
  if (chan < 0)
    return;
  soundchan[chan] = n;
  Mix_Volume(chan, MIX_MAX_VOLUME);
};

int soundsatonce = 0, lastsoundmillis = 0;

void playsound(int n, vec *loc) {
  if (nosound)
    return;
  if (!soundvol)
    return;
  if (editmode)
    return;
  if (lastmillis == lastsoundmillis)
    soundsatonce++;
  else
    soundsatonce = 1;
  lastsoundmillis = lastmillis;
  if (soundsatonce > 5)
    return;
  if (n < 0 || n >= samples.length()) {
    if (n == 22018) {
      if (!samples[53]) {
        sprintf_sd(buf)("packages/sounds/free/punch2.wav");
#ifdef USE_MIXER
        samples[53] = Mix_LoadWAV(path(buf));
#else
        samples[53] = FSOUND_Sample_Load(53, path(buf), FSOUND_LOOP_OFF, 0, 0);
#endif
      };
      if (samples[53]) {
#ifdef USE_MIXER
        Mix_PlayChannel(-1, samples[53], 0);
#else
        FSOUND_PlaySoundEx(FSOUND_FREE, samples[53], NULL, true);
#endif
      };
    };
    return;
  };

  {
    int limit = soundmax[n] > 0 ? soundmax[n] : maxsamesound;
    if (limit > 0) {
      int same = 0;
      loopi(MAXCHAN) if (soundchan[i] == n) {
#ifdef USE_MIXER
        if (Mix_Playing(i))
#else
        if (FSOUND_IsPlaying(i))
#endif
          same++;
      };
      if (same >= limit)
        return;
    };
  };

  if (!samples[n]) {
    sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);

#ifdef USE_MIXER
    samples[n] = Mix_LoadWAV(path(buf));
#else
    samples[n] = FSOUND_Sample_Load(n, path(buf), FSOUND_LOOP_OFF, 0, 0);
#endif

    if (!samples[n]) {
      conoutf("Failed to load sample: %s", buf);
      return;
    };
  };

  int prio;
  if (!loc) {
    prio = 3;
  } else if (isgunshot(n)) {
    prio = 2;
  } else {
    prio = 1;
  };

#ifdef USE_MIXER
  int chan = Mix_PlayChannel(-1, samples[n], 0);
  if (chan < 0 && prio > 1) {
    int worst = 0;
    loopi(MAXCHAN) if (chanprio[i] < chanprio[worst]) worst = i;
    if (chanprio[worst] < prio) {
      stopchan(worst);
      chan = worst;
      Mix_PlayChannel(chan, samples[n], 0);
    };
  };
#else
  int chan = FSOUND_PlaySoundEx(FSOUND_FREE, samples[n], NULL, true);
#endif
  if (chan < 0)
    return;
  soundchan[chan] = n;
  chanprio[chan] = prio;
  if (loc)
    newsoundloc(chan, loc);
  updatechanvol(chan, loc);
#ifndef USE_MIXER
  FSOUND_SetPaused(chan, false);
#endif
};

int playsoundloop(int n, vec *loc) {
  if (nosound)
    return -1;
  if (!soundvol)
    return -1;
  if (editmode)
    return -1;
  if (n < 0 || n >= samples.length()) {
    conoutf("Unregistered sound: %d", n);
    if (n == 22018) {
      if (!samples[53]) {
        sprintf_sd(buf)("packages/sounds/free/punch2.wav");
#ifdef USE_MIXER
        samples[53] = Mix_LoadWAV(path(buf));
#else
        samples[53] =
            FSOUND_Sample_Load(53, path(buf), FSOUND_LOOP_NORMAL, 0, 0);
#endif
      };
      if (samples[53]) {
#ifdef USE_MIXER
        return Mix_PlayChannel(-1, samples[53], -1);
#else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, samples[53], NULL, true);
        FSOUND_SetPaused(chan, false);
        return chan;
#endif
      };
    };
    return -1;
  }
  if (!samples[n]) {
    sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);
#ifdef USE_MIXER
    samples[n] = Mix_LoadWAV(path(buf));
#else
    samples[n] = FSOUND_Sample_Load(n, path(buf), FSOUND_LOOP_NORMAL, 0, 0);
#endif
    if (!samples[n]) {
      conoutf("Failed to load sample: %s", buf);
      return -1;
    }
  }
#ifdef USE_MIXER
  int chan = Mix_PlayChannel(-1, samples[n], -1);
#else
  int chan = FSOUND_PlaySoundEx(FSOUND_FREE, samples[n], NULL, true);
#endif
  if (chan < 0)
    return -1;
  soundchan[chan] = n;
  if (loc)
    newsoundloc(chan, loc);
  updatechanvol(chan, loc);
#ifndef USE_MIXER
  FSOUND_SetPaused(chan, false);
#endif
  return chan;
};

void stopsounds() {
  if (nosound)
    return;
#ifdef USE_MIXER
  Mix_HaltChannel(-1);
#else
  loopi(MAXCHAN) stopchan(i);
#endif
  loopi(MAXCHAN) {
    soundlocs[i].inuse = false;
    soundchan[i] = -1;
    chanprio[i] = 0;
  }
}

void stopchan(int chan) {
  if (chan < 0)
    return;
#ifdef USE_MIXER
  Mix_HaltChannel(chan);
#else
  FSOUND_StopSound(chan);
#endif
  if (chan < MAXCHAN) {
    soundlocs[chan].inuse = false;
    soundchan[chan] = -1;
    chanprio[chan] = 0;
  }
};

void preloadweaponsounds() {
  if (nosound)
    return;
  int ids[] = {S_CSAW,  S_SG,      S_CG,       S_RLFIRE,
               S_RIFLE, S_NAILGUN, S_WEAPLOAD, S_NOAMMO};
  loopi(sizeof(ids) / sizeof(ids[0])) {
    int n = ids[i];
    if (n < 0 || n >= samples.length())
      continue;
    if (!samples[n]) {
      sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);
#ifdef USE_MIXER
      samples[n] = Mix_LoadWAV(path(buf));
#else
      samples[n] = FSOUND_Sample_Load(n, path(buf), FSOUND_LOOP_OFF, 0, 0);
#endif
      if (!samples[n]) {
        conoutf("Failed to preload sound: %s", buf);
      }
    }
  }
}

void sound(int n) { playsound(n, NULL); };
COMMAND(sound, ARG_1INT);
