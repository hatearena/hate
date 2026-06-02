UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(OS),Windows_NT)
    PLATFORM := windows
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
else
    PLATFORM := linux
endif

ifeq ($(PLATFORM),windows)

	CXX := g++

	MSYS2_PREFIX := $(shell cygpath -u "$$MINGW_PREFIX" 2>/dev/null)

	ifeq ($(MSYS2_PREFIX),)
	    $(error MSYS2 MinGW64 environment not detected. Run this inside mingw64.exe)
	endif

	MSYS2_PREFIX_WIN := $(shell cygpath -m "$$MINGW_PREFIX")

	CLIENT_BIN = hate_client.exe
	SERVER_BIN = hate_server.exe

	CXXFLAGS_EXTRA = \
	    -I$(MSYS2_PREFIX_WIN)/include \
	    -I$(MSYS2_PREFIX_WIN)/include/SDL2

	CLIENT_LIBS = \
		-mconsole \
	    -L$(MSYS2_PREFIX_WIN)/lib \
	    -lSDL2 \
	    -lSDL2_image \
	    -lSDL2_mixer \
	    -lSDL2_ttf \
	    -lenet \
	    -lz \
	    -lopengl32 \
	    -lglu32 \
	    -lws2_32 \
	    -lwinmm

	SERVER_LIBS = \
	    -L$(MSYS2_PREFIX_WIN)/lib \
	    -mconsole \
	    -lenet \
	    -lws2_32

else
    ifeq ($(PLATFORM),macos)
        CXX = clang++

        BREW_PREFIX := $(shell brew --prefix)

        CLIENT_BIN = hate_client
        SERVER_BIN = hate_server

        CXXFLAGS_EXTRA = \
            -I$(BREW_PREFIX)/include

        SDL_CFLAGS := $(shell sdl2-config --cflags)
        SDL_LIBS := $(shell sdl2-config --libs)

        CLIENT_LIBS = \
            -L$(BREW_PREFIX)/lib \
            -lenet \
            -lz \
            $(SDL_LIBS) \
            -lSDL2_image \
            -lSDL2_mixer \
            -lSDL2_ttf \
            -framework OpenGL \
            -framework Cocoa \
            -framework IOKit \
            -framework CoreVideo

        SERVER_LIBS = \
            -L$(BREW_PREFIX)/lib \
            -lenet
    else

        CXX = g++

        CLIENT_BIN = hate_client
        SERVER_BIN = hate_server

        SDL_CFLAGS := $(shell sdl2-config --cflags)
        SDL_LIBS := $(shell sdl2-config --libs)

        CXXFLAGS_EXTRA =

        CLIENT_LIBS = \
            -lenet \
            $(SDL_LIBS) \
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
endif

CXXOPTFLAGS = \
    -Wall \
    -fsigned-char \
    -fomit-frame-pointer \
    -fpermissive \
    -w \
    -Wno-write-strings \
    -Wno-unused-variable \
    -g

CXXFLAGS = \
    $(CXXOPTFLAGS) \
    $(CXXFLAGS_EXTRA) \
    $(SDL_CFLAGS) \
    -Iinclude

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
ifeq ($(PLATFORM),windows)
	cp $(CLIENT_BIN) ../content
	cp $(SERVER_BIN) ../content
	cd ../content && ./$(CLIENT_BIN)
else
	cp $(CLIENT_BIN) ../content
	cp $(SERVER_BIN) ../content
	cd ../content && ./$(CLIENT_BIN)
endif

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_BIN) $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_OBJS) $(SERVER_LIBS)

standalone.o:
	$(CXX) $(CXXFLAGS) \
		-DSTANDALONE \
        -DSDL_MAIN_HANDLED \
		-Iinclude \
		-c server/serverutil.cpp \
		-o standalone.o

clean:
	-rm -f $(SERVER_OBJS)
	-rm -f $(CLIENT_OBJS)
	-rm -f $(CLIENT_BIN)
	-rm -f $(SERVER_BIN)
