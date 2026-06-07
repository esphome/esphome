"""
PlatformIO build filter for LVGL on ESP32.

Excludes platform-specific drivers, draw backends, libraries, and unused
widget source files. This significantly reduces compilation time and binary size.

Used as a PlatformIO extra_scripts middleware, added by ESPHome's LVGL component.

Communication from ESPHome (__init__.py) via build flags:
  -DLVGL_USE_THORVG=1        -> compile ThorVG sources
  -DLVGL_WIDGETS_USED="..."  -> comma-separated list of used widget/feature names
"""
import re
from pathlib import Path

Import("env")

ATOMIC_SHIM_TEXT = """#pragma once

#if defined(ESP_PLATFORM)
#include "freertos/atomic.h"
#else
#error "This atomic.h shim is intended for ESP-IDF / FreeRTOS builds only."
#endif
"""

def write_atomic_shim(shim):
    shim = Path(shim)
    shim.parent.mkdir(parents=True, exist_ok=True)
    if (
        not shim.exists()
        or shim.read_text(encoding="utf-8", errors="ignore") != ATOMIC_SHIM_TEXT
    ):
        shim.write_text(ATOMIC_SHIM_TEXT, encoding="utf-8")
        print("Created LVGL osal atomic.h shim:", shim)


def create_piolibdeps_atomic_shim():
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env.subst("$PIOENV")
    if libdeps_dir and pioenv:
        write_atomic_shim(Path(libdeps_dir) / pioenv / "lvgl" / "src" / "osal" / "atomic.h")


create_piolibdeps_atomic_shim()

# Parse build flags from ESPHome's __init__.py
_build_flags = " ".join(env.get("BUILD_FLAGS", []))
_thorvg_enabled = "LVGL_USE_THORVG=1" in _build_flags
_sysmon_enabled = "LVGL_USE_SYSMON=1" in _build_flags

# Extract used widgets list from build flags
# Format: -DLVGL_WIDGETS_USED=\"label,button,slider,...\"
_used_widgets = set()
_match = re.search(r'LVGL_WIDGETS_USED=\\"([^"]*)\\"', _build_flags)
if not _match:
    _match = re.search(r'LVGL_WIDGETS_USED="([^"]*)"', _build_flags)
if _match:
    _used_widgets = set(_match.group(1).split(","))

# Mapping from lv_uses names (ESPHome) to LVGL source file names
# ESPHome widget name -> LVGL 9.x source file name (without lv_ prefix and .c/.h)
# Only widgets that have a DIFFERENT name in the LVGL C source need mapping.
# Most widgets have the same name (e.g., "label" -> "lv_label.c")
_WIDGET_NAME_TO_LVGL_FILE = {
    "btn": "button",          # ESPHome get_uses() returns "btn", LVGL file is lv_button.c
    "btnmatrix": "buttonmatrix",
    "img": "image",           # LVGL 9.x renamed lv_img -> lv_image
    "imgbtn": "imagebutton",  # LVGL 9.x renamed
}

# All LVGL widget source files (in src/widgets/) and their corresponding
# ESPHome lv_uses name. When a widget is NOT in lv_uses, its source file
# will be excluded from compilation.
_LVGL_WIDGET_FILES = {
    # LVGL file base name -> set of ESPHome lv_uses names that require it
    "animimage": {"animimg", "animimage"},
    "arc": {"arc"},
    "bar": {"bar"},
    "button": {"button", "btn"},
    "buttonmatrix": {"buttonmatrix", "btnmatrix"},
    "calendar": {"calendar"},
    "canvas": {"canvas", "lottie"},
    "chart": {"chart"},
    "checkbox": {"checkbox"},
    "dropdown": {"dropdown"},
    "image": {"image", "img", "lottie"},
    "imagebutton": {"imgbtn", "imagebutton"},
    "keyboard": {"keyboard"},
    "label": {"label"},
    "led": {"led"},
    "line": {"line"},
    "list": {"list"},
    "lottie": {"lottie"},
    "menu": {"menu"},
    "msgbox": {"msgbox"},
    "roller": {"roller"},
    "scale": {"scale", "meter"},
    "slider": {"slider"},
    "span": {"span", "spangroup"},
    "spinbox": {"spinbox"},
    "spinner": {"spinner"},
    "switch": {"switch"},
    "table": {"table"},
    "tabview": {"tabview"},
    "textarea": {"textarea"},
    "tileview": {"tileview"},
    "win": {"win"},
}

# Determine which LVGL widget files are needed
_needed_widget_files = set()
for lvgl_file, use_names in _LVGL_WIDGET_FILES.items():
    if use_names & _used_widgets:
        _needed_widget_files.add(lvgl_file)

# QR code is in libs/qrcode/, not widgets/
_qrcode_needed = "qrcode" in _used_widgets

# Lottie requires LVGL canvas and image internally.
# The build filter sees only widgets explicitly used in YAML, so using
# lottie does not automatically keep lv_canvas.c unless we add it here.
if "lottie" in _used_widgets:
    _needed_widget_files.add("canvas")
    _needed_widget_files.add("image")

def lvgl_src_filter(env, node):
    """Skip compilation of LVGL source files not needed for ESP32."""
    path = str(node.get_path()).replace("\\", "/")

    # ESP-IDF / FreeRTOS atomic.h fix for LVGL compiled from .piolibdeps.
    #
    # lv_freertos.c contains:
    #   #include "atomic.h"
    #
    # The shim atomic.h from the external component is not reliably visible
    # when PlatformIO compiles LVGL as a library from .piolibdeps.
    #
    # Put atomic.h directly next to lv_freertos.c so quoted include resolution
    # always finds it first.
    if path.endswith("/osal/lv_freertos.c"):
        try:
            src = Path(node.get_path())
            write_atomic_shim(src.parent / "atomic.h")

        except Exception as err:
            print("WARNING: failed to create LVGL osal atomic.h shim:", err)

    # Only filter files inside the LVGL library
    if "/lvgl/" not in path:
        return node

    # ===== Draw backends NOT available on ESP32 =====
    EXCLUDED_DRAW = [
        "/draw/nanovg/",           # NanoVG (OpenGL) - desktop only
        "/draw/nema_gfx/",         # NemaGFX - Renesas/Think Silicon GPU
        "/draw/nxp/",              # NXP PXP/G2D - NXP MCUs only
        "/draw/renesas/",          # Renesas Dave2D - Renesas MCUs only
        "/draw/eve/",              # FT800/FT813 EVE GPU
        "/draw/vg_lite/",          # VG-Lite - NXP/Vivante GPU
        "/draw/dma2d/",            # STM32 DMA2D - STM32 only
        "/draw/sdl/",              # SDL - desktop only
        "/draw/opengles/",         # OpenGL ES - desktop only
    ]

    # ===== Display/input drivers NOT for ESP32 =====
    EXCLUDED_DRIVERS = [
        "/drivers/wayland/",       # Linux Wayland
        "/drivers/x11/",           # Linux X11
        "/drivers/windows/",       # Windows
        "/drivers/sdl/",           # SDL desktop
        "/drivers/nuttx/",         # NuttX RTOS
        "/drivers/qnx/",          # QNX RTOS
        "/drivers/uefi/",          # UEFI firmware
        "/drivers/opengles/",      # OpenGL ES desktop
        "/drivers/draw/eve/",      # EVE display driver
        "/drivers/display/drm/",         # Linux DRM
        "/drivers/display/fb/",          # Linux framebuffer
        "/drivers/display/ft81x/",       # FT81x display
        "/drivers/display/lovyan_gfx/",  # LovyanGFX (handled by ESPHome)
        "/drivers/display/nxp_elcdif/",  # NXP eLCDIF
        "/drivers/display/renesas_glcdc/",  # Renesas GLCDC
        "/drivers/display/st_ltdc/",     # STM32 LTDC
        "/drivers/display/tft_espi/",    # TFT_eSPI (handled by ESPHome)
        "/drivers/display/nv3007/",      # NV3007
        "/drivers/libinput/",      # Linux libinput
        "/drivers/evdev/",         # Linux evdev
    ]

    # ===== Libraries NOT needed on ESP32 =====
    EXCLUDED_LIBS = [
        "/libs/gltf/",             # 3D glTF rendering (OpenGL required)
        "/libs/nanovg/",           # NanoVG (OpenGL required)
        "/libs/ffmpeg/",           # FFmpeg video decoding
        "/libs/freetype/",         # FreeType font engine (use tiny_ttf instead)
        "/libs/rlottie/",          # rlottie (use ThorVG Lottie instead)
        "/libs/libpng/",           # libpng (use pngdec/lodepng instead)
        "/libs/libjpeg_turbo/",    # libjpeg-turbo (use tjpgd instead)
        "/libs/libwebp/",          # libwebp (use ThorVG WebP instead)
        "/libs/frogfs/",           # FrogFS filesystem
        "/libs/vg_lite_driver/",   # VG-Lite driver library
        "/libs/FT800-FT813/",      # FT800/FT813 EVE library
        "/libs/fsdrv/lv_fs_win32.",     # Windows filesystem
        "/libs/fsdrv/lv_fs_uefi.",      # UEFI filesystem
        "/libs/fsdrv/lv_fs_stdio.",     # stdio filesystem (desktop)
        "/libs/fsdrv/lv_fs_arduino_sd.",  # Arduino SD (not ESP-IDF)
        "/libs/fsdrv/lv_fs_arduino_esp_littlefs.",  # Arduino LittleFS
        "/libs/fsdrv/lv_fs_frogfs.",    # FrogFS driver
        "/libs/fsdrv/lv_fs_littlefs.",  # LittleFS driver
    ]

    # ===== OS abstraction layers NOT for ESP32 (uses FreeRTOS) =====
    EXCLUDED_OSAL = [
        "/osal/lv_linux.",          # Linux
        "/osal/lv_windows.",        # Windows
        "/osal/lv_sdl2.",           # SDL2
        "/osal/lv_pthread.",        # POSIX threads
        "/osal/lv_cmsis_rtos2.",    # CMSIS RTOS2
        "/osal/lv_mqx.",            # MQX RTOS
        "/osal/lv_rtthread.",       # RT-Thread
    ]

    # ===== stdlib NOT for ESP32 (uses custom malloc) =====
    EXCLUDED_STDLIB = [
        "/stdlib/micropython/",     # MicroPython
        "/stdlib/rtthread/",        # RT-Thread
        "/stdlib/uefi/",            # UEFI
    ]

    # ===== Debug/test files NOT for production =====
    EXCLUDED_DEBUG = [
        "/debugging/monkey/",               # Monkey testing
        "/debugging/test/",                 # Test helpers
        "/debugging/vg_lite_tvg/",          # VG-Lite ThorVG debug
    ]
    if not _sysmon_enabled:
        EXCLUDED_DEBUG.append("/debugging/sysmon/lv_sysmon.")

    # Combine platform exclusions (always applied)
    all_excluded = (
        EXCLUDED_DRAW
        + EXCLUDED_DRIVERS
        + EXCLUDED_LIBS
        + EXCLUDED_OSAL
        + EXCLUDED_STDLIB
        + EXCLUDED_DEBUG
    )

    # ===== Conditionally exclude ThorVG/SVG/Lottie when not needed =====
    if not _thorvg_enabled:
        all_excluded += [
            "/libs/thorvg/",           # ThorVG vector engine (~500KB+)
            "/libs/lottie/",           # Lottie animation parser
            "/libs/svg/",              # SVG parser
            "/draw/lv_draw_vector.",   # Vector drawing operations
            "/draw/sw/lv_draw_sw_vector.",  # SW vector renderer
        ]

    # ===== Conditionally exclude QR code library =====
    if not _qrcode_needed:
        all_excluded.append("/libs/qrcode/")

    # ===== Conditionally exclude GIF / BMP libraries =====
    # Must match the gating in components/lvgl/__init__.py where LV_USE_BMP /
    # LV_USE_GIF are set to 1 when any of 'image', 'img', or 'animimg' widgets
    # are used. If we exclude the source while the defines are on, lv_init()
    # references lv_bmp_init / lv_gif_init that no longer have definitions.
    if (
        "image" not in _used_widgets
        and "img" not in _used_widgets
        and "animimg" not in _used_widgets
    ):
        all_excluded.append("/libs/gif/")
        all_excluded.append("/libs/bmp/")

    # Check platform/library exclusions first
    for pattern in all_excluded:
        if pattern in path:
            return None  # Skip this file

    # ===== Per-widget source file exclusion =====
    # LVGL v9.x uses subdirectories: src/widgets/menu/lv_menu.c
    # LVGL v8.x used flat layout: src/widgets/lv_menu.c
    # Support both patterns.
    if _used_widgets and "/widgets/" in path and "/lv_" in path:
        match = re.search(r"/widgets/(?:\w+/)?lv_(\w+)\.[ch]", path)
        if match:
            widget_file_name = match.group(1)
            if widget_file_name not in _needed_widget_files:
                return None  # Skip: widget not used

    return node


env.AddBuildMiddleware(lvgl_src_filter)
