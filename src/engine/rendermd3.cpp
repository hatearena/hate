#include "../include/cube.h"

struct md3_header {
  char fileID[4];
  int version;
  char strFile[68];
  int numFrames;
  int numTags;
  int numMeshes;
  int numMaxSkins;
  int headerSize;
  int tagStart;
  int tagEnd;
  int fileSize;
};

struct md3_meshinfo {
  char meshID[4];
  char strName[68];
  int numMeshFrames;
  int numSkins;
  int numVertices;
  int numTriangles;
  int triStart;
  int headerSize;
  int uvStart;
  int vertexStart;
  int meshSize;
};

struct md3_triangle {
  signed short vertex[3];
  unsigned char normal[2];
};

struct md3_face {
  int vertexIndices[3];
};

struct md3_texcoord {
  float textureCoord[2];
};

struct md3_mesh {
  char name[68];
  int numVerts;
  int numFaces;
  vec *verts;
  vec *texcoords;
  md3_face *faces;
};

struct md3 {
  int numFrames;
  int numMeshes;
  md3_mesh *meshes;
  vec **mverts;
  int mdlnum;
  char *loadname;
  bool loaded;

  bool load(char *filename);
  void render(vec &light, int numFrame, int range, float x, float y, float z,
              float yaw, float pitch, float scale, float speed, int snap,
              int basetime, float alpha = 1.0f);
  void scale(int mesh, int frame, float s);
  md3()
      : numFrames(0), numMeshes(0), meshes(NULL), mverts(NULL), mdlnum(0),
        loadname(NULL), loaded(false) {}
  ~md3() {
    if (meshes) {
      loopi(numMeshes) {
        if (meshes[i].verts)
          delete[] meshes[i].verts;
        if (meshes[i].texcoords)
          delete[] meshes[i].texcoords;
        if (meshes[i].faces)
          delete[] meshes[i].faces;
      }
      delete[] meshes;
    }
    if (mverts) {
      loopi(numMeshes) {
        loopj(numFrames) if (mverts[i * numFrames +
                                    j]) delete[] mverts[i * numFrames + j];
      }
      delete[] mverts;
    }
  }
};

struct md3_bone {
  float mins[3];
  float maxs[3];
  float position[3];
  float scale;
  char creator[16];
};

struct md3_tag {
  char strName[64];
  float vPosition[3];
  float rotation[3][3];
};

bool md3::load(char *filename) {
  FILE *file;

  file = fopen(filename, "rb");
  if (!file)
    return false;

  md3_header header;
  fread(&header, sizeof(md3_header), 1, file);
  if (header.fileID[0] != 'I' || header.fileID[1] != 'D' ||
      header.fileID[2] != 'P' || header.fileID[3] != '3' ||
      header.version != 15) {
    fclose(file);
    return false;
  }

  numFrames = header.numFrames;
  numMeshes = header.numMeshes;

  long meshOffset = sizeof(md3_header) + numFrames * (long)56 +
                    numFrames * header.numTags * (long)112;
  fseek(file, meshOffset, SEEK_SET);

  meshes = new md3_mesh[numMeshes];
  memset(meshes, 0, sizeof(md3_mesh) * numMeshes);

  long base = meshOffset;
  loopi(numMeshes) {
    md3_meshinfo meshHeader;
    fseek(file, base, SEEK_SET);
    fread(&meshHeader, sizeof(md3_meshinfo), 1, file);

    int nv = meshHeader.numVertices;
    int nt = meshHeader.numTriangles;
    int nf = meshHeader.numMeshFrames;

    if (nv > 0 && nt > 0 && nf > 0) {
      meshes[i].numVerts = nv;
      meshes[i].numFaces = nt;
      strcpy(meshes[i].name, meshHeader.strName);

      meshes[i].verts = new vec[nv * nf];
      meshes[i].texcoords = new vec[nv];
      meshes[i].faces = new md3_face[nt];

      fseek(file, base + meshHeader.uvStart, SEEK_SET);
      loopj(nv) {
        float u, v;
        fread(&u, 4, 1, file);
        fread(&v, 4, 1, file);
        meshes[i].texcoords[j].x = u;
        meshes[i].texcoords[j].y = v;
      }

      fseek(file, base + meshHeader.triStart, SEEK_SET);
      loopj(nt) { fread(meshes[i].faces + j, 12, 1, file); }

      fseek(file, base + meshHeader.vertexStart, SEEK_SET);
      loopj(nv * nf) {
        signed short vx, vy, vz;
        unsigned char nx, ny;
        fread(&vx, 2, 1, file);
        fread(&vy, 2, 1, file);
        fread(&vz, 2, 1, file);
        fread(&nx, 1, 1, file);
        fread(&ny, 1, 1, file);
        meshes[i].verts[j].x = vx / 64.0f;
        meshes[i].verts[j].y = vy / 64.0f;
        meshes[i].verts[j].z = vz / 64.0f;
      }
    }
    base += meshHeader.meshSize;
  }

  fclose(file);

  mverts = new vec *[numMeshes * numFrames];
  loopi(numMeshes * numFrames) mverts[i] = NULL;

  return true;
}

void md3::scale(int mesh, int frame, float s) {
  int idx = mesh * numFrames + frame;
  if (mverts[idx])
    delete[] mverts[idx];
  mverts[idx] = new vec[meshes[mesh].numVerts];
  float sc = 16.0f / s;
  vec *src = meshes[mesh].verts + frame * meshes[mesh].numVerts;
  vec *dst = mverts[idx];
  loopi(meshes[mesh].numVerts) {
    dst[i].x = src[i].x / sc;
    dst[i].y = src[i].y / sc;
    dst[i].z = src[i].z / sc;
  }
}

void md3::render(vec &light, int frame, int range, float x, float y, float z,
                 float yaw, float pitch, float sc, float speed, int snap,
                 int basetime, float alpha) {
  if (speed <= 0.0f)
    speed = 100.0f;
  if (frame < 0)
    frame = 0;
  if (frame >= numFrames)
    frame = numFrames - 1;
  if (range < 1)
    range = 1;
  if (frame + range > numFrames)
    range = numFrames - frame;

  int time = lastmillis - basetime;
  int fr1 = (int)(time / speed);
  float frac1 = (time - fr1 * speed) / speed;
  float frac2 = 1.0f - frac1;
  fr1 = ((fr1 % range) + range) % range + frame;
  int fr2 = fr1 + 1;
  if (fr2 >= frame + range)
    fr2 = frame;

  if (fr1 < 0 || fr1 >= numFrames)
    fr1 = frame;
  if (fr2 < 0 || fr2 >= numFrames)
    fr2 = frame;

  glPushMatrix();
  glTranslatef(x, y, z);
  glRotatef(yaw + 180, 0, -1, 0);
  glRotatef(pitch, 0, 0, 1);
  if (alpha < 1.0f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(light.x, light.y, light.z, alpha);
  } else
    glColor3fv((float *)&light);

  loop(mi, numMeshes) {
    md3_mesh *m = &meshes[mi];
    if (m->numVerts <= 0 || m->numFaces <= 0)
      continue;

    int idx1 = mi * numFrames + fr1;
    int idx2 = mi * numFrames + fr2;

    if (!mverts[idx1])
      scale(mi, fr1, sc);
    if (!mverts[idx2])
      scale(mi, fr2, sc);
    if (!mverts[idx1] || !mverts[idx2])
      continue;

    vec *verts1 = mverts[idx1];
    vec *verts2 = mverts[idx2];

    glBegin(GL_TRIANGLES);
    loopj(m->numFaces) {
      loopk(3) {
        int vi = m->faces[j].vertexIndices[k];
        if (vi < 0 || vi >= m->numVerts)
          continue;
        glTexCoord2f(m->texcoords[vi].x, m->texcoords[vi].y);
        vec &v1 = verts1[vi];
        vec &v2 = verts2[vi];
        glVertex3f(v1.x * frac2 + v2.x * frac1, v1.z * frac2 + v2.z * frac1,
                   v1.y * frac2 + v2.y * frac1);
      }
    }
    glEnd();
    xtraverts += m->numFaces * 3;
  }

  if (alpha < 1.0f)
    glDisable(GL_BLEND);

  glPopMatrix();
}

hashtable<md3 *> *md3lookup = NULL;

md3 *loadmodel_md3(char *name) {
  if (!md3lookup)
    md3lookup = new hashtable<md3 *>;
  md3 **mm = md3lookup->access(name);
  if (mm)
    return *mm;
  md3 *m = new md3();
  m->mdlnum = -1;
  m->loadname = newstring(name);
  md3lookup->access(m->loadname, &m);
  return m;
}

int md3tex(const char *name) {
  static hashtable<int> *texmap = NULL;
  if (!texmap)
    texmap = new hashtable<int>;
  int *t = texmap->access((char *)name);
  if (t)
    return *t;
  int tnum;
  glGenTextures(1, (GLuint *)&tnum);
  sprintf_sd(jpg)("packages/models/%s/skin.jpg", name);
  int xs, ys;
  if (!installtex(tnum, path(jpg), xs, ys)) {
    sprintf_sd(tga)("packages/models/%s/skin.tga", name);
    installtex(tnum, path(tga), xs, ys);
  }
  texmap->access((char *)name, &tnum);
  return tnum;
}

void delayedload_md3(md3 *m) {
  if (!m->loaded) {
    sprintf_sd(name1)("packages/models/%s/tris.MD3", m->loadname);
    if (!m->load(path(name1))) {
      sprintf_sd(name2)("packages/models/%s/tris.md3", m->loadname);
      if (!m->load(path(name2)))
        fatal("loadmodel_md3: ", name1);
    }
    m->loaded = true;
  }
}

void rendermodel_md3(char *mdl, int frame, int range, int tex, float rad,
                     float x, float y, float z, float yaw, float pitch,
                     bool teammate, float scale, float speed, int snap,
                     int basetime, float alpha) {
  md3 *m = loadmodel_md3(mdl);
  if (isoccluded(player1->o.x, player1->o.y, x - rad, z - rad, rad * 2))
    return;
  delayedload_md3(m);

  glBindTexture(GL_TEXTURE_2D, tex ? tex : md3tex(m->loadname));

  int ix = (int)x;
  int iy = (int)z;
  vec light = {1.0f, 1.0f, 1.0f};
  if (!OUTBORD(ix, iy)) {
    sqr *s = S(ix, iy);
    float ll = 256.0f;
    float of = 0.0f;
    light.x = s->r / ll + of;
    light.y = s->g / ll + of;
    light.z = s->b / ll + of;
  }
  if (teammate) {
    light.x *= 0.6f;
    light.y *= 0.7f;
    light.z *= 1.2f;
  }
  m->render(light, frame, range, x, y, z, yaw, pitch, scale, speed, snap,
            basetime, alpha);
}

void preloadhudmodel_md3() {}
