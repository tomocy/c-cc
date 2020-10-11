FROM ubuntu:latest

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive \
    apt install -y \
    gcc \
    make \
    git \
    binutils \
    libc6-dev \
    gdb \
    sudo

RUN adduser --disabled-password --gecos '' cc
RUN echo 'cc ALL=(root) NOPASSWD:ALL' > /etc/sudoers.d/cc
USER cc

WORKDIR /home/cc/cc