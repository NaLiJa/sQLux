#! /bin/bash

set -e

ARCH=`arch`

if [ "$ARCH" != "x86_64" ]; then
    echo "Script assumes x86_64 host for building, exiting..."
fi

# Build the x86_64 linux version
cp -r sqlux sqlux-x86_64
pushd sqlux-x86_64
git clean -xfd
cmake -B x86_64/ -DCMAKE_BUILD_TYPE=Release
cmake --build x86_64/
cp x86_64/sqlux /build/release/sqlux_x86_64
popd

# Build the raspian version
cp -r sqlux /raspbian/sqlux
pushd /raspbian/sqlux
git clean -xfd
popd

chroot /raspbian /bin/bash -x <<'EOF'
cd sqlux
cmake -B armv6/ -DCMAKE_BUILD_TYPE=Release
cmake --build armv6/
EOF

cp /raspbian/sqlux/armv6/sqlux /build/release/sqlux_armv6

# Build the armv7 version
cp -r sqlux /bookworm32/sqlux
pushd /bookworm32/sqlux
git clean -xfd
popd

chroot /bookworm32 /bin/bash -x <<'EOF'
cd sqlux
cmake -B armv7/ -DCMAKE_BUILD_TYPE=Release
cmake --build armv7/
EOF

cp /bookworm32/sqlux/armv7/sqlux /build/release/sqlux_armv7

# Build the arm64 version
cp -r sqlux /bookworm/sqlux
pushd /bookworm/sqlux
git clean -xfd
popd

chroot /bookworm /bin/bash -x <<'EOF'
cd sqlux
cmake -B arm64/ -DCMAKE_BUILD_TYPE=Release
cmake --build arm64/
EOF

cp /bookworm/sqlux/arm64/sqlux /build/release/sqlux_arm64

# Build the Windows 64bit version
(
export WINEPREFIX=/root/.wine64
export WINEARCH=win64
export MINGW_CC=x86_64-w64-mingw32-gcc

# temp turn off exit on error to stop error on sudo missing

set +e
source /quasi-msys2-w64/env/all.src
set -e

cp -r sqlux sqlux-w64
pushd sqlux-w64
git clean -xfd
cmake -B w64 -DCMAKE_BUILD_TYPE=Release
cmake --build w64
cp w64/sqlux.exe /build/release/sqlux_w64.exe
popd
)

# Build the Windows 32bit version
(
export WINEPREFIX=/root/.wine32
export WINEARCH=win32
export MINGW_CC=i686-w64-mingw32-gcc

# temp turn off exit on error to stop error on sudo missing

set +e
source /quasi-msys2-w32/env/all.src
set -e

cp -r sqlux sqlux-w32
pushd sqlux-w32
git clean -xfd
cmake -B w32 -DCMAKE_BUILD_TYPE=Release
cmake --build w32
cp w32/sqlux.exe /build/release/sqlux_w32.exe
popd
)

pushd sqlux
VERSION=`git describe --tags --always`
popd

mkdir sqlux-$VERSION

cat > sqlux-$VERSION/sqlux.ini<< EOF
SYSROM = MIN198.rom
ROMIM = TK232.rom
RAMTOP = 4096
FAST_STARTUP = 1
DEVICE = FLP1,198ad.img,qdos-native
BOOT_DEVICE = FLP1
WIN_SIZE = max
FILTER = 1
FIXASPECT = 1
EOF

# Download vanpeebles game as example program
curl -O https://dilwyn.qlforum.co.uk/games/adventures/198-adventure.zip
unzip 198-adventure.zip "198adDisk ImageFinal.img"
mv "198adDisk ImageFinal.img" sqlux-$VERSION/198ad.img
rm 198-adventure.zip

# copy the roms and docs into release
cp -r sqlux/roms sqlux-$VERSION/
cp -r sqlux/docs sqlux-$VERSION/

# copy the binaries into release
cp release/sqlux_x86_64 sqlux-$VERSION/sqlux_x86_64
cp release/sqlux_armv6 sqlux-$VERSION/sqlux_armv6
cp release/sqlux_armv7 sqlux-$VERSION/sqlux_armv7
cp release/sqlux_arm64 sqlux-$VERSION/sqlux_arm64
cp release/sqlux_w64.exe sqlux-$VERSION/sqlux_w64.exe
cp release/sqlux_w32.exe sqlux-$VERSION/sqlux_w32.exe

# zip the release
zip -r sqlux-$VERSION.zip sqlux-$VERSION/

rm -r sqlux-$VERSION/

mv sqlux-$VERSION.zip release/

