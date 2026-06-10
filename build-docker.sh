#!/bin/sh
# Build the docker compilation image
docker build -t mygestures-builder .

# Run the compilation with the current workspace mounted
docker run --rm -v "$(pwd)":/workspace mygestures-builder
