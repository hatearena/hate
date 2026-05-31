// rendertext.cpp: TTF font rendering

#include "cube.h"

static TTF_Font *font = NULL;
static bool font_inited = false;

void init_font()
{
    font = TTF_OpenFont(path(newstring("data/font.ttf")), FONTH);
    if (!font) fatal("could not load font data/font.ttf");
    font_inited = true;
}

static void render_surface(SDL_Surface *s, int x, int y)
{
    if (!s) return;
    int w = s->w, h = s->h;
    int spitch = s->pitch / 4;

    SDL_Surface *rgb = SDL_CreateRGBSurface(0, w, h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!rgb) { SDL_FreeSurface(s); return; }

    SDL_LockSurface(s);
    SDL_LockSurface(rgb);

    for (int gy = 0; gy < h; gy++)
    {
        for (int gx = 0; gx < w; gx++)
        {
            Uint32 pixel = ((Uint32 *)s->pixels)[gy * spitch + gx];
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, s->format, &r, &g, &b, &a);
            Uint8 *dst = (Uint8 *)rgb->pixels + gy * rgb->pitch + gx * 3;
            dst[0] = a;
            dst[1] = a;
            dst[2] = a;
        }
    }

    SDL_UnlockSurface(rgb);
    SDL_UnlockSurface(s);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb->pixels);
    SDL_FreeSurface(rgb);

    glBlendFunc(GL_ONE, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(x, y);
    glTexCoord2f(1, 0); glVertex2i(x + w, y);
    glTexCoord2f(1, 1); glVertex2i(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2i(x, y + h);
    glEnd();
    xtraverts += 4;

    glDeleteTextures(1, &tex);
    SDL_FreeSurface(s);
}

int text_width(char *str)
{
    if (!font_inited) return 0;
    int w;
    TTF_SizeUTF8(font, str, &w, NULL);
    return w;
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

    SDL_Color white = {255, 255, 255, 255};
    if (str[0] == '\f')
    {
        glColor3ub(64, 255, 128);
        render_surface(TTF_RenderUTF8_Blended(font, str + 1, white), left, top);
    }
    else
    {
        glColor3ub(255, 255, 255);
        render_surface(TTF_RenderUTF8_Blended(font, str, white), left, top);
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
