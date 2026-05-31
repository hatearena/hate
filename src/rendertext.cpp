// rendertext.cpp: TTF font rendering

#include "cube.h"

static TTF_Font *font = NULL;
static bool font_inited = false;

struct glyph
{
    GLuint texid;
    short w, h;
    short advance;
    short minx, miny;
};
static glyph glyphs[95];

void init_font()
{
    font = TTF_OpenFont(path(newstring("data/font.ttf")), FONTH);
    if (!font) fatal("could not load font data/font.ttf");

    for (int i = 0; i < 95; i++)
    {
        Uint16 ch = (Uint16)(unsigned char)(i + 33);
        int minx, maxx, miny, maxy, advance;
        TTF_GlyphMetrics(font, ch, &minx, &maxx, &miny, &maxy, &advance);
        glyphs[i].advance = advance;
        glyphs[i].minx = minx;
        glyphs[i].miny = miny;

        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *s = TTF_RenderGlyph_Blended(font, ch, white);
        if (!s)
        {
            glyphs[i].texid = 0;
            glyphs[i].w = 0;
            glyphs[i].h = 0;
            continue;
        }

        int sw = s->w, sh = s->h;
        int src_pitch_pix = s->pitch / 4;

        SDL_Surface *rgb = SDL_CreateRGBSurface(0, sw, sh, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
        if (!rgb) fatal("could not create glyph surface");

        SDL_LockSurface(s);
        SDL_LockSurface(rgb);

        for (int gy = 0; gy < sh; gy++)
        {
            for (int gx = 0; gx < sw; gx++)
            {
                Uint32 pixel = ((Uint32 *)s->pixels)[gy * src_pitch_pix + gx];
                Uint8 r, g, b, a;
                SDL_GetRGBA(pixel, s->format, &r, &g, &b, &a);
                Uint8 val = a;
                Uint8 *dst = (Uint8 *)rgb->pixels + gy * rgb->pitch + gx * 3;
                dst[0] = val;
                dst[1] = val;
                dst[2] = val;
            }
        }

        SDL_UnlockSurface(rgb);
        SDL_UnlockSurface(s);
        SDL_FreeSurface(s);

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, sw, sh, GL_RGB, GL_UNSIGNED_BYTE, rgb->pixels);
        SDL_FreeSurface(rgb);

        glyphs[i].texid = tex;
        glyphs[i].w = sw;
        glyphs[i].h = sh;
    }

    font_inited = true;
}

int text_width(char *str)
{
    int x = 0;
    for (int i = 0; str[i] != 0; i++)
    {
        int c = (unsigned char)str[i];
        if (c == '\t') { x = (x + PIXELTAB) / PIXELTAB * PIXELTAB; continue; }
        if (c == '\f') continue;
        if (c == ' ') { x += FONTH / 2; continue; }
        c -= 33;
        if (c < 0 || c >= 95 || !font_inited) continue;
        x += glyphs[c].advance;
    }
    return x;
}

void draw_textf(char *fstr, int left, int top, int gl_num, ...)
{
    sprintf_sdlv(str, gl_num, fstr);
    draw_text(str, left, top, gl_num);
}

void draw_text(char *str, int left, int top, int gl_num)
{
    (void)gl_num;
    if (!font_inited) return;
    glBlendFunc(GL_ONE, GL_ONE);
    glColor3ub(255, 255, 255);

    int x = left;
    int y = top;

    for (int i = 0; str[i] != 0; i++)
    {
        int c = (unsigned char)str[i];
        if (c == '\t') { x = (x - left + PIXELTAB) / PIXELTAB * PIXELTAB + left; continue; }
        if (c == '\f') { glColor3ub(64, 255, 128); continue; }
        if (c == ' ') { x += FONTH / 2; continue; }
        c -= 33;
        if (c < 0 || c >= 95) continue;
        if (!glyphs[c].texid) continue;

        glBindTexture(GL_TEXTURE_2D, glyphs[c].texid);

        int dx = x + glyphs[c].minx;
        int dy = y;
        int dw = glyphs[c].w;
        int dh = glyphs[c].h;

        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(dx, dy);
        glTexCoord2f(1, 0); glVertex2i(dx + dw, dy);
        glTexCoord2f(1, 1); glVertex2i(dx + dw, dy + dh);
        glTexCoord2f(0, 1); glVertex2i(dx, dy + dh);
        glEnd();

        xtraverts += 4;
        x += glyphs[c].advance;
    }
}

// also Don's code, so goes in here too :)

void draw_envbox_aux(float s0, float t0, int x0, int y0, int z0,
        float s1, float t1, int x1, int y1, int z1,
        float s2, float t2, int x2, int y2, int z2,
        float s3, float t3, int x3, int y3, int z3,
        int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(s3, t3); glVertex3d(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3d(x2, y2, z2);
    glTexCoord2f(s1, t1); glVertex3d(x1, y1, z1);
    glTexCoord2f(s0, t0); glVertex3d(x0, y0, z0);
    glEnd();
    xtraverts += 4;
}

void draw_envbox(int t, int w)
{
    glDepthMask(GL_FALSE);

    draw_envbox_aux(1.0f, 1.0f, -w, -w,  w,
            0.0f, 1.0f,  w, -w,  w,
            0.0f, 0.0f,  w, -w, -w,
            1.0f, 0.0f, -w, -w, -w, t);

    draw_envbox_aux(1.0f, 1.0f, +w,  w,  w,
            0.0f, 1.0f, -w,  w,  w,
            0.0f, 0.0f, -w,  w, -w,
            1.0f, 0.0f, +w,  w, -w, t+1);

    draw_envbox_aux(0.0f, 0.0f, -w, -w, -w,
            1.0f, 0.0f, -w,  w, -w,
            1.0f, 1.0f, -w,  w,  w,
            0.0f, 1.0f, -w, -w,  w, t+2);

    draw_envbox_aux(1.0f, 1.0f, +w, -w,  w,
            0.0f, 1.0f, +w,  w,  w,
            0.0f, 0.0f, +w,  w, -w,
            1.0f, 0.0f, +w, -w, -w, t+3);

    draw_envbox_aux(0.0f, 1.0f, -w,  w,  w,
            0.0f, 0.0f, +w,  w,  w,
            1.0f, 0.0f, +w, -w,  w,
            1.0f, 1.0f, -w, -w,  w, t+4);

    draw_envbox_aux(0.0f, 1.0f, +w,  w, -w,
            0.0f, 0.0f, -w,  w, -w,
            1.0f, 0.0f, -w, -w, -w,
            1.0f, 1.0f, +w, -w, -w, t+5);

    glDepthMask(GL_TRUE);
}
