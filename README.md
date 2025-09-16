# microlauncher

MicroLauncher is a Minecraft launcher implementation for official game client JSONs which are hosted by Mojang at https://launchermeta.mojang.com/mc/game/version_manifest_v2.json

## Features
- Supports Windows, Linux, and macOS.
- By default MicroLauncher uses [BetterJSONs](https://github.com/MCPHackers/BetterJSONs) which bundle slightly different libraries for versions prior to 1.13 to provide better experience. `-lwjgl3` JSONs use [legacy-lwjgl3](https://github.com/MCPHackers/legacy-lwjgl3) to allow better Wayland support via glfw instead of using the broken LWJGL 2 implementation.

## Build dependencies
This project uses [CMake](https://cmake.org/).

### Arch Linux:
```
pacman -Sy gtk4 glib2 util-linux-libs json-c curl libzip openssl
```

### Fedora Linux:
```
sudo dnf install gtk4 json-c libzip gtk4-devel json-c-devel libzip-devel pciutils pciutils-devel
```

### MSYS2 with MinGW:
```
pacman -Sy mingw-w64-x86_64-gtk4 mingw-w64-x86_64-glib2 mingw-w64-x86_64-json-c mingw-w64-x86_64-curl-winssl mingw-w64-x86_64-libzip mingw-w64-x86_64-openssl
```

### macOS:
```
brew install gtk4 json-c libzip
```

## TODO

### GUI:
- Save java and display their version
- Implement java downloading from https://launchermeta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json or similar
- Other memory management fixes