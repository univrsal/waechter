FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Keep layers small: update, install packages, then clean apt lists
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
            build-essential \
            cmake \
            ninja-build \
            pkg-config \
            clang llvm \
            linux-tools-common \
            libbpf-dev \
            libelf-dev \
            sudo \
            zlib1g-dev \
            libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev libxxf86vm-dev \
            libgl1-mesa-dev \
            libwayland-dev wayland-protocols libxkbcommon-dev \
 && rm -rf /var/lib/apt/lists/*

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

