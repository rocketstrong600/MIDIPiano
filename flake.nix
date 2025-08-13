{
  description = "Basic rpi pico development shell";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
      };
      local-pico-sdk = pkgs.pico-sdk.override {
        withSubmodules = true;
      };
    in {
      devShells."${system}".default = pkgs.mkShell {
        packages = with pkgs; [
          cmake
          gcc-arm-embedded
          libusb1
          openocd
          local-pico-sdk
          picotool
          python3
          udisks
        ];
        shellHook = ''
          export PICO_SDK_PATH="${local-pico-sdk}/lib/pico-sdk"
          '';
        };
    };
}
