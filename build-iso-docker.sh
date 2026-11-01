#!/bin/sh
# No Linux tools installed? With Docker (Windows/Mac/Linux) this builds ClaudeOS.iso in one go.
docker run --rm -v "$PWD":/w -w /w ubuntu:24.04 sh -c "apt-get update && apt-get install -y build-essential grub-pc-bin xorriso mtools && ./build-iso.sh"
