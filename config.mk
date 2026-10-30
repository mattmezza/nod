# nod - suckless notification daemon

VERSION = 0.2

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man

# Vendored CUI native source bundle (see README.md, "Updating CUI")
CUI = vendor/cui-native

PKG_CONFIG = pkg-config
PKGS = x11 xrandr xrender xext fontconfig dbus-1 sdl3 freetype2 harfbuzz

INCS = -I$(CUI)/vendor/include -I$(CUI)/vendor/third_party/clay \
       `$(PKG_CONFIG) --cflags $(PKGS)`
LIBS = `$(PKG_CONFIG) --libs $(PKGS)` -lm

CPPFLAGS = -DVERSION=\"$(VERSION)\" -D_POSIX_C_SOURCE=200809L
CFLAGS   = -std=c11 -Wall -Wextra -Os $(INCS) $(CPPFLAGS)
LDFLAGS  = $(LIBS)

CC = cc
AR = ar
