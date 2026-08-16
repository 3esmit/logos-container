{
  description = "Logos Module Container interface";

  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
  };

  outputs = { self, nixpkgs, logos-nix }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      # "x86_64-windows" pseudo-system: a cross derivation's `system` is its
      # BUILD platform, so it evaluates anywhere and realises on x86_64-linux.
      packages = logos-nix.lib.forAllTargets ({ pkgs, system, ... }:
        let
          # Common configuration
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;

          # Shared build that compiles everything
          build = import ./nix/build.nix { inherit pkgs common src; };

          # Header-only contract: the package is just the installed headers.
          include = import ./nix/include.nix { inherit pkgs common src; };
          tests = import ./nix/tests.nix { inherit pkgs common build; };

          # Combined package (headers only)
          logos-container = pkgs.symlinkJoin {
            name = "logos-container";
            paths = [ include ];
          };
        in
        {
          # Individual outputs
          logos-container-include = include;
          logos-container-tests = tests;

          # Combined output
          logos-container = logos-container;

          # Default package
          default = logos-container;
        }
      );

      checks = forAllSystems ({ pkgs, system }:
        let
          testsPkg = self.packages.${system}.logos-container-tests;
        in
        {
          tests = pkgs.runCommand "logos-container-tests" {
            nativeBuildInputs = [ testsPkg ];
          } ''
            echo "Running logos-container tests..."
            ${testsPkg}/bin/logos_container_tests
            mkdir -p $out
            touch $out/.tests-passed
          '';
        }
      );

      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.nlohmann_json
            pkgs.gtest
          ];
        };
      });
    };
}
