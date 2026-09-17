# SystemC Hello World

A standalone C++17 example for checking the SystemC development environment.
It runs a SystemC thread, prints a greeting, waits 10 ns of simulation time,
prints another greeting, and stops. No CPU model, RISC-V program, or Verilator
build is needed.

## Run in Docker

From the repository root:

```sh
./run_docker.sh ./systemc_hello/run.sh
```

The Docker launcher builds the development image if needed. The example runner
configures and builds this folder in `systemc_hello/build/`, runs its CTest smoke
test, and executes the example. When the image already exists, no rebuild prompt
is shown in this command mode.

The simulation prints these lines, along with the SystemC startup/stop messages:

```text
0 s: Hello, SystemC!
10 ns: Hello, SystemC!
```

## Build Manually

After entering the container with `./run_docker.sh`, run from `/workspace`:

```sh
cmake -S systemc_hello -B systemc_hello/build -DCMAKE_BUILD_TYPE=Debug
cmake --build systemc_hello/build -j2
ctest --test-dir systemc_hello/build --output-on-failure
./systemc_hello/build/systemc_hello
```

The same example works on a host with a C++17 compiler, CMake, and SystemC
development files. It uses the repository's shared SystemC dependency lookup:
the `SystemCLanguage` CMake package when available, otherwise `pkg-config` and
`systemc.pc`. No `.pc` file is needed for the Docker source installation.
Use a separate build directory for host and Docker
builds to avoid mixing compiler and library installations. The runner accepts
an optional build-directory argument, for example on the host:

```sh
./systemc_hello/run.sh ./systemc_hello/build-host
```
