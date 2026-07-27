FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    ccache \
    clang-18 \
    clangd-18 \
    clang-format-18 \
    clang-tidy-18 \
    cmake \
    cppcheck \
    curl \
    git \
    iproute2 \
    iptables \
    iputils-ping \
    lld-18 \
    lldb-18 \
    ninja-build \
    pkg-config \
    stress-ng \
    tar \
    unzip \
    valgrind \
    zip \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# Enable bash tab-completion (git, etc.) in non-login interactive shells
RUN echo '. /etc/profile.d/bash_completion.sh' >> /etc/bash.bashrc

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
    && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100

# Node.js and pnpm
ARG NODE_VERSION=24.15.0
RUN curl -fsSL "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-x64.tar.xz" -o /tmp/node.tar.xz \
    && tar -xJf /tmp/node.tar.xz -C /usr/local --strip-components=1 \
    && rm /tmp/node.tar.xz \
    && corepack enable pnpm \
    && npm cache clean --force

# CMake 4.2.1
ARG CMAKE_VER=4.3.2
RUN curl -OL https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/cmake-${CMAKE_VER}-linux-x86_64.sh \
    && chmod +x cmake-${CMAKE_VER}-linux-x86_64.sh \
    && ./cmake-${CMAKE_VER}-linux-x86_64.sh --prefix=/usr/local --skip-license \
    && rm cmake-${CMAKE_VER}-linux-x86_64.sh

USER vscode

# Sync vcpkg with builtin-baseline commit
COPY vcpkg.json /tmp/vcpkg.json
WORKDIR /tmp
ARG VCPKG_COMMIT="66c0373dc7fca549e5803087b9487edfe3aca0a1"
RUN git config --global --add safe.directory ${VCPKG_ROOT} \
    && cd ${VCPKG_ROOT} \
    && git fetch origin \
    && git checkout ${VCPKG_COMMIT} \
    && ./bootstrap-vcpkg.sh \
    && cd /tmp \
    && vcpkg install --triplet x64-linux --clean-after-build

ENV PATH="/usr/lib/ccache:${PATH}"

# Set the working directory to the project folder
WORKDIR /workspaces/chaos
