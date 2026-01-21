#!/bin/bash
# Install build dependencies for Waechter
set -e

echo "Updating package lists..."
sudo apt-get update

echo "Installing dependencies..."
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  clang llvm \
  linux-tools-common \
  libbpf-dev \
  libelf-dev \
  libcurl4-gnutls-dev \
  zlib1g-dev \
  libwebsockets-dev \
  libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev libxxf86vm-dev \
  libgl1-mesa-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev \
  ccache

echo "Dependencies installed successfully!"
