{ pkgs ? import <nixpkgs> {} }:

let
  llvm = pkgs.llvmPackages_22;
in
pkgs.mkShell.override { stdenv = llvm.libcxxStdenv; } {
  name = "kumi";

  packages = with pkgs; [
    cmake
    ninja
    mold
    ccache
    llvm.clang
    llvm.clang-tools
    llvm.lldb
    fish
  ];

  shellHook = ''
    export CC="${llvm.clang}/bin/clang"
    export CXX="${llvm.clang}/bin/clang++"
    export LIBCXX_PREFIX="${llvm.libcxx}"
    [[ $- == *i* ]] && exec fish
  '';
}
