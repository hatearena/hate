CXX=g++
CXXOPTFLAGS=-Wall -fsigned-char -fomit-frame-pointer -fpermissive -w \
			-Wno-Write-strings -Wno-Unused-variable -g

CXXFLAGS=$(CXXOPTFLAGS) `sdl2-config --cflags`

CLIENT_LIBS=-lenet `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lz -lGL -lGLU -lX11
CLIENT_OBJS= client.o clientextras.o clientgame.o clients2c.o command.o \
			 console.o editing.o entities.o main.o menus.o monster.o physics.o \
			 bot.o rendercubes.o renderextras.o rendergl.o rendermd2.o \
			 renderparticles.o rendertext.o rndmap.o savegamedemo.o server.o \
			 serverbrowser.o serverms.o serverutil.o sound.o tools.o weapon.o \
			 world.o worldio.o worldlight.o worldocull.o worldrender.o

SERVER_LIBS=-lenet
SERVER_OBJS= standalone.o serverms.o server.o tools.o

default: all

all: client server
	cp cube_client ../content && \
		cd ../content && \
		./cube_client

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) cube_server cube_client

standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -o standalone.o -c serverutil.cpp

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o cube_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o cube_server $(SERVER_OBJS) $(SERVER_LIBS)

