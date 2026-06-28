FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Keep layers small: update, install packages, then clean apt lists
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  clang llvm \
  libbpf-dev \
  libelf-dev \
  libcurl4-gnutls-dev \
  zlib1g-dev \
  libwebsockets-dev \
  libsdl2-dev \
  libgl1-mesa-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev \
  libsqlite3-dev \
  sudo \
  ccache \
  wget \
 && rm -rf /var/lib/apt/lists/*

  # Install an up-to-date bpftool (matches CI)
  ARG BPFTOOL_VERSION=v7.6.0
  RUN ARCH=$(uname -m) \
   && case "$ARCH" in \
     x86_64) BPFTOOL_ARCH="amd64" ;; \
     aarch64) BPFTOOL_ARCH="arm64" ;; \
     *) echo "Unsupported architecture: $ARCH"; exit 1 ;; \
   esac \
   && wget -q "https://github.com/libbpf/bpftool/releases/download/${BPFTOOL_VERSION}/bpftool-${BPFTOOL_VERSION}-${BPFTOOL_ARCH}.tar.gz" \
   && tar -xzf "bpftool-${BPFTOOL_VERSION}-${BPFTOOL_ARCH}.tar.gz" \
   && install -m 755 bpftool /usr/local/bin/bpftool \
   && rm -f "bpftool-${BPFTOOL_VERSION}-${BPFTOOL_ARCH}.tar.gz" bpftool \
   && bpftool --version

# Create a non-root builder user to match CI usage and to avoid file permission issues
ARG USER=builder
ARG UID=1000
ARG GID=1000
RUN groupadd -g ${GID} ${USER} || true \
 && useradd -m -u ${UID} -g ${GID} -s /bin/bash ${USER} \
 && echo "${USER} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USER} \
 && chmod 0440 /etc/sudoers.d/${USER}

# Create workspace mount point and make builder own it
RUN mkdir -p /workspace && chown ${USER}:${USER} /workspace

WORKDIR /workspace
USER ${USER}

# Provide a small entrypoint (bash)
ENTRYPOINT ["/bin/bash"]

