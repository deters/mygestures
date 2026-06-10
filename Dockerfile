FROM rust:alpine

# Install GTK4, Cairo, Graphene, GLib and build tools
RUN apk add --no-cache \
    build-base \
    pkgconfig \
    gtk4.0-dev \
    graphene-dev \
    glib-dev

# Use an external target directory so mounts do not overwrite cached builds
ENV CARGO_TARGET_DIR=/target

WORKDIR /workspace

# Copy manifests first to cache dependencies
COPY Cargo.toml Cargo.lock ./

# Create dummy source files so cargo can compile the dependencies
RUN mkdir -p src/bin && \
    echo "pub fn dummy() {}" > src/lib.rs && \
    echo "fn main() {}" > src/bin/mygestures.rs && \
    echo "fn main() {}" > src/bin/gestos.rs

# Build dependencies in release mode (cached in intermediate layer)
RUN cargo build --release

# Remove the dummy build artifacts to force rebuilding real source files
RUN rm -f /target/release/deps/mygestures* /target/release/deps/gestos* /target/release/deps/libmygestures*

# Copy the actual source directory
COPY src ./src

# Build the real binaries during docker build phase
RUN cargo build --release

# Default command: rebuild if mounted workspace changed, then copy final binaries back to host mount
CMD ["sh", "-c", "cargo build --release && mkdir -p /workspace/target/release && cp /target/release/mygestures /target/release/gestos /workspace/target/release/"]
