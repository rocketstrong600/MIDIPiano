{
  description = "Basic rpi pico development shell";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
    let
      pico-sdk210 = with pkgs; (pico-sdk.overrideAttrs (o:
        rec {
        pname = "pico-sdk";
        version = "2.1.0";
        src = fetchFromGitHub {
          fetchSubmodules = true;
          owner = "raspberrypi";
          repo = pname;
          rev = version;
          sha256 = "sha256-Yf3cVFdI8vR/qcxghcx8fsbYs1qD8r6O5aqZ0AkSLDc=";
        };
        }));
      pkgs = nixpkgs.legacyPackages.${system};
    in {
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            gcc-arm-embedded
            libusb1
            openocd
            pico-sdk210
            picotool
            ];
          shellHook = with pkgs; ''
            export PICO_SDK_PATH="${pico-sdk210}/lib/pico-sdk"
            '';
          };
        });
}
