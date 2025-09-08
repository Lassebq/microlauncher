# microlauncher

MicroLauncher is a Minecraft launcher implementation for official game client JSONs which are hosted by Mojang at https://launchermeta.mojang.com/mc/game/version_manifest_v2.json

## Features
- Supports Windows and Linux
- By default MicroLauncher uses [BetterJSONs](https://github.com/MCPHackers/BetterJSONs) which bundle slightly different libraries for versions prior to 1.13 to provide better experience. `-lwjgl3` JSONs use [legacy-lwjgl3](https://github.com/MCPHackers/legacy-lwjgl3) to allow better Wayland support via glfw instead of using the broken LWJGL 2 implementation.

## Build dependencies
For building this project it is recommended to use CMake. Meson support is to be removed.

### Arch Linux:
```
pacman -Sy gtk4 glib2 util-linux-libs json-c curl libzip openssl
```

### MSYS2 with MinGW
```
pacman -Sy mingw-w64-x86_64-gtk4 mingw-w64-x86_64-glib2 mingw-w64-x86_64-json-c mingw-w64-x86_64-curl-winssl mingw-w64-x86_64-libzip mingw-w64-x86_64-openssl
```

## TODO

### GUI:
- Use GObject for instance and account rows.
- Figure out why right click context menu doesn't get fully destroyed?