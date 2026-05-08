FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \  
    clang \
    clangd \
    lld \
    cmake \
    ninja-build \
    pkg-config \
    zip \
    unzip \
    tar \
    curl \
    git \
    ca-certificates \
    iproute2 \
    iputils-ping \
    stress-ng \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# Bun
RUN curl -fsSL https://bun.sh/install | bash \
    && ln -s /root/.bun/bin/bun /usr/local/bin/bun \
    && /usr/local/bin/bun --version

# vcpkg
ENV VCPKG_ROOT=/vcpkg
ENV PATH=$VCPKG_ROOT:$PATH
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh

# CMake 4.2.1
ARG CMAKE_VER=4.2.1
RUN curl -OL https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/cmake-${CMAKE_VER}-linux-x86_64.sh \
    && chmod +x cmake-${CMAKE_VER}-linux-x86_64.sh \
    && ./cmake-${CMAKE_VER}-linux-x86_64.sh --prefix=/usr/local --skip-license \
    && rm cmake-${CMAKE_VER}-linux-x86_64.sh

# Install vcpkg dependencies
COPY vcpkg.json /tmp/vcpkg.json
WORKDIR /tmp
RUN vcpkg install --triplet x64-linux --clean-after-build \
    && chown -R vscode:vscode /vcpkg

WORKDIR /workspaces/chaos
