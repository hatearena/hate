#include "../include/cube.h"

void backup(char *name, char *backupname) {
  remove(backupname);
  rename(name, backupname);
};

string cgzname, bakname, pcfname, mcfname;

void setnames(char *name) {
  string pakname, mapname;
  char *slash = strpbrk(name, "/\\");
  if (slash) {
    strn0cpy(pakname, name, slash - name + 1);
    strcpy_s(mapname, slash + 1);
  } else {
    strcpy_s(pakname, "base");
    strcpy_s(mapname, name);
  };
  sprintf_s(cgzname)("packages/%s/%s.hmap", pakname, mapname);
  sprintf_s(bakname)("packages/%s/%s_%d.hmapbak", pakname, mapname, lastmillis);
  sprintf_s(pcfname)("packages/%s/package.cfg", pakname);
  sprintf_s(mcfname)("packages/%s/%s.cfg", pakname, mapname);

  path(cgzname);
  path(bakname);
};

inline bool nhf(sqr *s) { return s->type != FHF && s->type != CHF; };

void voptimize() {
  loop(x, ssize) loop(y, ssize) {
    sqr *s = S(x, y);
    if (x && y) {
      if (nhf(s) && nhf(S(x - 1, y)) && nhf(S(x - 1, y - 1)) &&
          nhf(S(x, y - 1)))
        s->vdelta = 0;
    } else
      s->vdelta = 0;
  };
};

void topt(sqr *s, bool &wf, bool &uf, int &wt, int &ut) {
  sqr *o[4];
  o[0] = SWS(s, 0, -1, ssize);
  o[1] = SWS(s, 0, 1, ssize);
  o[2] = SWS(s, 1, 0, ssize);
  o[3] = SWS(s, -1, 0, ssize);
  wf = true;
  uf = true;
  if (SOLID(s)) {
    loopi(4) if (!SOLID(o[i])) {
      wf = false;
      wt = s->wtex;
      ut = s->utex;
      return;
    };
  } else {
    loopi(4) if (!SOLID(o[i])) {
      float sfloor = (float)s->floor;
      float ofloor = (float)o[i]->floor;
      if (s->type == FHF && o[i]->type != FHF)
        sfloor -= s->vdelta / 4.0f;
      if (o[i]->type == FHF && s->type != FHF)
        ofloor -= o[i]->vdelta / 4.0f;
      if (ofloor < sfloor) {
        wt = s->wtex;
        wf = false;
      };
      float sceil = (float)s->ceil;
      float oceil = (float)o[i]->ceil;
      if (s->type == CHF && o[i]->type != CHF)
        sceil += s->vdelta / 4.0f;
      if (o[i]->type == CHF && s->type != CHF)
        oceil += o[i]->vdelta / 4.0f;
      if (oceil > sceil) {
        ut = s->utex;
        uf = false;
      };
    };
  };
};

void toptimize() {
  bool wf[4], uf[4];
  sqr *s[4];
  for (int x = 2; x < ssize - 4; x += 2)
    for (int y = 2; y < ssize - 4; y += 2) {
      s[0] = S(x, y);
      int wt = s[0]->wtex, ut = s[0]->utex;
      topt(s[0], wf[0], uf[0], wt, ut);
      topt(s[1] = SWS(s[0], 0, 1, ssize), wf[1], uf[1], wt, ut);
      topt(s[2] = SWS(s[0], 1, 1, ssize), wf[2], uf[2], wt, ut);
      topt(s[3] = SWS(s[0], 1, 0, ssize), wf[3], uf[3], wt, ut);
      loopi(4) {
        if (wf[i])
          s[i]->wtex = wt;
        if (uf[i])
          s[i]->utex = ut;
      };
    };
};

// these two are used by getmap/sendmap.. transfers compressed maps directly

void writemap(char *mname, int msize, uchar *mdata) {
  setnames(mname);
  backup(cgzname, bakname);
  FILE *f = fopen(cgzname, "wb");
  if (!f) {
    char *dot = strrchr(cgzname, '.');
    if (dot)
      *dot = '\0';
    conoutf("Could not write map to %s", cgzname);
    return;
  };
  fwrite(mdata, 1, msize, f);
  fclose(f);
  char *dot = strrchr(cgzname, '.');
  if (dot)
    *dot = '\0';
  conoutf("Wrote map %s as file %s", mname, cgzname + 14);
}

uchar *readmap(char *mname, int *msize) {
  if (!*mname || !strcmp(mname, "."))
    mname = getclientmap();
  setnames(mname);
  uchar *mdata = (uchar *)loadfile(cgzname, msize);
  if (!mdata) {
    conoutf("Could not read map %s", cgzname);
    return NULL;
  };
  return mdata;
}

// save map as .hmap file. uses 2 layers of compression: first does simple
// run-length encoding and leaves out data for certain kinds of cubes, then zlib
// removes the last bits of redundancy. Both passes contribute greatly to the
// miniscule map sizes.

void save_world(char *mname) {
  resettagareas();
  voptimize();
  toptimize();
  if (!*mname || !strcmp(mname, "."))
    mname = getclientmap();
  setnames(mname);
  backup(cgzname, bakname);
  gzFile f = gzopen(cgzname, "wb9");
  if (!f) {
    conoutf("Could not write map to %s", cgzname);
    return;
  };
  hdr.version = MAPVERSION;
  hdr.numents = 0;
  loopv(ents) if (ents[i].type != NOTUSED) hdr.numents++;
  header tmp = hdr;
  endianswap(&tmp.version, sizeof(int), 4);
  endianswap(&tmp.waterlevel, sizeof(int), 16);
  gzwrite(f, &tmp, sizeof(header));
  loopv(ents) {
    if (ents[i].type != NOTUSED) {
      entity tmp = ents[i];
      endianswap(&tmp, sizeof(short), 4);
      gzwrite(f, &tmp, sizeof(persistent_entity));
    };
  };
  sqr *t = NULL;
  int sc = 0;
#define spurge                                                                 \
  while (sc) {                                                                 \
    gzputc(f, 255);                                                            \
    if (sc > 255) {                                                            \
      gzputc(f, 255);                                                          \
      sc -= 255;                                                               \
    } else {                                                                   \
      gzputc(f, sc);                                                           \
      sc = 0;                                                                  \
    }                                                                          \
  };
  loopk(cubicsize) {
    sqr *s = &world[k];
#define c(f) (s->f == t->f)
    // 4 types of blocks, to compress a bit:
    // 255 (2): same as previous block + count
    // 254 (3): same as previous, except light // deprecated
    // SOLID (5)
    // anything else (9)

    if (SOLID(s)) {
      if (t && c(type) && c(wtex) && c(vdelta)) {
        sc++;
      } else {
        spurge;
        gzputc(f, s->type);
        gzwrite(f, &s->wtex, sizeof(ushort));
        gzputc(f, s->vdelta);
      };
    } else {
      if (t && c(type) && c(floor) && c(ceil) && c(ctex) && c(ftex) &&
          c(utex) && c(wtex) && c(vdelta) && c(tag)) {
        sc++;
      } else {
        spurge;
        gzputc(f, s->type);
        gzputc(f, s->floor);
        gzputc(f, s->ceil);
        gzwrite(f, &s->wtex, sizeof(ushort));
        gzwrite(f, &s->ftex, sizeof(ushort));
        gzwrite(f, &s->ctex, sizeof(ushort));
        gzputc(f, s->vdelta);
        gzwrite(f, &s->utex, sizeof(ushort));
        gzputc(f, s->tag);
      };
    };
    t = s;
  };
  spurge;
  gzclose(f);
  char *dot = strrchr(cgzname, '.');
  if (dot)
    *dot = '\0';
  conoutf("Wrote map file %s", cgzname + 14);
  settagareas();
};

void load_world(char *mname) {
  if (!*mname || !strcmp(mname, "."))
    mname = getclientmap();
  stopifrecording();
  cleardlights();
  setnames(mname);
  gzFile f = gzopen(cgzname, "rb9");
  if (!f) {
    conoutf("Could not read map %s", cgzname);
    return;
  };
  gzread(f, &hdr, (char *)&hdr.texlists - (char *)&hdr);
  endianswap(&hdr.version, sizeof(int), 4);
  if (strncmp(hdr.head, "CUBE", 4) != 0)
    fatal("While reading map: header malformatted");
  if (hdr.version > MAPVERSION)
    fatal("This map requires a newer version of HATE.");
  if (hdr.version >= 6) {
    gzread(f, hdr.texlists, sizeof(ushort) * 3 * 2048);
  } else {
    uchar oldtex[3][256];
    gzread(f, oldtex, 3 * 256);
    loopk(3) {
      loopi(256) hdr.texlists[k][i] = oldtex[k][i];
      for (int i = 256; i < 2048; i++)
        hdr.texlists[k][i] = i;
    };
  };
  if (sfactor < SMALLEST_FACTOR || sfactor > LARGEST_FACTOR)
    fatal("Illegal map size");
  if (hdr.version >= 4) {
    gzread(f, &hdr.waterlevel, sizeof(int) * 16);
    endianswap(&hdr.waterlevel, sizeof(int), 16);
  } else {
    hdr.waterlevel = -100000;
  };
  ents.setsize(0);
  loopi(hdr.numents) {
    entity &e = ents.add();
    gzread(f, &e, sizeof(persistent_entity));
    endianswap(&e, sizeof(short), 4);
    e.spawned = false;
    if (e.type == LIGHT) {
      if (!e.attr2)
        e.attr2 = 255; // needed for MAPVERSION<=2
      if (e.attr1 > 32)
        e.attr1 = 32; // 12_03 and below
    };
  };
  free(world);
  setupworld(hdr.sfactor);
  char texuse[2048];
  loopi(2048) texuse[i] = 0;
  sqr *t = NULL;
  loopk(cubicsize) {
    sqr *s = &world[k];
    int type = gzgetc(f);
    switch (type) {
    case 255: {
      int n = gzgetc(f);
      for (int i = 0; i < n; i++, k++)
        memcpy(&world[k], t, sizeof(sqr));
      k--;
      break;
    };
    case 254: // only in MAPVERSION<=2
    {
      memcpy(s, t, sizeof(sqr));
      s->r = s->g = s->b = gzgetc(f);
      gzgetc(f);
      break;
    };
    case SOLID: {
      s->type = SOLID;
      if (hdr.version >= 6)
        gzread(f, &s->wtex, sizeof(ushort));
      else
        s->wtex = gzgetc(f);
      s->vdelta = gzgetc(f);
      if (hdr.version <= 2) {
        gzgetc(f);
        gzgetc(f);
      };
      s->ftex = DEFAULT_FLOOR;
      s->ctex = DEFAULT_CEIL;
      s->utex = s->wtex;
      s->tag = 0;
      s->floor = 0;
      s->ceil = 16;
      break;
    };
    default: {
      if (type < 0 || type >= MAXTYPE) {
        sprintf_sd(t)("%d @ %d", type, k);
        fatal("Error while reading map: type out of range: ", t);
      };
      s->type = type;
      s->floor = gzgetc(f);
      s->ceil = gzgetc(f);
      if (s->floor >= s->ceil)
        s->floor = s->ceil - 1;
      if (hdr.version >= 6) {
        gzread(f, &s->wtex, sizeof(ushort));
        gzread(f, &s->ftex, sizeof(ushort));
        gzread(f, &s->ctex, sizeof(ushort));
      } else {
        s->wtex = gzgetc(f);
        s->ftex = gzgetc(f);
        s->ctex = gzgetc(f);
      };
      if (hdr.version <= 2) {
        gzgetc(f);
        gzgetc(f);
      };
      s->vdelta = gzgetc(f);
      if (hdr.version >= 6) {
        gzread(f, &s->utex, sizeof(ushort));
      } else {
        s->utex = (hdr.version >= 2) ? gzgetc(f) : s->wtex;
      };
      s->tag = (hdr.version >= 5) ? gzgetc(f) : 0;
      s->type = type;
    };
    };
    s->defer = 0;
    t = s;
    texuse[s->wtex] = 1;
    if (!SOLID(s))
      texuse[s->utex] = texuse[s->ftex] = texuse[s->ctex] = 1;
  };
  gzclose(f);
  calclight();
  settagareas();
  int xs, ys;
  loopi(2048) if (texuse[i]) lookuptexture(i, xs, ys);
  char *dot = strrchr(cgzname, '.');
  if (dot)
    *dot = '\0';
  char *cgzdisplayname = (char *)"";
  if (strlen(cgzname) > 14) {
    cgzdisplayname = cgzname + 14;
  }
  conoutf("Read map %s in %d milliseconds.", cgzdisplayname,
          SDL_GetTicks() - lastmillis);
  if (dot)
    *dot = '.';
  conoutf("%s", hdr.maptitle);
  startmap(mname);
  loopl(256) {
    sprintf_sd(aliasname)("level_trigger_%d", l);
    if (identexists(aliasname))
      alias(aliasname, "");
  };
  execfile("data/default_map_settings.cfg");
  execfile(pcfname);
  execfile(mcfname);
};

COMMANDN(savemap, save_world, ARG_1STR);
static int __ad_savemap =
    (addcommanddetail("savemap", "Saves the current map to a file"), 0);
