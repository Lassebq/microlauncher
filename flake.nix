{
  description = "Microlauncher NixOS flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
    
    # Обновили названия пакетов X11, чтобы NixOS не сыпал варнингами
    runtimeLibs = with pkgs; [
      libGL
      vulkan-loader
      wayland
      libX11
      libXext
      libXcursor
      libXrandr
      libXxf86vm
      glfw
      openal
    ];
  in {
    packages.${system}.default = pkgs.stdenv.mkDerivation {
      pname = "microlauncher";
      version = "git";

      src = ./.;

      nativeBuildInputs = with pkgs; [
        cmake
        pkg-config
        wrapGAppsHook4
      ];

      buildInputs = with pkgs; [
        curl
        glib
        gtk4
        imagemagick
        json_c
        libzip
        openssl
        pciutils
        # === НОВЫЕ ПАКЕТЫ ИЗ ОШИБКИ СБОРКИ ===
        webkitgtk_6_0
        util-linux # Именно в util-linux в NixOS лежит библиотека uuid
      ];

      preFixup = ''
        gappsWrapperArgs+=(
          --prefix PATH : "${pkgs.jdk21}/bin"
          --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath runtimeLibs}"
        )
      '';
    };
  };
}
