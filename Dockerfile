FROM rust:alpine

# Install GTK4, Cairo, Graphene, GLib and build tools
RUN apk add --no-cache \
    build-base \
    pkgconfig \
    gtk4.0-dev \
    graphene-dev \
    glib-dev

WORKDIR /workspace

# Default command: build in release mode
CMD ["cargo", "build", "--release"]
