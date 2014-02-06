target_platform	mingw32
name            NetCleanVideoPlayer

template        c++11
cflags          ggdb
sourcedir       src
cflags          std=c++0x Wall Wno-deprecated-declarations

lib-static      libavdevice libavformat libavcodec libavfilter libswscale libavutil sdl

#ldflags         mconsole 

# link statically
ldflags         static-libgcc "Wl,-Bstatic $(tmpdir)/versioninfo.res"

prebuildscript  "$(host_dash)windres -O coff -o $(tmpdir)/versioninfo.res res/versioninfo.rc"
