rec {
  nixcrpkgs = import <nixcrpkgs>;

  src = nixcrpkgs.filter ./.;

  build = env: env.make_derivation {
    builder = ./nix/builder.sh;
    inherit src;
    cross_inputs = [ env.libusbp ];
  };

  win32 = build nixcrpkgs.win32;
  linux-x86 = build nixcrpkgs.linux-x86;
  linux-rpi = build nixcrpkgs.linux-rpi;
}
