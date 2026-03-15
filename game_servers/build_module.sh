#!/bin/bash

MODULE_NAME=$1
if [ -z "$MODULE_NAME" ]; then
    echo "Error: module_name is missing" >&2
    exit 1
fi

cd $MODULE_NAME

case "$MODULE_NAME" in
    bluemsx)
        make && \
            echo "done:bluemsx Machines launch.sh stop.sh"
        ;;
    chocolate-doom)
        if [ ! -f Makefile ]; then
            ./autogen.sh || exit 1
        fi
        make && \
                echo "done:src/chocolate-doom src/chocolate-heretic src/chocolate-hexen src/chocolate-strife launch.sh stop.sh"
        ;;
    wolf4sdl)
        touch version.h && make CONFIG=config.ssw RGBS_DIR=../deps/rgbserver && \
            touch version.h && make CONFIG=config.sss RGBS_DIR=../deps/rgbserver && \
            echo "done:wolf3d spear launch.sh stop.sh"
        ;;
    fbneo)
        cd src/burner/libretro/ && make && \
            echo "done:src/burner/libretro/fbneo_libretro.so"
        ;;
    nestopia)
        cd libretro && make && \
            echo "done:libretro/nestopia_libretro.so"
        ;;
    genplus-gx)
        make -f Makefile.libretro && \
            echo "done:genesis_plus_gx_libretro.so"
        ;;
    snes9x)
        cd libretro && make && \
            echo "done:libretro/snes9x_libretro.so"
        ;;
    geargrafx)
        cd platforms/libretro && make && \
            echo "done:platforms/libretro/geargrafx_libretro.so"
        ;;
    mgba)
        if [ ! -f build/Makefile ]; then
            mkdir -p build && cd build && cmake \
                -DBUILD_SHARED=OFF \
                -DBUILD_STATIC=ON \
                -DBUILD_SDL=OFF \
                -DBUILD_QT=OFF \
                -DBUILD_LIBRETRO=ON \
                -DUSE_SQLITE3=OFF \
                -DENABLE_DEBUGGERS=OFF \
                -DUSE_DISCORD_RPC=OFF \
                -DENABLE_GDB_STUB=OFF \
                -DENABLE_SCRIPTING=OFF \
                -DCMAKE_INSTALL_PREFIX:PATH=/usr .. || exit 1
            cd ..
        fi

        cd build && \
            make && \
            echo "done:build/mgba_libretro.so"
        ;;
    *)
        echo "Unknown module: $MODULE_NAME" >&2
        exit 1
        ;;
esac
