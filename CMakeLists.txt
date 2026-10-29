cmake_minimum_required(VERSION 3.18)

if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(zombiewood C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Vita application configuration
set(VITA_APP_NAME "Zombiewood")
set(VITA_TITLEID "ZOMBW0001")
set(VITA_VPKNAME "Zombiewood")
set(VITA_VERSION "01.55")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2800000000000000")
add_definitions(-DTITLE_ID="${VITA_TITLEID}")

# Game-specific paths
set(DATA_PATH "ux0:data/zombiewood/" CACHE STRING "Path to data (with trailing /)")
set(SO_PATH "${DATA_PATH}libZombieWood.so" CACHE STRING "Path to .so")
add_definitions(-DDATA_PATH="${DATA_PATH}" -DSO_PATH="${SO_PATH}")

option(USE_SCELIBC_IO "Use SceLibcBridge for IO" OFF)
if (USE_SCELIBC_IO)
  add_definitions(-DUSE_SCELIBC_IO)
endif()

set(SHADER_FORMAT "GLSL") # Preferred shader format ('GLSL', 'CG', 'GXP')
if (${SHADER_FORMAT} STREQUAL "GLSL")
  add_definitions(-DUSE_GLSL_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "CG")
  add_definitions(-DUSE_CG_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "GXP")
  add_definitions(-DUSE_GXP_SHADERS)
endif()

# Space-separated flags passed to vitaGL
option(PORT_VITA3K_COMPAT "Decode unsupported textures for Vita3K" OFF)
option(PORT_SAFE_TEXTURES "Decode ETC1 through the conservative texture path" ON)
option(USE_KUBRIDGE "Use the native Vita kuBridge executable loader" ON)
if(USE_KUBRIDGE)
  add_definitions(-DUSE_KUBRIDGE)
endif()
set(VITAGL_MAKE_FLAGS "SOFTFP_ABI=1 SAFE_DRAW=1 SAFE_UNIFORMS=1 LOG_ERRORS=1 NO_SPLASHSCREEN=1")
if(PORT_SAFE_TEXTURES)
  string(APPEND VITAGL_MAKE_FLAGS " SAFE_ETC1=1")
endif()
if(PORT_VITA3K_COMPAT)
  string(APPEND VITAGL_MAKE_FLAGS " HAVE_VITA3K_SUPPORT=1")
endif()
option(PORT_SHADER_CACHE "Cache compiled game shaders between launches" OFF)
if(PORT_SHADER_CACHE)
  string(APPEND VITAGL_MAKE_FLAGS " HAVE_SHADER_CACHE=1")
endif()

# Some softfp SDKs already adapt the float arguments in their import stubs.
# Applying vitaGL's viewport adapter as well would convert the arguments twice.
execute_process(COMMAND "${VITASDK}/bin/arm-vita-eabi-nm${CMAKE_HOST_EXECUTABLE_SUFFIX}"
    "${VITASDK}/arm-vita-eabi/lib/libSceGxm_stub.a"
    OUTPUT_VARIABLE GXM_STUB_SYMBOLS RESULT_VARIABLE GXM_NM_RESULT)
if(GXM_NM_RESULT EQUAL 0 AND GXM_STUB_SYMBOLS MATCHES "__vita_softfp_target_sceGxmSetViewport")
  string(APPEND VITAGL_MAKE_FLAGS " SDK_SOFTFP_STUBS=1")
  message(STATUS "Using SDK-provided softfp GXM import adapters")
endif()

add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)
option(PORT_DIAGNOSTICS "Enable startup and JNI diagnostics" OFF)
option(PORT_PERF_TRACE "Collect temporary frame performance measurements" OFF)
option(PORT_PERF_NATIVE "Measure native game stages in profiling builds" OFF)
option(PORT_TEST_INPUT "Enable file-based input for automated tests" OFF)
if(PORT_PERF_TRACE)
  add_definitions(-DPORT_PERF_TRACE)
endif()
if(PORT_PERF_NATIVE)
  add_definitions(-DPORT_PERF_NATIVE)
endif()
if(PORT_TEST_INPUT)
  add_definitions(-DPORT_TEST_INPUT)
endif()
option(PORT_MISSION_TRACE "Enable focused mission transition diagnostics" OFF)
if(PORT_MISSION_TRACE)
  add_definitions(-DPORT_MISSION_TRACE)
endif()
if(PORT_DIAGNOSTICS)
  add_definitions(-DDEBUG_SOLOADER -DFALSOJNI_DEBUGLEVEL=2)
else()
  add_definitions(-DFALSOJNI_DEBUGLEVEL=4)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q,--allow-multiple-definition -mfloat-abi=softfp -std=gnu11 -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q,--allow-multiple-definition -mfloat-abi=softfp -std=gnu++20 -Wno-write-strings -Wno-psabi")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1 -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g3")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g0 -ffast-math")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g0 -ffast-math")
endif()

set(SOURCES
    source/main.c
    source/controls.c
    source/presentation.c
    source/performance.c
    source/dynlib.c
    source/java.c
    source/patch.c
    source/mission_trace.c
    source/reimpl/bits/_ctype.c
    source/reimpl/errno.c
    source/reimpl/audio_track.c
    source/reimpl/io.c
    source/reimpl/log.c
    source/reimpl/mem.c
    source/reimpl/pthr.c
    source/reimpl/sndfile_stubs.c
    source/reimpl/sys.c
    source/reimpl/time64.c
    source/utils/dialog.c
    source/utils/glutil.c
    source/utils/init.c
    source/utils/logger.c
    source/utils/settings.c
    source/utils/utils.c
    lib/falso_jni/FalsoJNI.c
    lib/falso_jni/FalsoJNI_ImplBridge.c
    lib/falso_jni/FalsoJNI_Logger.c
    lib/falso_jni/converter.c
    lib/falso_ndk/android/AAssetManager.cpp
    lib/falso_ndk/FalsoNDK_Utils.cpp
    lib/sha1/sha1.c
    lib/fios/fios.c
    lib/so_util/so_util.c
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

add_subdirectory(lib/libc_bridge)
add_dependencies(${CMAKE_PROJECT_NAME} SceLibcBridge)

separate_arguments(VITAGL_MAKE_ARGS UNIX_COMMAND "${VITAGL_MAKE_FLAGS}")

# Each configuration owns its library objects. VitaGL's Makefile does not
# invalidate objects when build flags change, so a shared archive could silently
# mix emulator texture decoding with the hardware release.
set(VITAGL_DIR ${CMAKE_BINARY_DIR}/vitagl)
file(COPY "${CMAKE_SOURCE_DIR}/lib/vitagl/" DESTINATION "${VITAGL_DIR}"
     PATTERN ".git" EXCLUDE PATTERN "*.o" EXCLUDE PATTERN "*.a" EXCLUDE
     PATTERN "samples" EXCLUDE)
set(VITAGL_LIB ${VITAGL_DIR}/libvitaGL.a)

include(ProcessorCount)
ProcessorCount(VITAGL_NPROC)
if (VITAGL_NPROC EQUAL 0)
  set(VITAGL_NPROC 4)
endif()

add_custom_target(vitaGL_build
                  COMMAND make -C ${VITAGL_DIR} -j${VITAGL_NPROC} ${VITAGL_MAKE_ARGS}
                  BYPRODUCTS ${VITAGL_LIB}
                  COMMENT "Building VitaGL"
                  VERBATIM)

add_library(vitaGL STATIC IMPORTED)
set_target_properties(vitaGL PROPERTIES
                      IMPORTED_LOCATION ${VITAGL_LIB}
                      INTERFACE_INCLUDE_DIRECTORIES ${VITAGL_DIR}/source)
add_dependencies(${CMAKE_PROJECT_NAME} vitaGL_build)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib/falso_ndk
                           )

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           )

target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      -Wl,--start-group
                      vitaGL
                      vitashark
                      mathneon
                      stdc++
                      OpenSLES
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      z
                      m
                      ScePgf_stub
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
                      SceCommonDialog_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceFios2_stub
                      SceCtrl_stub
                      SceGxm_stub
                      ScePower_stub
                      SceVshBridge_stub
                      -Wl,--end-group
                      )

vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
if(NOT CMAKE_HOST_WIN32)
vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/meminfo meminfo
                ${CMAKE_SOURCE_DIR}/extras/urandom urandom
                ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                )
else()
  # vita.cmake flattens resource arguments into an unquoted string. Use
  # CMake's ZIP writer on Windows so user directories with spaces work.
  file(GLOB_RECURSE VPK_RESOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/extras/*")
  add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/${VITA_VPKNAME}.vpk"
    COMMAND "${VITA_MKSFOEX}" -s TITLE_ID=${VITA_TITLEID}
      -s APP_VER=${VITA_VERSION} -d ATTRIBUTE2=12 "${VITA_APP_NAME}"
      "${CMAKE_BINARY_DIR}/${VITA_VPKNAME}.vpk_param.sfo"
    COMMAND "${CMAKE_COMMAND}" "-DPORT_SOURCE=${CMAKE_SOURCE_DIR}"
      "-DPORT_BUILD=${CMAKE_BINARY_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/PackageVpk.cmake"
    DEPENDS eboot.bin-self "${CMAKE_BINARY_DIR}/eboot.bin" ${VPK_RESOURCES}
      "${CMAKE_SOURCE_DIR}/cmake/PackageVpk.cmake"
    VERBATIM)
  add_custom_target(Zombiewood.vpk-vpk ALL DEPENDS "${CMAKE_BINARY_DIR}/${VITA_VPKNAME}.vpk")
endif()



