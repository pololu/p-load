require 'fileutils'
require 'pathname'
include FileUtils

ENV['PATH'] = ENV.fetch('_PATH')

ConfigName = ENV.fetch('config_name')
OutDir = Pathname(ENV.fetch('out'))
PayloadDir = Pathname(ENV.fetch('payload'))
SrcDir = Pathname(ENV.fetch('src'))
Version = File.read(PayloadDir + 'version.txt')
TarName = "pololu-tic-#{Version}-#{ConfigName}"
StagingDir = Pathname(TarName)
OutTar = OutDir + "#{TarName}.tar.xz"

mkdir_p StagingDir
cp_r Dir.glob(PayloadDir + 'bin' + '*'), StagingDir
cp_r SrcDir + 'udev-rules' + '99-pololu.rules', StagingDir
cp ENV.fetch('license'), StagingDir + 'LICENSE.html'

File.open(StagingDir + 'README.txt', 'w') do |f|
  f.puts <<EOF
Pololu USB Bootloader Utility (p-load) #{Version} for #{ConfigName}

To install this software, we recommend starting a terminal, navigating to this
directory using the "cd" command, and then running:

  sudo ./install.sh

The install.sh script will install files to the following locations on your
system:

  /usr/local/bin/p-load
  /etc/udev/rules.d/99-pololu.rules

For more information, see:

  https://github.com/pololu/p-load
EOF
end

File.open(StagingDir + 'install.sh', 'w') do |f|
  f.puts <<EOF
#!/usr/bin/env sh
set -ue
cd "$(dirname "$0")"
cp -v p-load /usr/local/bin/
cp -v 99-pololu.rules /etc/udev/rules.d/
EOF
end

chmod_R 'u+w', StagingDir
chmod 'u+x', StagingDir + 'install.sh'

mkdir_p OutDir
success = system("tar cJfv #{OutTar} #{TarName}")
raise "tar failed: error #{$?.exitstatus}" if !success
