# SystemC and SystemVerilog development environment for tiny_5_riscv.
FROM ubuntu:22.04

ARG SYSTEMC_VERSION=3.0.2
ARG VERILATOR_VERSION=5.046
ARG BUILD_JOBS=2

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        autoconf \
        binutils-riscv64-unknown-elf \
        bison \
        build-essential \
        ca-certificates \
        ccache \
        cmake \
        curl \
        flex \
        gcc-riscv64-unknown-elf \
        gdb \
        git \
        help2man \
        less \
        libfl-dev \
        ninja-build \
        opensta \
        perl \
        pkg-config \
        python3 \
        vim \
        yosys \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Match this project's C++17 ABI and use one predictable library directory.
RUN git clone --depth 1 --branch "${SYSTEMC_VERSION}" \
        https://github.com/accellera-official/systemc.git /tmp/systemc \
    && cmake -S /tmp/systemc -B /tmp/systemc/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=ON \
    && cmake --build /tmp/systemc/build --parallel "${BUILD_JOBS}" \
    && cmake --install /tmp/systemc/build \
    && ldconfig \
    && rm -rf /tmp/systemc

ENV SYSTEMC_INCLUDE=/usr/local/include \
    SYSTEMC_LIBDIR=/usr/local/lib \
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig

# Configure Verilator after SystemC so --sc uses the same installation.
RUN git clone --depth 1 --branch "v${VERILATOR_VERSION}" \
        https://github.com/verilator/verilator.git /tmp/verilator \
    && cd /tmp/verilator \
    && autoconf \
    && ./configure --prefix=/usr/local \
    && make -j "${BUILD_JOBS}" \
    && make install \
    && rm -rf /tmp/verilator /root/.cache

# Print the installed SystemC and RISC-V toolchain versions.
RUN printf 'SystemC %s\n' "${SYSTEMC_VERSION}" \
    && riscv64-unknown-elf-gcc --version \
    && riscv64-unknown-elf-objcopy --version

WORKDIR /workspace
CMD ["/bin/bash"]
