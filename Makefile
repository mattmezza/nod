# nod - suckless notification daemon

include config.mk
include $(CUI)/sources.mk

# CUI: portable core + FreeType/HarfBuzz text + bidi + SDL3 renderer
CUISRC = $(PORTABLE_SOURCES) $(TEXT_SOURCES) $(BIDI_SOURCES) \
         $(SHEENBIDI_SOURCES) $(SDL3_SOURCES)
CUIOBJ = $(CUISRC:%.c=build/%.o)
CUIA   = build/libcui.a

all: nod

config.h:
	cp config.def.h $@

nod.o: nod.c config.h config.mk
	$(CC) -c $(CFLAGS) nod.c

nod: nod.o $(CUIA)
	$(CC) -o $@ nod.o $(CUIA) $(LDFLAGS)

$(CUIA): $(CUIOBJ)
	$(AR) rcs $@ $(CUIOBJ)

# vendored sources, compiled exactly as the bundle's own Makefile does
build/vendor/src/text/cui_text.o: CPPFLAGS += -DCUI_TEXT_BIDI
build/vendor/src/text/cui_bidi.o: CPPFLAGS += -I$(CUI)/vendor/third_party/sheenbidi/Headers
build/vendor/third_party/sheenbidi/%.o: CPPFLAGS += -DSB_CONFIG_UNITY \
	-I$(CUI)/vendor/third_party/sheenbidi/Headers -I$(CUI)/vendor/third_party/sheenbidi/Source
build/vendor/third_party/sheenbidi/%.o: WNO = -Wno-unused-parameter -Wno-unused-value \
	-Wno-unused-function -Wno-implicit-fallthrough

build/%.o: $(CUI)/%.c
	@mkdir -p $(dir $@)
	@echo CC $<
	@$(CC) $(CFLAGS) $(CPPFLAGS) -O2 $(WNO) -c $< -o $@

clean:
	rm -f nod nod.o

distclean: clean
	rm -rf build config.h

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f nod $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/nod
	cp -f nod-send $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/nod-send
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < nod.1      > $(DESTDIR)$(MANPREFIX)/man1/nod.1
	sed "s/VERSION/$(VERSION)/g" < nod-send.1 > $(DESTDIR)$(MANPREFIX)/man1/nod-send.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nod.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nod-send.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/nod
	rm -f $(DESTDIR)$(PREFIX)/bin/nod-send
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nod.1
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nod-send.1

# refresh vendor/cui-native from a CUI checkout: make update-cui CUI_SRC=~/dev/cui
update-cui:
	./scripts/update-cui.sh $(CUI_SRC)

.PHONY: all clean distclean install uninstall update-cui
