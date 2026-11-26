FROM alpine:3.23

RUN apk add --no-cache \
    binutils build-base ca-certificates cmake curl gperf meson ninja pkgconf python3 \
    libx11-dev libx11-static libxcb-static libxext-dev libxext-static \
    libxi-dev libxi-static libxfixes-dev libxrandr-dev libxrender-dev \
    freetype-dev freetype-static libpng-static brotli-static expat-dev expat-static \
    bzip2-static zlib-static \
    dbus font-dejavu glib xauth xrdb xvfb-run

ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

# Build the X11 libraries Alpine ships only as shared objects.
RUN set -eu; \
    mkdir -p /usr/local/share/nod-licenses; \
    work=$(mktemp -d); cd "$work"; \
    build_xorg() { \
        curl -fsSLO --retry 3 "https://xorg.freedesktop.org/archive/individual/lib/$1.tar.xz"; \
        printf '%s  %s\n' "$2" "$1.tar.xz" | sha256sum -c; \
        tar -xf "$1.tar.xz"; \
        (cd "$1" && ./configure --enable-static --disable-shared && make -j4 install); \
        cp "$1/COPYING" "/usr/local/share/nod-licenses/$1"; \
    }; \
    build_xorg libXau-1.0.12 74d0e4dfa3d39ad8939e99bda37f5967aba528211076828464d2777d477fc0fb; \
    build_xorg libXdmcp-1.1.5 d8a5222828c3adab70adf69a5583f1d32eb5ece04304f7f8392b6a353aa2228c; \
    build_xorg libXrender-0.9.12 b832128da48b39c8d608224481743403ad1691bf4e554e4be9c174df171d1b97; \
    build_xorg libXrandr-1.5.4 1ad5b065375f4a85915aa60611cc6407c060492a214d7f9daf214be752c3b4d3; \
    build_xorg libXfixes-6.0.2 39f115d72d9c5f8111e4684164d3d68cc1fd21f9b27ff2401b08fddfc0f409ba; \
    rm -rf "$work"

# Build the D-Bus client library and HarfBuzz's FreeType backend.
RUN set -eu; \
    work=$(mktemp -d); cd "$work"; \
    curl -fsSLO --retry 3 https://dbus.freedesktop.org/releases/dbus/dbus-1.16.2.tar.xz; \
    echo '0ba2a1a4b16afe7bceb2c07e9ce99a8c2c3508e5dec290dbb643384bd6beb7e2  dbus-1.16.2.tar.xz' | sha256sum -c; \
    tar -xf dbus-1.16.2.tar.xz; \
    meson setup dbus-build dbus-1.16.2 --buildtype=release --default-library=static \
        --auto-features=disabled -Dmessage_bus=false -Dtools=false; \
    meson compile -C dbus-build -j4; \
    meson install -C dbus-build; \
    cp dbus-1.16.2/LICENSES/AFL-2.1.txt /usr/local/share/nod-licenses/dbus-AFL-2.1; \
    cp dbus-1.16.2/LICENSES/MIT.txt /usr/local/share/nod-licenses/dbus-MIT; \
    curl -fsSL --retry 3 https://codeload.github.com/harfbuzz/harfbuzz/tar.gz/refs/tags/12.2.0 -o harfbuzz.tar.gz; \
    echo 'f63fc519f150465bd0bdafcdf3d0e9c23474f4c474171cd515ea1b3a72c081fb  harfbuzz.tar.gz' | sha256sum -c; \
    tar -xf harfbuzz.tar.gz; \
    meson setup hb-build harfbuzz-12.2.0 --buildtype=release --default-library=static \
        --auto-features=disabled -Dfreetype=enabled -Dtests=disabled -Dutilities=disabled; \
    meson compile -C hb-build -j4; \
    meson install -C hb-build; \
    cp harfbuzz-12.2.0/COPYING /usr/local/share/nod-licenses/harfbuzz; \
    rm -rf "$work"

# Match current desktop Fontconfig configuration syntax and system paths.
RUN set -eu; \
    work=$(mktemp -d); cd "$work"; \
    curl -fsSLO --retry 3 https://gitlab.freedesktop.org/api/v4/projects/890/packages/generic/fontconfig/2.18.4/fontconfig-2.18.4.tar.xz; \
    echo '95013637eff5f81d4871dd16cd098c273264c16d0f9f0b27cc43b81b2794bee3  fontconfig-2.18.4.tar.xz' | sha256sum -c; \
    tar -xf fontconfig-2.18.4.tar.xz; \
    cd fontconfig-2.18.4; \
    ./configure --sysconfdir=/etc --localstatedir=/var --enable-static --disable-shared --disable-docs --disable-nls; \
    make -j4; \
    install -Dm644 src/.libs/libfontconfig.a /usr/local/lib/libfontconfig.a; \
    install -Dm644 fontconfig.pc /usr/local/lib/pkgconfig/fontconfig.pc; \
    mkdir -p /usr/local/include/fontconfig; \
    cp fontconfig/*.h /usr/local/include/fontconfig/; \
    cp COPYING /usr/local/share/nod-licenses/fontconfig; \
    cd /; rm -rf "$work"

# Link SDL's X11 driver directly. The software renderer works without dlopen.
RUN set -eu; \
    work=$(mktemp -d); cd "$work"; \
    curl -fsSLO --retry 3 https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz; \
    echo 'f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f  SDL3-3.4.20.tar.gz' | sha256sum -c; \
    tar -xf SDL3-3.4.20.tar.gz; \
    cmake -S SDL3-3.4.20 -B sdl-build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF \
        -DSDL_X11=ON -DSDL_X11_SHARED=OFF -DSDL_X11_XCURSOR=OFF \
        -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
        -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_GPU=OFF \
        -DSDL_AUDIO=OFF -DSDL_HIDAPI=OFF -DSDL_LIBUDEV=OFF; \
    cmake --build sdl-build --parallel 4; \
    cmake --install sdl-build; \
    cp SDL3-3.4.20/LICENSE.txt /usr/local/share/nod-licenses/SDL3; \
    rm -rf "$work"

# Collect redistribution notices for Alpine's static archives and compiler runtime.
RUN set -eu; \
    licenses=/usr/local/share/nod-licenses; \
    apk add --no-cache --virtual .nod-license-docs libx11-doc libxi-doc; \
    cp /usr/share/licenses/libx11/COPYING "$licenses/libX11"; \
    cp /usr/share/licenses/libxi/COPYING "$licenses/libXi"; \
    apk del .nod-license-docs; \
    work=$(mktemp -d); cd "$work"; \
    notice() { curl -fsSL --retry 3 "$2" -o "$licenses/$1"; }; \
    archive_notice() { \
        curl -fsSLO --retry 3 "$2"; \
        tar -xOf "${2##*/}" "$3" > "$licenses/$1"; \
    }; \
    archive_notice musl https://distfiles.alpinelinux.org/distfiles/v3.23/musl-1.2.5.tar.gz musl-1.2.5/COPYRIGHT; \
    archive_notice libxcb https://xorg.freedesktop.org/archive/individual/lib/libxcb-1.17.0.tar.xz libxcb-1.17.0/COPYING; \
    archive_notice libXext https://xorg.freedesktop.org/archive/individual/lib/libXext-1.3.6.tar.xz libXext-1.3.6/COPYING; \
    archive_notice bzip2 https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz bzip2-1.0.8/LICENSE; \
    notice FreeType https://raw.githubusercontent.com/freetype/freetype/VER-2-14-3/docs/FTL.TXT; \
    notice libpng https://raw.githubusercontent.com/pnggroup/libpng/v1.6.59/LICENSE; \
    notice Brotli https://raw.githubusercontent.com/google/brotli/v1.2.1/LICENSE; \
    notice Expat https://raw.githubusercontent.com/libexpat/libexpat/R_2_11_0/expat/COPYING; \
    notice zlib https://raw.githubusercontent.com/madler/zlib/v1.4/LICENSE; \
    notice GCC https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-15.2.0/COPYING3; \
    notice GCC-runtime https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-15.2.0/COPYING.RUNTIME; \
    rm -rf "$work"
