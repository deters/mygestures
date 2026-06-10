# Makefile for mygestures (Meson wrapper)

PREFIX ?= /usr/local
BUILDDIR = build

.PHONY: all build install uninstall clean setup

setup:
	meson setup $(BUILDDIR) --prefix=$(PREFIX)

all: build

build:
	@if [ ! -d "$(BUILDDIR)" ]; then \
		meson setup $(BUILDDIR) --prefix=$(PREFIX); \
	fi
	meson compile -C $(BUILDDIR)

install:
	@if [ ! -d "$(BUILDDIR)" ]; then \
		meson setup $(BUILDDIR) --prefix=$(PREFIX); \
	fi
	meson install -C $(BUILDDIR)

uninstall:
	@if [ -d "$(BUILDDIR)" ]; then \
		ninja -C $(BUILDDIR) uninstall; \
	else \
		echo "Build directory not found, cannot uninstall automatically."; \
	fi

clean:
	rm -rf $(BUILDDIR)

.PHONY: version major minor patch

version:
	@if [ -z "$(VERSION)" ]; then \
		./packaging/bump-version.sh; \
	else \
		./packaging/bump-version.sh $(VERSION); \
	fi

major:
	./packaging/bump-version.sh major

minor:
	./packaging/bump-version.sh minor

patch:
	./packaging/bump-version.sh patch
