# This shell script updates the copy of the tinyxml2 library in this directory.
# Should set the version number below to the desired version.
VER=3.0.0
wget https://raw.githubusercontent.com/leethomason/tinyxml2/$VER/tinyxml2.{h,cpp}
