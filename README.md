# SO

Process scheduler with a client-server architecture, developed for our Operating Systems class.
See [the full project requirements](Requirements.pdf) (in Portuguese) for more information.

## Building

### Dependencies

- [GNU Make](https://www.gnu.org/software/make/) (build-time);
- [GCC](https://www.gnu.org/software/gcc/) (build-time).

Our focus is in supporting GCC + Linux, though other compilers *may* work. However, in terms of
other operating systems, because the purpose of this project is to use system calls directly, there
can be some Linux exclusive functionality.

### Building

This project can be built with:

```console
$ make
```

Build artifacts can be removed with:

```console
$ make clean
```

## Installation

After [building](#building), the program can be installed by running:

```console
$ make install
```

`$PREFIX` can be overridden, to install the program in another location:

```console
# PREFIX=/usr make install
```

The program can also be uninstalled. Just make sure you use the same `$PREFIX` you used for
installation:

```console
$ make uninstall
```

## Developers

As a university project, external contributors aren't allowed.
All contributors must read the [DEVELOPERS.md](DEVELOPERS.md) guide.
