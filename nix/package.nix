{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  gtk4,
  curl,
  openssl,
  libuuid,
  json_c,
  libzip,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "microlauncher";
  version = "0.1";

  src = builtins.path {
    path = ../.;
    name = finalAttrs.pname;
  };


  strictDeps = true;
  nativeBuildInputs = [meson ninja pkg-config];
  buildInputs = [
    gtk4
    curl
    openssl
    libuuid
    json_c
    libzip
  ];

  installPhase = ''
    mkdir -p $out/bin
    install -Dm766 microlauncher $out/bin/
  '';
})
