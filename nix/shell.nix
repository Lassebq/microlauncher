{
  mkShell,
  gdb,
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
mkShell {
  name = "C";

  packages = [gdb];

  nativeBuildInputs = [meson ninja pkg-config];
  buildInputs = [
    gtk4
    curl
    openssl
    libuuid
    json_c
    libzip
  ];
}
