{
  description = "Basic rpi pico development shell";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };
      pico-sdk-210 = pkgs.stdenv.mkDerivation rec {
        pname = "pico-sdk";
        version = "2.1.0";

        src = pkgs.fetchFromGitHub {
          owner = "raspberrypi";
          repo = "pico-sdk";
          rev = version;
          fetchSubmodules = true;
          hash = "sha256-nLn6H/P79Jbk3/TIowH2WqmHFCXKEy7lgs7ZqhqJwDM=";
        };

        nativeBuildInputs = [ pkgs.cmake ];

        # SDK contains libraries and build-system to develop projects for RP2040 chip
        # We only need to compile pioasm binary
        sourceRoot = "${src.name}/tools/pioasm";

        installPhase = ''
          runHook preInstall
          mkdir -p $out/lib/pico-sdk
          cp -a ../../../* $out/lib/pico-sdk/
          chmod 755 $out/lib/pico-sdk/tools/pioasm/build/pioasm
          runHook postInstall
        '';

        meta = with pkgs.lib; {
          homepage = "https://github.com/raspberrypi/pico-sdk";
          description = "SDK provides the headers, libraries and build system necessary to write programs for the RP2040-based devices";
          license = licenses.bsd3;
          maintainers = with maintainers; [ muscaln ];
          platforms = platforms.unix;
        };
      };

      picotool-210 = pkgs.stdenv.mkDerivation rec {
        pname = "picotool";
        version = "2.1.0";

        src = pkgs.fetchFromGitHub {
          owner = "raspberrypi";
          repo = "picotool";
          rev = version;
          hash = "sha256-aGhh19/dl6o/3hbmKJGVh22qSHeCqxST2PoWzxmc7KQ=";
        };

        postPatch = ''
          # necessary for signing/hashing support. our pico-sdk does not come with
          # it by default, and it shouldn't due to submodule size. pico-sdk uses
          # an upstream version of mbedtls 2.x so we patch ours in directly.
          substituteInPlace lib/CMakeLists.txt \
          --replace-fail "''$"'{PICO_SDK_PATH}/lib/mbedtls' '${pkgs.mbedtls_2.src}'
        '';

        buildInputs = [
          pkgs.libusb1
          pico-sdk-210
        ];
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.pkg-config
        ];
        cmakeFlags = [ "-DPICO_SDK_PATH=${pico-sdk-210}/lib/pico-sdk" ];

        postInstall = ''
          install -Dm444 ../udev/99-picotool.rules -t $out/etc/udev/rules.d
        '';

        meta = with pkgs.lib; {
          homepage = "https://github.com/raspberrypi/picotool";
          description = "Tool for interacting with RP2040/RP2350 device(s) in BOOTSEL mode, or with an RP2040/RP2350 binary";
          mainProgram = "picotool";
          license = licenses.bsd3;
          maintainers = with maintainers; [ muscaln ];
          platforms = platforms.unix;
        };
      };
    in {
      devShell = pkgs.mkShell {
        buildInputs = with pkgs; [
          cmake
          gcc-arm-embedded
          libusb1
          openocd
          pico-sdk-210
          picotool-210
          python3Full
        ];
        shellHook = with pkgs; ''
          export PICO_SDK_PATH="${pico-sdk-210}/lib/pico-sdk"
          '';
        };
    });
}
