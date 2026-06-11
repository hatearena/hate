cmake_minimum_required(VERSION 3.10)
project(hate VERSION 0.0.2 LANGUAGES CXX)

if(WIN32)
  if(MSVC)
    message(FATAL_ERROR "MSVC is not supported. Use MinGW instead.\n  Configure with: cmake --preset default\n  Or: cmake -G \"MinGW Makefiles\" .")
  endif()
  set(CLIENT_BIN "hate_client")
  set(SERVER_BIN "hate_server")
  set(CLIENT_OUT "hate_client.exe")
  set(SERVER_OUT "hate_server.exe")
else()
  set(CLIENT_BIN "hate_client")
  set(SERVER_BIN "hate_server")
  set(CLIENT_OUT "hate_client")
  set(SERVER_OUT "hate_server")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fsigned-char -fomit-frame-pointer -fpermissive -w -Wno-write-strings -Wno-unused-variable -Wno-c++11-narrowing -g")
if(WIN32 AND NOT MSVC)
  add_compile_definitions(WIN32)
endif()
set(INCLUDE_DIRS src/include)
set(OpenGL_GL_PREFERENCE LEGACY)

if(WIN32 AND NOT MSVC)
  if(NOT CMAKE_PREFIX_PATH)
    set(_msys2_found FALSE)

    if(DEFINED ENV{MINGW_PREFIX})
      set(_cand "$ENV{MINGW_PREFIX}")
      if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
        set(CMAKE_PREFIX_PATH "${_cand}")
        list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
        message(STATUS "Found MSYS2 at: ${_cand} (from MINGW_PREFIX)")
        set(_msys2_found TRUE)
      endif()
    endif()

    if(NOT _msys2_found)
      find_program(_msys2_exe NAMES msys2 msys2.exe)
      if(_msys2_exe)
        get_filename_component(_msys2_root "${_msys2_exe}" DIRECTORY)
        if(DEFINED ENV{MSYSTEM})
          string(TOLOWER "$ENV{MSYSTEM}" _sub)
          set(_cand "${_msys2_root}/${_sub}")
          if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
            set(CMAKE_PREFIX_PATH "${_cand}")
            list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
            message(STATUS "Found MSYS2 at: ${_cand} (from msys2.exe + MSYSTEM)")
            set(_msys2_found TRUE)
          endif()
        endif()
        if(NOT _msys2_found)
          foreach(_sub mingw64 ucrt64 mingw32 clang64)
            set(_cand "${_msys2_root}/${_sub}")
            if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
              set(CMAKE_PREFIX_PATH "${_cand}")
              list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
              message(STATUS "Found MSYS2 at: ${_cand} (from msys2.exe)")
              set(_msys2_found TRUE)
              break()
            endif()
          endforeach()
        endif()
      endif()
      unset(_msys2_exe CACHE)
    endif()

    if(NOT _msys2_found)
      find_program(_mingw_gxx NAMES g++ g++.exe)
      if(_mingw_gxx)
        get_filename_component(_mingw_bin "${_mingw_gxx}" DIRECTORY)
        get_filename_component(_cand "${_mingw_bin}" DIRECTORY)
        if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
          set(CMAKE_PREFIX_PATH "${_cand}")
          message(STATUS "Found MinGW at: ${_cand} (from g++ location)")
          set(_msys2_found TRUE)
        endif()
      endif()
      unset(_mingw_gxx CACHE)
    endif()

    if(NOT _msys2_found AND DEFINED ENV{SCOOP})
      set(_cand "$ENV{SCOOP}/apps/msys2/current/mingw64")
      if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
        set(CMAKE_PREFIX_PATH "${_cand}")
        list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
        message(STATUS "Found MSYS2 at: ${_cand} (from Scoop)")
        set(_msys2_found TRUE)
      endif()
    endif()

    if(NOT _msys2_found)
      set(_msys_roots C:/msys64 C:/msys2)
      if(DEFINED ENV{MSYSTEM})
        string(TOLOWER "$ENV{MSYSTEM}" _sub)
        foreach(_root IN LISTS _msys_roots)
          set(_cand "${_root}/${_sub}")
          if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
            set(CMAKE_PREFIX_PATH "${_cand}")
            list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
            message(STATUS "Found MSYS2 at: ${_cand}")
            set(_msys2_found TRUE)
            break()
          endif()
        endforeach()
      endif()
      if(NOT _msys2_found)
        foreach(_root IN LISTS _msys_roots)
          foreach(_sub mingw64 ucrt64 mingw32 clang64)
            set(_cand "${_root}/${_sub}")
            if(EXISTS "${_cand}/include/zlib.h" AND EXISTS "${_cand}/lib/libz.a")
              set(CMAKE_PREFIX_PATH "${_cand}")
              list(APPEND CMAKE_PROGRAM_PATH "${_cand}/bin")
              message(STATUS "Found MSYS2 at: ${_cand}")
              set(_msys2_found TRUE)
              break()
            endif()
          endforeach()
          if(_msys2_found)
            break()
          endif()
        endforeach()
      endif()
    endif()
  endif()
  if(DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_PREFIX_PATH}/bin/g++.exe")
    list(APPEND CMAKE_PROGRAM_PATH "${CMAKE_PREFIX_PATH}/bin")
  endif()
endif()

find_package(ZLIB REQUIRED)

if(WIN32)
  find_library(ENET_LIBRARY enet REQUIRED)
  find_path(ENET_INCLUDE_DIR enet/enet.h)
  if(ENET_INCLUDE_DIR)
    set(ENET_INCLUDE_DIRS ${ENET_INCLUDE_DIR})
  endif()
  find_library(SDL2_LIBRARY SDL2 REQUIRED)
  find_library(SDL2_IMAGE_LIBRARY SDL2_image REQUIRED)
  find_library(SDL2_TTF_LIBRARY SDL2_ttf REQUIRED)
  find_library(SDL2_MIXER_LIBRARY SDL2_mixer REQUIRED)
  find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
  if(SDL2_INCLUDE_DIR)
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
  endif()
  find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2)
  if(SDL2_IMAGE_INCLUDE_DIR)
    set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
  endif()
  find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2)
  if(SDL2_TTF_INCLUDE_DIR)
    set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_INCLUDE_DIR})
  endif()
  find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h PATH_SUFFIXES SDL2)
  if(SDL2_MIXER_INCLUDE_DIR)
    set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
  endif()
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

  macro(resolve_library PREFIX LIBNAME HEADER HEADER_PATH REQUIRED_FLAG)
    if(NOT ${PREFIX}_FOUND)
      find_path(${PREFIX}_INCLUDE_DIR ${HEADER}
        HINTS /usr/local/include /opt/homebrew/include /usr/include
        PATH_SUFFIXES ${HEADER_PATH}
      )
      find_library(${PREFIX}_LIBRARY_RESOLVED ${LIBNAME}
        HINTS /usr/local/lib /opt/homebrew/lib /usr/lib
      )
      if(NOT ${PREFIX}_LIBRARY_RESOLVED)
        if(${REQUIRED_FLAG})
          message(FATAL_ERROR "${LIBNAME} not found. Install with: brew install ${LIBNAME} on MacOS or check your package manager.")
        endif()
      else()
        set(${PREFIX}_LIBRARIES   ${${PREFIX}_LIBRARY_RESOLVED})
        set(${PREFIX}_LIBRARY_DIRS "")
        if(${PREFIX}_INCLUDE_DIR)
          set(${PREFIX}_INCLUDE_DIRS ${${PREFIX}_INCLUDE_DIR})
        endif()
      endif()
    else()
      set(_resolved "")
      foreach(_lib ${${PREFIX}_LIBRARIES})
        if(EXISTS "${_lib}")
          list(APPEND _resolved "${_lib}")
        else()
          find_library(_lib_path ${_lib}
            HINTS ${${PREFIX}_LIBRARY_DIRS} /usr/local/lib /opt/homebrew/lib /usr/lib
          )
          if(_lib_path)
            list(APPEND _resolved "${_lib_path}")
          else()
            list(APPEND _resolved "${_lib}")
          endif()
          unset(_lib_path CACHE)
        endif()
      endforeach()
      set(${PREFIX}_LIBRARIES ${_resolved})
    endif()
  endmacro()

  resolve_library(ENET      enet       enet/enet.h  ""     TRUE)
  resolve_library(SDL2      SDL2       SDL.h        SDL2   TRUE)
  resolve_library(SDL2_IMAGE SDL2_image SDL_image.h SDL2   TRUE)
  resolve_library(SDL2_TTF  SDL2_ttf   SDL_ttf.h   SDL2   TRUE)
  resolve_library(SDL2_MIXER SDL2_mixer SDL_mixer.h SDL2   TRUE)

  foreach(_prefix ENET SDL2 SDL2_IMAGE SDL2_TTF SDL2_MIXER)
    if(${_prefix}_INCLUDE_DIRS AND NOT ${_prefix}_CFLAGS)
      foreach(_inc ${${_prefix}_INCLUDE_DIRS})
        list(APPEND ${_prefix}_CFLAGS "-I${_inc}")
      endforeach()
    endif()
  endforeach()

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
foreach(_inc_var ENET_INCLUDE_DIRS SDL2_INCLUDE_DIRS SDL2_IMAGE_INCLUDE_DIRS SDL2_TTF_INCLUDE_DIRS SDL2_MIXER_INCLUDE_DIRS)
  if(${_inc_var})
    target_include_directories(standalone_obj PRIVATE ${${_inc_var}})
  endif()
endforeach()

set(CLIENT_ALL_SOURCES
  src/main.cpp
  ${ENGINE_SOURCES}
  ${CLIENT_SOURCES}
  ${GAME_SOURCES}
  ${SERVER_SOURCES}
)

add_executable(${CLIENT_BIN} ${CLIENT_ALL_SOURCES})
target_include_directories(${CLIENT_BIN} PRIVATE ${INCLUDE_DIRS})
foreach(_inc_var ENET_INCLUDE_DIRS SDL2_INCLUDE_DIRS SDL2_IMAGE_INCLUDE_DIRS SDL2_TTF_INCLUDE_DIRS SDL2_MIXER_INCLUDE_DIRS)
  if(${_inc_var})
    target_include_directories(${CLIENT_BIN} PRIVATE ${${_inc_var}})
  endif()
endforeach()

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
  target_compile_options(standalone_obj PRIVATE
    ${ENET_CFLAGS}
    ${SDL2_CFLAGS}
  )
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

  if(BUILD_APPBUNDLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/HateArena-MacOS.app")
    set(LAUNCHER_SCRIPT "${CMAKE_BINARY_DIR}/hate_client_launcher.sh")
    set(PLIST_FILE "${CMAKE_BINARY_DIR}/hate_client_Info.plist")

    file(WRITE "${LAUNCHER_SCRIPT}"
      "#!/bin/bash\n"
      "cd \"$(dirname \"$0\")/../Resources\"\n"
      "exec \"./${CLIENT_BIN}\"\n"
    )

    file(WRITE "${PLIST_FILE}"
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>CFBundleDevelopmentRegion</key>\n"
      "  <string>English</string>\n"
      "  <key>CFBundleExecutable</key>\n"
      "  <string>${CLIENT_BIN}</string>\n"
      "  <key>CFBundleIconFile</key>\n"
      "  <string>hatearena</string>\n"
      "  <key>CFBundleIdentifier</key>\n"
      "  <string>com.hatearena.${CLIENT_BIN}</string>\n"
      "  <key>CFBundleInfoDictionaryVersion</key>\n"
      "  <string>6.0</string>\n"
      "  <key>CFBundleName</key>\n"
      "  <string>HateArena</string>\n"
      "  <key>CFBundlePackageType</key>\n"
      "  <string>APPL</string>\n"
      "  <key>CFBundleShortVersionString</key>\n"
      "  <string>${PROJECT_VERSION}</string>\n"
      "  <key>CFBundleVersion</key>\n"
      "  <string>${PROJECT_VERSION}</string>\n"
      "</dict>\n"
      "</plist>\n"
    )

    add_custom_target(appbundle
      COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/MacOS"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${CLIENT_BIN}>"
        "${BUNDLE_DIR}/Contents/Resources/${CLIENT_BIN}"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/content"
        "${BUNDLE_DIR}/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LAUNCHER_SCRIPT}"
        "${BUNDLE_DIR}/Contents/MacOS/${CLIENT_BIN}"
      COMMAND chmod +x "${BUNDLE_DIR}/Contents/MacOS/${CLIENT_BIN}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PLIST_FILE}"
        "${BUNDLE_DIR}/Contents/Info.plist"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/packaging/darwin/hatearena.icns"
        "${BUNDLE_DIR}/Contents/Resources/hatearena.icns"
      DEPENDS ${CLIENT_BIN}
      COMMENT "Creating macOS .app bundle"
    )
  endif()
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

add_custom_command(TARGET ${CLIENT_BIN} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${CLIENT_BIN}>"
    "${CMAKE_SOURCE_DIR}/content/${CLIENT_OUT}"
  COMMENT "Copying ${CLIENT_OUT} to content/"
)
if(WIN32)
  file(GLOB _runtime_dlls "${CMAKE_SOURCE_DIR}/packaging/windows/*.dll")
  if(_runtime_dlls)
    add_custom_command(TARGET ${CLIENT_BIN} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_runtime_dlls}
        "${CMAKE_BINARY_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_runtime_dlls}
        "${CMAKE_SOURCE_DIR}/content/"
      COMMENT "Copying runtime DLLs"
    )
  endif()
endif()

add_executable(${SERVER_BIN}
  $<TARGET_OBJECTS:standalone_obj>
  src/server/server.cpp
  src/server/serverbots.cpp
  src/server/serverms.cpp
  src/engine/tools.cpp
)

target_include_directories(${SERVER_BIN} PRIVATE ${INCLUDE_DIRS})
foreach(_inc_var ENET_INCLUDE_DIRS SDL2_INCLUDE_DIRS)
  if(${_inc_var})
    target_include_directories(${SERVER_BIN} PRIVATE ${${_inc_var}})
  endif()
endforeach()
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

add_custom_command(TARGET ${SERVER_BIN} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${SERVER_BIN}>"
    "${CMAKE_SOURCE_DIR}/content/${SERVER_OUT}"
  COMMENT "Copying ${SERVER_OUT} to content/"
)
if(WIN32)
  add_custom_command(TARGET ${SERVER_BIN} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${_runtime_dlls}
      "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${_runtime_dlls}
      "${CMAKE_SOURCE_DIR}/content/"
    COMMENT "Copying runtime DLLs"
  )
endif()

option(BUILD_APPIMAGE "Enable AppImage packaging" OFF)
option(BUILD_APPBUNDLE "Enable macOS .app bundle packaging" OFF)

if(UNIX AND NOT APPLE AND BUILD_APPIMAGE)
  install(PROGRAMS packaging/linux/AppRun
    DESTINATION .
  )
  install(TARGETS ${CLIENT_BIN}
    RUNTIME DESTINATION bin
  )
  install(DIRECTORY content/
    DESTINATION share/hate
    PATTERN "hate_client" EXCLUDE
    PATTERN "hate_server" EXCLUDE
  )
  install(FILES packaging/linux/hatearena.desktop
    DESTINATION share/applications
  )
  install(FILES packaging/linux/hatearena.png
    DESTINATION share/icons/hicolor/192x192/apps
  )
  find_program(LINUXDEPLOY linuxdeploy linuxdeploy-x86_64.AppImage)
  if(NOT LINUXDEPLOY)
    set(LINUXDEPLOY "${CMAKE_BINARY_DIR}/linuxdeploy-x86_64.AppImage")
    file(DOWNLOAD
      "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
      "${LINUXDEPLOY}"
      SHOW_PROGRESS
    )
    file(CHMOD "${LINUXDEPLOY}" PERMISSIONS
      OWNER_READ OWNER_WRITE OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
    )
  endif()

  add_custom_target(appimage
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${CMAKE_BINARY_DIR}/AppDir/usr
    COMMAND ${LINUXDEPLOY}
      --appdir ${CMAKE_BINARY_DIR}/AppDir
      --desktop-file ${CMAKE_SOURCE_DIR}/packaging/linux/hatearena.desktop
      --icon-file ${CMAKE_SOURCE_DIR}/packaging/linux/hatearena.png
      --custom-apprun ${CMAKE_SOURCE_DIR}/packaging/linux/AppRun
      --output appimage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${CLIENT_BIN}
    COMMENT "Building AppImage..."
  )
endif()

