PLATFORM ?= $(shell uname -s | tr '[:upper:]' '[:lower:]' | sed 's/darwin/macos/')

ifeq ($(PLATFORM),macos)
    CXX = clang++

    BREW_PREFIX := $(shell brew --prefix)

    CXXFLAGS_EXTRA = \
        -I$(BREW_PREFIX)/include

    LDFLAGS_EXTRA = \
        -L$(BREW_PREFIX)/lib

    CLIENT_LIBS = \
        $(LDFLAGS_EXTRA) \
        -lenet \
        -lz \
        `sdl2-config --libs` \
        -lSDL2_image -lSDL2_mixer -lSDL2_ttf \
        -framework OpenGL \
        -framework Cocoa \
        -framework IOKit \
        -framework CoreVideo

    SERVER_LIBS = \
        $(LDFLAGS_EXTRA) \
        -lenet
else
    CXX = g++

    CXXFLAGS_EXTRA =

    CLIENT_LIBS = \
        -lenet \
        `sdl2-config --libs` \
        -lSDL2_image \
        -lSDL2_mixer \
        -lSDL2_ttf \
        -lz \
        -lGL \
        -lGLU \
        -lX11

    SERVER_LIBS = \
        -lenet
endif

CXXOPTFLAGS = \
    -Wall \
    -fsigned-char \
    -fomit-frame-pointer \
    -fpermissive \
    -w \
    -Wno-Write-strings \
    -Wno-Unused-variable \
    -g

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

CXXFLAGS = \
    $(CXXOPTFLAGS) \
    $(CXXFLAGS_EXTRA) \
    -Iinclude \
    `sdl2-config --cflags`

CLIENT_OBJS = \
    client/client.o \
    client/clientextras.o \
    client/clientgame.o \
    client/clients2c.o \
    client/menus.o \
    engine/command.o \
    engine/console.o \
    engine/editing.o \
    game/entities.o \
    main.o \
    game/monster.o \
    engine/physics.o \
    game/bot.o \
    engine/rendercubes.o \
    engine/renderextras.o \
    engine/rendergl.o \
    engine/rendermd2.o \
    engine/rendermd3.o \
    engine/renderparticles.o \
    engine/rendertext.o \
    engine/rndmap.o \
    engine/savegamedemo.o \
    server/server.o \
    engine/serverbrowser.o \
    server/serverms.o \
    server/serverutil.o \
    engine/sound.o \
    engine/tools.o \
    game/weapon.o \
    engine/world.o \
    engine/worldio.o \
    engine/worldlight.o \
    engine/worldocull.o \
    engine/worldrender.o


SERVER_OBJS = \
    standalone.o \
    server/serverms.o \
    server/server.o \
    engine/tools.o

default: all

all: client server
	cp hate_client ../content && \
	cd ../content && \
	./hate_client

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) hate_server hate_client

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o hate_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o hate_server $(SERVER_OBJS) $(SERVER_LIBS)

standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -Iinclude \
		-c server/serverutil.cpp -o standalone.o