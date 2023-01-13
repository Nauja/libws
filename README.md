# libws

[![CI](https://github.com/Nauja/libws/actions/workflows/CI.yml/badge.svg)](https://github.com/Nauja/libws/actions/workflows/CI.yml)
[![CI Docs](https://github.com/Nauja/libws/actions/workflows/CI_docs.yml/badge.svg)](https://github.com/Nauja/libws/actions/workflows/CI_docs.yml)
[![Documentation Status](https://readthedocs.org/projects/libws/badge/?version=latest)](https://libws.readthedocs.io/en/latest/?badge=latest)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Nauja/libws/master/LICENSE)

Wrapper over [libwebsockets](https://libwebsockets.org/) for websockets.

## Why

WIP

## Examples

WIP

## Build Manually

Copy the files [ws.c](https://github.com/Nauja/libws/blob/main/ws.c) and [ws.h](https://github.com/Nauja/libws/blob/main/ws.h) into an existing project.

Comment or uncomment the defines at the top of `ws.h` depending on your configuration:

```c
/* Define to 1 if you have the <stdio.h> header file. */
#ifndef HAVE_STDIO_H
#define HAVE_STDIO_H 1
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#ifndef HAVE_STDLIB_H
#define HAVE_STDLIB_H 1
#endif

/* Define to 1 if you have the <sys/stat.h> header file. */
#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H 1
#endif

...
```

You should now be able to compile this library correctly.

## Build with CMake

Tested with CMake >= 3.13.4:

```
git clone https://github.com/Nauja/libws.git
cd libws
git submodule init
git submodule update
mkdir build
cd build
cmake ..
```

CMake will correctly configure the defines at the top of [ws.h](https://github.com/Nauja/libws/blob/main/ws.h) for your system.

You can then build this library manually as described above, or by using:

```
make
```

This will generate `libws.a` if building as a static library and `liblibws.so` in the `build` directory.

You can change the build process with a list of different options that you can pass to CMake. Turn them on with `On` and off with `Off`:
  * `-DLIBWS_STATIC=On`: Enable building as static library. (on by default)
  * `-DLIBWS_UNIT_TESTING=On`: Enable building the tests. (on by default)
  * `-DLIBWS_DOXYGEN=On`: Enable building the docs. (off by default)

## Build with Visual Studio

Generate the Visual Studio solution with:

```
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
```

You can now open `build/libws.sln` and compile the library.

## License

Licensed under the [MIT](https://github.com/Nauja/libws/blob/main/LICENSE) License.
