# nod - suckless notification daemon

.DEFAULT_GOAL := all
include config.mk
include cui.lock.mk

# Remaking this included file restarts make, so the very first build works too.
# Maintenance/site targets must not trigger a network fetch.
ifeq ($(strip $(MAKECMDGOALS)),)
include $(CUI)/sources.mk
else ifneq ($(filter-out clean distclean uninstall release site site-build site-check,$(MAKECMDGOALS)),)
include $(CUI)/sources.mk
else
-include $(wildcard $(CUI)/sources.mk)
endif

$(CUI)/sources.mk: cui.lock.mk scripts/fetch-cui.sh
	./scripts/fetch-cui.sh $(CUI_REVISION) $(CUI_SHA256) $(CUI)
	touch $@

fetch-cui: $(CUI)/sources.mk

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

# CUI sources, compiled exactly as the bundle's own Makefile does
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
	cp -f nod-dnd $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/nod-dnd
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < nod.1      > $(DESTDIR)$(MANPREFIX)/man1/nod.1
	sed "s/VERSION/$(VERSION)/g" < nod-send.1 > $(DESTDIR)$(MANPREFIX)/man1/nod-send.1
	cp -f nod-dnd.1 $(DESTDIR)$(MANPREFIX)/man1/nod-dnd.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nod.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nod-send.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nod-dnd.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/nod
	rm -f $(DESTDIR)$(PREFIX)/bin/nod-send
	rm -f $(DESTDIR)$(PREFIX)/bin/nod-dnd
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nod.1
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nod-send.1
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nod-dnd.1

# Publish a validated source release: make release name=v0.1.0
release:
	./scripts/release.sh "$(name)"

.PHONY: all clean distclean install uninstall fetch-cui release
