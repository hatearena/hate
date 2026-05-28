// editing.cpp: most map editing commands go here, entity editing commands are in world.cpp

#include "cube.h"

bool editmode = false;

// the current selection, used by almost all editing commands
// invariant: all code assumes that these are kept inside MINBORD distance of the edge of the map
block sel =
{
	variable("selx",  0, 0, 4096, &sel.x,  NULL, false),
	variable("sely",  0, 0, 4096, &sel.y,  NULL, false),
	variable("selxs", 0, 0, 4096, &sel.xs, NULL, false),
	variable("selys", 0, 0, 4096, &sel.ys, NULL, false),
};

#define loopselxy(b) { makeundo(); loop(x,sel.xs) loop(y,sel.ys) { sqr *s = S(sel.x+x, sel.y+y); b; }; remip(sel); }

int cx, cy, ch;

int curedittex[] = { -1, -1, -1 };

vector<block *> undos;                                  // unlimited undo
VARP(undomegs, 0, 1, 10);                                // bounded by n megs

void makeundo()
{
	undos.add(blockcopy(sel));
	int t = 0;
	loopvrev(undos)
	{
		t += undos[i]->xs*undos[i]->ys*sizeof(sqr);
		if(t>(undomegs<<20)) free(undos.remove(i));
	};
};

void tofronttex()                                       // maintain most recently used of the texture lists when applying texture
{
	loopi(3)
	{
		int c = curedittex[i];
		if(c>=0)
		{
			uchar *p = hdr.texlists[i];
			int t = p[c];
			for(int a = c-1; a>=0; a--) p[a+1] = p[a];
			p[0] = t;
			curedittex[i] = -1;
		};
	};
};

// the core editing function. all the *xy functions perform the core operations
// and are also called directly from the network, the function below it is strictly
// triggered locally. They all have very similar structure.

void editheightxy(bool isfloor, int amount, block &sel)
{
	loopselxy(if(isfloor)
			{
			s->floor += amount;
			if(s->floor>=s->ceil) s->floor = s->ceil-1;
			}
			else
			{
			s->ceil += amount;
			if(s->ceil<=s->floor) s->ceil = s->floor+1;
			});
};

void edittypexy(int type, block &sel)
{
	loopselxy(s->type = type);
};

const int MAXARCHVERT = 50;
int archverts[MAXARCHVERT][MAXARCHVERT];
bool archvinit = false;

void archvertex(int span, int vert, int delta)
{
	if(!archvinit)
	{
		archvinit = true;
		loop(s,MAXARCHVERT) loop(v,MAXARCHVERT) archverts[s][v] = 0;
	};
	if(span>=MAXARCHVERT || vert>=MAXARCHVERT || span<0 || vert<0) return;
	archverts[span][vert] = delta;
};

VARF(fullbright, 0, 0, 1,
		if(fullbright)
		{
		return;
		};
	);

	COMMAND(archvertex, ARG_3INT);

