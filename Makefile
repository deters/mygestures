# Makefile for mygestures (Meson wrapper)

PREFIX ?= /usr/local
BUILDDIR = build

.PHONY: all build install uninstall clean

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
		./bump-version.sh; \
	else \
		./bump-version.sh $(VERSION); \
	fi

major:
	./bump-version.sh major

minor:
	./bump-version.sh minor

patch:
	./bump-version.sh patch
