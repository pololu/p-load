require 'fileutils'
require 'pathname'
include FileUtils

ENV['PATH'] = ENV.fetch('_PATH')

ConfigName = ENV.fetch('config_name')
OutDir = Pathname(ENV.fetch('out'))
PayloadDir = Pathname(ENV.fetch('payload'))
SrcDir = Pathname(ENV.fetch('src'))
Version = File.read(PayloadDir + 'version.txt')
StagingDir = OutDir + 'release'
AppDir = StagingDir + 'Pololu USB Bootloader Utility'
BinDir = AppDir + 'bin'
PathDir = OutDir + 'path'
ResDir = OutDir + 'resources'

mkdir_p BinDir
mkdir_p PathDir
mkdir_p ResDir

cp_r Dir.glob(PayloadDir + 'bin' + '*'), BinDir
cp ENV.fetch('license'), StagingDir + 'LICENSE.html'

File.open(OutDir + 'distribution.xml', 'w') do |f|
  f.print <<EOF
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<installer-gui-script minSpecVersion="1">
  <title>Pololu USB Bootloader Utility #{Version}</title>
  <welcome file="welcome.html" />
  <pkg-ref id="app">app.pkg</pkg-ref>
  <pkg-ref id="path">path.pkg</pkg-ref>
  <options customize="allow" require-scripts="false" rootVolumeOnly="true" />
  <choices-outline>
    <line choice="app" />
    <line choice="path" />
  </choices-outline>
  <choice id="app" visible="false">
    <pkg-ref id="app" />
  </choice>
  <choice id="path" title="Add bin directory to the PATH"
    description="Adds an entry to /etc/paths.d/ so you can run p-load from a terminal easily.">
    <pkg-ref id="path" />
  </choice>
  <volume-check>
    <allowed-os-versions>
      <os-version min="10.11" />
    </allowed-os-versions>
  </volume-check>
</installer-gui-script>
EOF
end

File.open(ResDir + 'welcome.html', 'w') do |f|
  f.print <<EOF
<!DOCTYPE html>
<html>
<style>
body { font-family: sans-serif; }
</style>
<title>Welcome</title>
<p>
This package installs the Pololu USB Bootloader Utility (p-load)
version #{Version} on your computer.
EOF
end

File.open(PathDir + '99-p-load', 'w') do |f|
  f.puts '/Applications/Pololu USB Bootloader Utility/bin'
end

File.open(OutDir + 'build.sh', 'w') do |f|
  f.puts <<EOF
#!/bin/bash
set -ue

strip "release/Pololu USB Bootloader Utility/bin/p-load"

pkgbuild \
  --identifier com.pololu.p-load.app \
  --version "#{Version}" \
  --root "release/" \
  --install-location /Applications \
  app.pkg

pkgbuild \
  --identifier com.pololu.p-load.path \
  --version "#{Version}" \
  --root "path/" \
  --install-location /etc/paths.d \
  path.pkg

productbuild \
  --identifier com.pololu.p-load \
  --version "#{Version}" \
  --resources "resources/" \
  --distribution distribution.xml \
  "p-load-#{Version}-mac.pkg"
EOF
end

chmod 'u+x', OutDir + 'build.sh'
