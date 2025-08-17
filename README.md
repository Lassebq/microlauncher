# microlauncher

MicroLauncher is a Minecraft launcher implementation for official game client JSONs which are hosted by Mojang at https://launchermeta.mojang.com/mc/game/version_manifest_v2.json

## Features
- Currently only linux is supported
- By default MicroLauncher uses [BetterJSONs](https://github.com/MCPHackers/BetterJSONs) which bundle slightly different libraries for versions prior to 1.13 to provide better experience. `-lwjgl3` JSONs use [legacy-lwjgl3](https://github.com/MCPHackers/legacy-lwjgl3) to allow better Wayland support via glfw instead of using the broken LWJGL 2 implementation.

## TODO

### JSON: 
- Support `arguments` array instead of relying on `minecraftArguments` string

### GUI:
- Instance sorting (manual and automatic)
- DnD support (manual sorting)
- Use GObject for instance and account rows.
- Figure out why right click context menu doesn't get fully destroyed?

### Other: 
- Creation of instance shortcuts (to launch instances directly from app launcher)