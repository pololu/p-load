source $setup

cmake-cross $src \
  -DCMAKE_INSTALL_PREFIX=$out

make
make install

# TODO: Temporary workaround: nixcrpkgs provides a bad strip utility that makes the
# program fail at runtime.
if [ "$os" != "macos" ]; then
  echo stripping
  $host-strip $out/bin/*
fi

cp version.txt $out/
