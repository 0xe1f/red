#!/bin/bash

try_libretro_build() {
    local makefile="$1"
    local mf_dir

    if [ -n "$makefile" ]; then
        mf_dir=$(dirname "$makefile")
        pushd "$mf_dir" > /dev/null && \
            make -f "$(basename "$makefile")" && \
            popd > /dev/null && \
            echo "done:$(find "$mf_dir" -maxdepth 1 -type f -name '*_libretro.so')"
        exit 0
    fi
}

MODULE_NAME=$1
if [ -z "$MODULE_NAME" ]; then
    echo "Error: module_name is missing" >&2
    exit 1
fi

cd $MODULE_NAME

case "$MODULE_NAME" in
    chocolate-doom)
        if [ ! -f Makefile ]; then
            ./autogen.sh || exit 1
        fi
        make && \
                echo "done:src/chocolate-doom src/chocolate-heretic src/chocolate-hexen src/chocolate-strife launch.sh stop.sh"
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
    atari800)
        make && \
            echo "done:atari800_libretro.so"
        ;;
    dosbox)
        cd libretro && \
            make BUNDLED_AUDIO_CODECS=1 BUNDLED_LIBSNDFILE=1 BUNDLED_SDL=1 WITH_DYNAREC=arm64 deps && \
            make BUNDLED_AUDIO_CODECS=1 BUNDLED_LIBSNDFILE=1 BUNDLED_SDL=1 WITH_DYNAREC=arm64 -j`nproc` && \
            echo "done:libretro/dosbox_core_libretro.so"
        ;;
    *)
        # See if there's a Makefile.libretro and build that
        try_libretro_build `find . -type f -name "Makefile.libretro"`
        # See if there's a Makefile in a libretro subdirectory and build that
        try_libretro_build `find . -path '*/libretro/*' -name "Makefile" -type f | head -1`

        # Well, shit...
        echo "Unknown or unbuildable module: $MODULE_NAME" >&2
        exit 1
        ;;
esac
