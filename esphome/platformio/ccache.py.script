import os

# pylint: disable=E0602
Import("env")  # noqa

# ESPHome decides whether ccache is used and exports the CCACHE_* settings
# into the environment before PlatformIO starts (_ccache_env() in
# esphome/platformio/toolchain.py); this script only supplies the SCons-level
# mechanism.
#
# The binary comes pre-resolved in ESPHOME_CCACHE_PATH; _ccache_env() has
# already stripped the Windows \\?\ prefix that cmd.exe cannot run.
#
# This is a "pre" script, so the platform's builder (which sets CC/CXX and
# clones the construction environment for framework and library builds) runs
# after it. Replacing CC/CXX here would be overwritten, and replacing them in
# a "post" script would miss the already-cloned library environments. Wrapping
# SPAWN instead is ordering-proof: clones copy the wrapper, and every compiler
# invocation from every environment funnels through it at execution time.
if os.environ.get("ESPHOME_CCACHE_ENABLE") == "1" and (
    ccache_path := os.environ.get("ESPHOME_CCACHE_PATH")
):
    original_spawn = env["SPAWN"]

    def ccache_spawn(sh, escape, cmd, args, child_env):
        # Only wrap compile steps (gcc/g++ with -c); linking, archiving and
        # the other tools gain nothing from ccache.
        prog = os.path.basename(cmd).removesuffix(".exe")
        if prog.endswith(("gcc", "g++")) and "-c" in args:
            cmd = ccache_path
            args = [escape(ccache_path), *args]
        return original_spawn(sh, escape, cmd, args, child_env)

    env.Replace(SPAWN=ccache_spawn)
    print("ESPHome: Compiling with ccache")
