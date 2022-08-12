FROM ubuntu:20.04

COPY --from=mvdan/shfmt /bin/shfmt /usr/local/bin/shfmt

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive \
    apt install -y \
    gcc \
    make \
    git \
    binutils \
    libc6-dev \
    gdb \
    sudo \
    python3 \
    python3-pip \
    shellcheck \
    tclsh \
    tcl-dev \
    autoconf \
    libssl-dev \
    libcurl4-openssl-dev \
    gettext

RUN adduser --disabled-password --gecos '' cc
RUN echo 'cc ALL=(root) NOPASSWD:ALL' > /etc/sudoers.d/cc

USER cc

WORKDIR /home/cc/cc

RUN echo "add-auto-load-safe-path /workspaces/c-cc/.gdbinit" > ~/.gdbinit
