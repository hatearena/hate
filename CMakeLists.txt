cmake_minimum_required(VERSION 3.10)
project(hate VERSION 0.0.2 LANGUAGES CXX)

if(WIN32)
  set(CLIENT_BIN "hate_client.exe")
  set(SERVER_BIN "hate_server.exe")
else()
  set(CLIENT_BIN "hate_client")
  set(SERVER_BIN "hate_server")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fsigned-char -fomit-frame-pointer -fpermissive -w -Wno-write-strings -Wno-unused-variable -g")
set(INCLUDE_DIRS src/include)

find_package(ZLIB REQUIRED)

if(WIN32)
  find_library(ENET_LIBRARY enet REQUIRED)
  find_library(SDL2_LIBRARY SDL2 REQUIRED)
  find_library(SDL2_IMAGE_LIBRARY SDL2_image REQUIRED)
  find_library(SDL2_TTF_LIBRARY SDL2_ttf REQUIRED)
  find_library(SDL2_MIXER_LIBRARY SDL2_mixer REQUIRED)
  find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
  find_package(OpenGL REQUIRED)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(ENET libenet)
    pkg_check_modules(SDL2 sdl2)
    pkg_check_modules(SDL2_IMAGE SDL2_image)
    pkg_check_modules(SDL2_TTF SDL2_ttf)
    pkg_check_modules(SDL2_MIXER SDL2_mixer)
  endif()
  if(NOT ENET_FOUND)
    find_path(ENET_INCLUDE_DIR enet/enet.h)
    find_library(ENET_LIBRARY enet)
    if(NOT ENET_LIBRARY)
      message(FATAL_ERROR "enet not found")
    endif()
  endif()
  if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
  else()
    find_package(OpenGL REQUIRED)
  endif()
endif()

set(ENGINE_SOURCES
  src/engine/command.cpp
  src/engine/console.cpp
  src/engine/editing.cpp
  src/engine/physics.cpp
  src/engine/rendercubes.cpp
  src/engine/renderextras.cpp
  src/engine/rendergl.cpp
  src/engine/rendermd2.cpp
  src/engine/rendermd3.cpp
  src/engine/renderparticles.cpp
  src/engine/rendertext.cpp
  src/engine/rndmap.cpp
  src/engine/savegamedemo.cpp
  src/engine/serverbrowser.cpp
  src/engine/sound.cpp
  src/engine/tools.cpp
  src/engine/world.cpp
  src/engine/worldio.cpp
  src/engine/worldlight.cpp
  src/engine/worldocull.cpp
  src/engine/worldrender.cpp
)

set(CLIENT_SOURCES
  src/client/client.cpp
  src/client/clientextras.cpp
  src/client/clientgame.cpp
  src/client/clients2c.cpp
  src/client/menus.cpp
)

set(GAME_SOURCES
  src/game/bot.cpp
  src/game/entities.cpp
  src/game/monster.cpp
  src/game/weapon.cpp
)

set(SERVER_SOURCES
  src/server/server.cpp
  src/server/serverbots.cpp
  src/server/serverms.cpp
  src/server/serverutil.cpp
)

add_library(standalone_obj OBJECT src/server/serverutil.cpp)
target_compile_definitions(standalone_obj PRIVATE STANDALONE SDL_MAIN_HANDLED)
target_include_directories(standalone_obj PRIVATE ${INCLUDE_DIRS})

set(CLIENT_ALL_SOURCES
  src/main.cpp
  ${ENGINE_SOURCES}
  ${CLIENT_SOURCES}
  ${GAME_SOURCES}
  ${SERVER_SOURCES}
)

add_executable(${CLIENT_BIN} ${CLIENT_ALL_SOURCES})
target_include_directories(${CLIENT_BIN} PRIVATE ${INCLUDE_DIRS})

target_link_libraries(${CLIENT_BIN}
  ZLIB::ZLIB
)

if(WIN32)
  target_include_directories(${CLIENT_BIN} PRIVATE ${SDL2_INCLUDE_DIR})
  target_link_libraries(${CLIENT_BIN}
    ${ENET_LIBRARY}
    ${SDL2_LIBRARY}
    ${SDL2_IMAGE_LIBRARY}
    ${SDL2_TTF_LIBRARY}
    ${SDL2_MIXER_LIBRARY}
    opengl32
    glu32
    ws2_32
    winmm
  )
  set_target_properties(${CLIENT_BIN} PROPERTIES LINK_FLAGS "-mconsole")
elseif(APPLE)
  target_compile_options(${CLIENT_BIN} PRIVATE
    ${ENET_CFLAGS}
    ${SDL2_CFLAGS} ${SDL2_IMAGE_CFLAGS} ${SDL2_TTF_CFLAGS} ${SDL2_MIXER_CFLAGS}
  )
  target_link_libraries(${CLIENT_BIN}
    ${ENET_LIBRARIES}
    ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${SDL2_MIXER_LIBRARIES}
    ${OPENGL_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
  )
else()
  target_compile_options(${CLIENT_BIN} PRIVATE
    ${ENET_CFLAGS}
    ${SDL2_CFLAGS} ${SDL2_IMAGE_CFLAGS} ${SDL2_TTF_CFLAGS} ${SDL2_MIXER_CFLAGS}
  )
  target_link_libraries(${CLIENT_BIN}
    ${ENET_LIBRARIES}
    ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${SDL2_MIXER_LIBRARIES}
    OpenGL::GL
    OpenGL::GLU
    X11
  )
endif()

add_executable(${SERVER_BIN}
  $<TARGET_OBJECTS:standalone_obj>
  src/server/server.cpp
  src/server/serverbots.cpp
  src/server/serverms.cpp
  src/engine/tools.cpp
)

target_include_directories(${SERVER_BIN} PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(${SERVER_BIN} PRIVATE STANDALONE SDL_MAIN_HANDLED)

target_link_libraries(${SERVER_BIN}
  ZLIB::ZLIB
)

if(WIN32)
  target_link_libraries(${SERVER_BIN}
    ${ENET_LIBRARY}
    ws2_32
  )
  set_target_properties(${SERVER_BIN} PROPERTIES LINK_FLAGS "-mconsole")
elseif(APPLE)
  target_compile_options(${SERVER_BIN} PRIVATE ${ENET_CFLAGS})
  target_link_libraries(${SERVER_BIN} ${ENET_LIBRARIES})
else()
  target_compile_options(${SERVER_BIN} PRIVATE ${ENET_CFLAGS})
  target_link_libraries(${SERVER_BIN} ${ENET_LIBRARIES})
endif()
