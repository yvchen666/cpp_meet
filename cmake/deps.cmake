# cmake/deps.cmake — 集中的第三方依赖发现
include(FetchContent)

# ── 系统库 ──────────────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Opus
pkg_check_modules(OPUS REQUIRED opus)

# x264
pkg_check_modules(X264 REQUIRED x264)

# FFmpeg
pkg_check_modules(AVCODEC  REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(SWSCALE  REQUIRED libswscale)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)
pkg_check_modules(AVUTIL   REQUIRED libavutil)

# ALSA
pkg_check_modules(ALSA REQUIRED alsa)

# V4L2 (header-only from system, no pkg-config needed on Ubuntu)
find_path(V4L2_INCLUDE_DIR linux/videodev2.h REQUIRED)

# Qt6（可选：未安装时跳过 module10/11）
find_package(Qt6 COMPONENTS Core Widgets OpenGL OpenGLWidgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found — building Qt client modules")
else()
    message(WARNING "Qt6 NOT found — skipping module10_qt_client and module11_whiteboard")
endif()

# ── FetchContent ────────────────────────────────────────────────────────────
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest nlohmann_json)
