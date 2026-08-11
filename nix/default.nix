# Common build configuration shared across all packages
{ pkgs }:

{
  pname = "logos-container";
  version = "0.1.0";

  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
  ];

  buildInputs = [
    pkgs.nlohmann_json
    pkgs.gtest
  ];

  cmakeFlags = [
    "-GNinja"
  ];

  meta = with pkgs.lib; {
    description = "Container contract: The Logos ModuleContainer interface, the module descriptor value types it exchanges, and the shared module-name allowlist";
    platforms = platforms.unix ++ platforms.windows;
  };
}
