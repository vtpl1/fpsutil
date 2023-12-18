FROM ubuntu:20.04

ENV SHELL /bin/bash

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive && apt-get install -y wget lsb-release \
    software-properties-common gnupg ninja-build ccache \
    build-essential gdb pkg-config git software-properties-common python3-pip python3-setuptools \
    curl zip unzip autoconf autoconf-archive

RUN wget -O cmake.sh https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-linux-x86_64.sh \
    && sh cmake.sh --prefix=/usr/local/ --exclude-subdir && rm -rf cmake.sh

# RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
RUN wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && ./llvm.sh all

ENV PATH="${PATH}:/usr/lib/llvm-17/bin"

RUN pip3 install -U pip
RUN pip3 install bump2version

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

RUN groupmod --gid $USER_GID $USERNAME \
    && usermod --uid $USER_UID --gid $USER_GID $USERNAME \
    && chown -R $USER_UID:$USER_GID /home/$USERNAME