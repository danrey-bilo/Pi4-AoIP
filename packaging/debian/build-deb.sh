#!/bin/sh
set -eu
umask 022
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build="$root/build/package"
output=${1:-"$root/dist"}
aoip=${AOIP_SOURCE_DIR:-"$root/external/AoIP-lib"}
version=2.1.0-1
test "$(dpkg --print-architecture)" = arm64 || { echo 'Build on Debian 13 ARM64' >&2; exit 2; }
test -f "$aoip/CMakeLists.txt" || { echo 'Initialize external/AoIP-lib or set AOIP_SOURCE_DIR' >&2; exit 2; }
mkdir -p "$build" "$output"
output=$(CDPATH= cd -- "$output" && pwd)
cmake -S "$root" -B "$build/cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib \
  -DAOIP_SOURCE_DIR="$aoip" -DAOIP_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build "$build/cmake" --parallel 2
ctest --test-dir "$build/cmake" --output-on-failure --timeout 30
sdkprefix=$(mktemp -d "$build/install.XXXXXX")
cmake --install "$build/cmake" --prefix "$sdkprefix"
stage=$(mktemp -d "$build/runtime.XXXXXX")
chmod 0755 "$stage"
install -D -m 0755 "$build/cmake/bin/aoip_peer_rpi4" "$stage/usr/lib/piaoip/aoip_peer_rpi4"
install -D -m 0644 "$root/packaging/debian/pi-aoip.service" "$stage/usr/lib/systemd/system/pi-aoip.service"
install -D -m 0644 "$root/packaging/debian/peer.conf" "$stage/etc/piaoip/peer.conf"
for tool in piaoip-configure piaoip-doctor; do
  install -D -m 0755 "$root/packaging/debian/$tool" "$stage/usr/bin/$tool"
done
install -D -m 0644 "$aoip/tools/control.py" "$stage/usr/share/piaoip/control.py"
install -d "$stage/usr/share/doc/piaoip-rpi4" "$stage/DEBIAN" "$build/debian"
printf '64 64 192000 32 5\n' > "$stage/usr/share/piaoip/default-profile.txt"
install -m 0644 "$root/docs/BUILD.md" "$stage/usr/share/doc/piaoip-rpi4/INSTALL-RU.md"
install -m 0644 "$root/docs/API.md" "$stage/usr/share/doc/piaoip-rpi4/PLATFORM-API.md"
install -m 0644 "$root/LICENSE" "$stage/usr/share/doc/piaoip-rpi4/copyright"
cat > "$build/debian/control" <<'EOF'
Source: piaoip-rpi4
Section: sound
Priority: optional
Maintainer: danrey-bilo <80554517+danrey-bilo@users.noreply.github.com>

Package: piaoip-rpi4
Architecture: arm64
Description: PiAoIP Raspberry Pi 4 RT audio transport
EOF
depends=$(cd "$build" && dpkg-shlibdeps -O -e"$stage/usr/lib/piaoip/aoip_peer_rpi4" | sed -n 's/^shlibs:Depends=//p')
cat > "$stage/DEBIAN/control" <<EOF
Package: piaoip-rpi4
Version: $version
Architecture: arm64
Maintainer: danrey-bilo <80554517+danrey-bilo@users.noreply.github.com>
Section: sound
Priority: optional
Depends: $depends, adduser, init-system-helpers, systemd, python3
Homepage: https://github.com/danrey-bilo/Pi4-AoIP
Installed-Size: $(du -sk "$stage/usr" "$stage/etc" | awk '{n+=$1} END {print n}')
Description: PiAoIP audio transport for Raspberry Pi 4 PREEMPT_RT
 Synthetic UDP PCM peer up to 64 channels per direction and 192 kHz PCM32.
 Requires Pi 4 Model B, PREEMPT_RT and Debian 13 ARM64.
 Network threads use CPU0 and CPU1; CPU2 and CPU3 remain reserved for effects.
 Personal noncommercial use is free; commercial use requires a paid license.
EOF
printf '/etc/piaoip/peer.conf\n' > "$stage/DEBIAN/conffiles"
for script in preinst postinst prerm postrm; do
  install -m 0755 "$root/packaging/debian/$script" "$stage/DEBIAN/$script"
  sh -n "$stage/DEBIAN/$script"
done
(cd "$stage" && find usr etc -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums)
dpkg-deb --root-owner-group --build "$stage" "$output/piaoip-rpi4_${version}_arm64.deb"
sdkstage=$(mktemp -d "$build/sdk.XXXXXX")
chmod 0755 "$sdkstage"
install -d "$sdkstage/usr" "$sdkstage/DEBIAN" "$sdkstage/usr/share/doc/piaoip-sdk"
cp -a "$sdkprefix/include" "$sdkprefix/lib" "$sdkprefix/share" "$sdkstage/usr/"
cp "$root/docs/API.md" "$sdkstage/usr/share/doc/piaoip-sdk/PLATFORM.md"
cp "$aoip/docs/CORE-API.md" "$sdkstage/usr/share/doc/piaoip-sdk/CORE.md"
cp "$aoip/docs/PEER-API.md" "$sdkstage/usr/share/doc/piaoip-sdk/PEER.md"
cat > "$sdkstage/DEBIAN/control" <<EOF
Package: piaoip-sdk
Version: $version
Architecture: arm64
Maintainer: danrey-bilo <80554517+danrey-bilo@users.noreply.github.com>
Section: libdevel
Priority: optional
Depends: libstdc++-14-dev, libc6-dev
Description: AoIP core, peer and Raspberry Pi 4 static libraries
 C++17 headers, archives and CMake targets AoIP::core, AoIP::peer,
 and Pi4AoIP::platform. Personal use free; commercial license required.
EOF
dpkg-deb --root-owner-group --build "$sdkstage" "$output/piaoip-sdk_${version}_arm64.deb"
dpkg-deb --info "$output/piaoip-rpi4_${version}_arm64.deb"
