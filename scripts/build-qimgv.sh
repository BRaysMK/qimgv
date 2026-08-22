#!/bin/bash
# This builds a complete qimgv-x64 package. Result is placed in qimgv/qimgv-x64_<version>
# Warning: Some stuff will be left over behind after building (C:/qt and C:/opencv-4.5.5-minimal)

#CFL='-ffunction-sections -fdata-sections -march=native -mtune=native -O3 -pipe'
CFL='-ffunction-sections -fdata-sections -O3 -pipe'
LDFL='-Wl,--gc-sections'

if [[ -z "$RUNNER_TEMP" ]]; then
  MSYS_DIR="C:/msys64/mingw64"
else
  MSYS_DIR="$(cd "$RUNNER_TEMP" && pwd)/msys64/mingw64"
fi
CUSTOM_QT_DIR="C:/qt/5.15.3-mingw64-slim"
OPENCV_DIR="C:/opencv-minimal-4.5.5"
SCRIPTS_DIR=$(dirname $(readlink -f $0)) # this file's location (/path/to/qimgv/scripts)
SRC_DIR=$(dirname $SCRIPTS_DIR)
BUILD_DIR=$SRC_DIR/build
EXT_DIR=$SRC_DIR/_external
rm -rf "$EXT_DIR"
mkdir "$EXT_DIR"
MPV_DIR=$EXT_DIR/mpv

# ------------------------------------------------------------------------------
echo "PREPARING BUILD DIR"
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# ------------------------------------------------------------------------------
echo "UPDATING DEPENDENCY LIST"
wget --progress=dot:mega -O $BUILD_DIR/msys2-build-deps.txt https://raw.githubusercontent.com/easymodo/qimgv-deps-bin/main/msys2-build-deps.txt
wget --progress=dot:mega -O $BUILD_DIR/msys2-dll-deps.txt https://raw.githubusercontent.com/easymodo/qimgv-deps-bin/main/msys2-dll-deps.txt

# ------------------------------------------------------------------------------
echo "INSTALLING MSYS2 BUILD DEPS"
# exiv2 is no longer packaged in the mingw64 MSYS2 repo (it moved to
# ucrt64/clang64). It is filtered out here and built from source below.
MSYS_DEPS=$(grep -v '^mingw-w64-x86_64-exiv2' $BUILD_DIR/msys2-build-deps.txt | tr '\n' ' ')
pacman -S $MSYS_DEPS --noconfirm --needed

# ------------------------------------------------------------------------------
echo "GETTING Qt"
mkdir C:/qt
cd C:/qt
wget --progress=dot:mega -O 5.15.3-mingw64-slim.7z https://github.com/easymodo/qt-builds/releases/download/5.15.3-mingw64-slim/5.15.3-mingw64-slim.7z
7z x 5.15.3-mingw64-slim.7z -y
rm 5.15.3-mingw64-slim.7z

# ------------------------------------------------------------------------------
echo "GETTING OpenCV"
mkdir $OPENCV_DIR
cd $OPENCV_DIR
wget --progress=dot:mega -O opencv-minimal-4.5.5-x64.7z https://github.com/easymodo/qimgv-deps-bin/releases/download/x64/opencv-minimal-4.5.5-x64.7z
7z x opencv-minimal-4.5.5-x64.7z -y
rm opencv-minimal-4.5.5-x64.7z

# ------------------------------------------------------------------------------
echo "GETTING MPV"
mkdir $MPV_DIR
cd $MPV_DIR
wget --progress=dot:mega -O mpv-x64.7z https://github.com/easymodo/qimgv-deps-bin/releases/download/x64/mpv-x86_64-20230402-git-0f13c38.7z
7z x mpv-x64.7z -y
rm mpv-x64.7z

# ------------------------------------------------------------------------------
echo "BUILDING EXIV2 (not available in the mingw64 repo; building from source)"
# v0.27.x: matches the API qimgv was written against (open(std::wstring));
# EXIV2_ENABLE_WIN_UNICODE=ON is required or the std::wstring overload is not built
# XMP support (EXIV2_ENABLE_XMP, default ON) needs the bundled xmp-sdk
# submodule, hence --recurse-submodules; it is used to read DJI flight data
# (Xmp.drone-dji.*).
cd $EXT_DIR
git clone --depth 1 --branch v0.27.7 --recurse-submodules https://github.com/Exiv2/exiv2.git
cd exiv2
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MSYS_DIR \
    -DEXIV2_BUILD_SAMPLES=OFF \
    -DEXIV2_BUILD_EXIV2_COMMAND=OFF \
    -DEXIV2_BUILD_DOC=OFF \
    -DEXIV2_BUILD_UNIT_TESTS=OFF \
    -DEXIV2_ENABLE_BMFF=OFF \
    -DEXIV2_ENABLE_VIDEO=OFF \
    -DEXIV2_ENABLE_PNG=OFF \
    -DEXIV2_ENABLE_WEBREADY=OFF \
    -DEXIV2_ENABLE_EXTERNAL_XMP=OFF \
    -DEXIV2_ENABLE_CURL=OFF \
    -DEXIV2_ENABLE_SSH=OFF \
    -DEXIV2_ENABLE_INIH=OFF \
    -DEXIV2_ENABLE_BROTLI=OFF \
    -DEXIV2_ENABLE_WIN_UNICODE=ON
ninja -C build
ninja -C build install

# ------------------------------------------------------------------------------
# We are using prebuilt opencv but feel free to compile instead
#echo "BUILDING OPENCV"
#cd $EXT_DIR
#git clone --depth 1 --branch 4.5.5 https://github.com/opencv/opencv.git
#cd opencv
#rm -rf ./build
#cmake -S ./ -B build -G Ninja \
#    -DCMAKE_INSTALL_PREFIX="$OPENCV_DIR" \
#    -DCMAKE_BUILD_TYPE=Release \
#    -DBUILD_LIST='core,imgproc' \
#    -DWITH_1394=OFF \
#    -DWITH_VTK=OFF \
#    -DWITH_FFMPEG=OFF \
#    -DWITH_GSTREAMER=OFF \
#    -DWITH_DSHOW=OFF \
#    -DWITH_QUIRC=OFF \
#    -DBUILD_SHARED_LIBS=ON \
#    -DCMAKE_C_FLAGS="$CFL" -DCMAKE_CXX_FLAGS="$CFL" -DCMAKE_EXE_LINKER_FLAGS="$LDFL"
#ninja install -C build

# ------------------------------------------------------------------------------
echo "BUILDING"
#rm -rf $BUILD_DIR
sed -i 's|opencv4/||' $SRC_DIR/qimgv/3rdparty/QtOpenCV/cvmatandqimage.{h,cpp}
cmake -S $SRC_DIR -B $BUILD_DIR -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR \
    -DOpenCV_DIR=$OPENCV_DIR \
    -DOPENCV_SUPPORT=ON \
    -DVIDEO_SUPPORT=ON \
    -DMPV_DIR=$MPV_DIR \
    -DCMAKE_CXX_FLAGS="$CFL" -DCMAKE_EXE_LINKER_FLAGS="$LDFL"
ninja -C $BUILD_DIR

# Fail fast: a broken qimgv build must abort instead of producing a package
# without qimgv.exe (that used to get uploaded as a release).
# Note: we deliberately do NOT use 'set -e' here - the mingw32-make steps for
# some imageformat plugins return non-zero because the slim Qt lacks the
# "testlib" module, even though the plugin DLLs build fine.
test -f $BUILD_DIR/qimgv/qimgv.exe || { echo "ERROR: qimgv build failed - qimgv.exe not found"; exit 1; }

# ------------------------------------------------------------------------------
echo "BUILDING IMAGEFORMATS"
# We are using libjxl from MSYS2 but feel free to compile instead
#cd $EXT_DIR
#git clone --depth 1 https://github.com/libjxl/libjxl.git --recursive
#cd libjxl
#rm -rf build
#cmake -S . -B build -G "Ninja" \
#    -DCMAKE_INSTALL_PREFIX=$MSYS_DIR \
#    -DCMAKE_BUILD_TYPE=Release -DJPEGXL_ENABLE_PLUGINS=OFF \
#    -DBUILD_TESTING=OFF -DJPEGXL_WARNINGS_AS_ERRORS=OFF \
#    -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF \
#    -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_MANPAGES=OFF \
#    -DJPEGXL_FORCE_SYSTEM_BROTLI=ON
#ninja install -C build

# qt-jpegxl-image-plugin
cd $EXT_DIR
git clone --depth 1 https://github.com/novomesk/qt-jpegxl-image-plugin.git
cd qt-jpegxl-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_MAJOR_VERSION=5 \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# qt-avif-image-plugin
cd $EXT_DIR
git clone https://github.com/novomesk/qt-avif-image-plugin
cd qt-avif-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_MAJOR_VERSION=5 \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# QtApng
cd $EXT_DIR
git clone https://github.com/Skycoder42/QtApng.git
cd QtApng
rm -rf build
mkdir build && cd build
$CUSTOM_QT_DIR/bin/qmake.exe ..
mingw32-make -j4

# qt-heif-image-plugin
cd $EXT_DIR
git clone https://github.com/jakar/qt-heif-image-plugin.git
cd qt-heif-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# qtraw
cd $EXT_DIR
git clone https://gitlab.com/mardy/qtraw
cd qtraw
rm -rf build
mkdir build && cd build
$CUSTOM_QT_DIR/bin/qmake.exe .. DEFINES+="LIBRAW_WIN32_CALLS=1"
mingw32-make -j4

# ------------------------------------------------------------------------------
echo "PACKAGING"
# 0 - prepare dir
cd $SRC_DIR
BUILD_NAME=qimgv-x64_$(git describe --tags)
PACKAGE_DIR=$SRC_DIR/$BUILD_NAME
rm -rf $PACKAGE_DIR
mkdir $PACKAGE_DIR

# 1 - copy qimgv build
cp $BUILD_DIR/qimgv/qimgv.exe $PACKAGE_DIR
mkdir $PACKAGE_DIR/plugins
cp $BUILD_DIR/plugins/player_mpv/player_mpv.dll $PACKAGE_DIR/plugins
cp -r $BUILD_DIR/qimgv/translations/ $PACKAGE_DIR/

# 2 - copy qt dlls
cd $CUSTOM_QT_DIR/bin
cp Qt5Core.dll Qt5Gui.dll Qt5PrintSupport.dll Qt5Svg.dll Qt5Widgets.dll $PACKAGE_DIR
cd $CUSTOM_QT_DIR/plugins
cp -r iconengines imageformats printsupport styles $PACKAGE_DIR
mkdir $PACKAGE_DIR/platforms
cp platforms/qwindows.dll $PACKAGE_DIR/platforms

# 3 - copy msys dlls
MSYS_DLLS=$(grep -v '^libexiv2' $BUILD_DIR/msys2-dll-deps.txt | tr '\n' ' ')
cd $MSYS_DIR/bin
cp $MSYS_DLLS $PACKAGE_DIR
# exiv2 was built from source; copy its runtime dll (MinGW names it libexiv2.dll)
cp $MSYS_DIR/bin/libexiv2.dll $PACKAGE_DIR

# 4 - copy imageformats
cp $EXT_DIR/qt-jpegxl-image-plugin/build/bin/imageformats/libqjpegxl5.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qt-avif-image-plugin/build/bin/imageformats/libqavif5.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/QtApng/build/plugins/imageformats/qapng.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qt-heif-image-plugin/build/bin/imageformats/libqheif.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qtraw/build/src/imageformats/qtraw.dll $PACKAGE_DIR/imageformats

# 5 - copy opencv & mpv
cd $OPENCV_DIR/x64/mingw/bin
cp libopencv_core455.dll libopencv_imgproc455.dll $PACKAGE_DIR
cd $MPV_DIR/bin/x86_64
cp mpv.exe libmpv-2.dll $PACKAGE_DIR

# 6 - misc
mkdir $PACKAGE_DIR/cache
mkdir $PACKAGE_DIR/conf
mkdir $PACKAGE_DIR/thumbnails
cp -r $SRC_DIR/qimgv/distrib/mimedata/data $PACKAGE_DIR

# Copy every runtime dependency reported by ldd (covers stale dll lists and
# version bumps in the msys2 packages). Runs after ALL copies so plugin dlls
# are included; repeats until stable so transitive deps are picked up too.
for pass in 1 2 3 4 5 6 7 8; do
    added=0
    for f in $(find $PACKAGE_DIR -name "*.dll" -o -name "*.exe"); do
        for d in $(ldd "$f" 2>/dev/null | grep '=>' | awk '{print $1}'); do
            if [ -f "$MSYS_DIR/bin/$d" ] && [ ! -f "$PACKAGE_DIR/$d" ]; then
                cp "$MSYS_DIR/bin/$d" "$PACKAGE_DIR"
                added=1
            fi
        done
    done
    [ "$added" = "0" ] && break
done

cd $SRC_DIR
echo "PACKAGING DONE"

# ------------------------------------------------------------------------------
echo "BUILDING NSIS INSTALLER"
# NSIS is not in the base msys2 image; install it now
pacman -S --noconfirm --needed mingw-w64-x86_64-nsis
VER=$(git describe --tags)
NSIS_SCRIPT="$SCRIPTS_DIR/../packaging/qimgv-installer.nsi"
# add a UTF-8 BOM so makensis reads the Chinese strings correctly
# (the CI runner is en-US; without a BOM makensis decodes the .nsi as ACP)
sed -i '1s/^/\xef\xbb\xbf/' "$NSIS_SCRIPT"
# disable msys2 path conversion so the /D... defines reach makensis intact
export MSYS2_ARG_CONV_EXCL='*'
makensis.exe \
    /DVER="$VER" \
    /DBUILD_DIR="$(cygpath -w $PACKAGE_DIR)" \
    /DAPP_ICON="$(cygpath -w $SRC_DIR/qimgv/res/icons/common/logo/app/qimgv.ico)" \
    "$(cygpath -w $NSIS_SCRIPT)"
# makensis writes the OutFile next to the script (packaging/), move it to repo root
if [ -f "$SRC_DIR/packaging/qimgv-x64_$VER.exe" ]; then
    mv "$SRC_DIR/packaging/qimgv-x64_$VER.exe" "$SRC_DIR/qimgv-x64_$VER.exe"
fi
if [ ! -f "$SRC_DIR/qimgv-x64_$VER.exe" ]; then
    echo "ERROR: NSIS installer build failed - qimgv-x64_$VER.exe not found"
    exit 1
fi
echo "INSTALLER DONE: $SRC_DIR/qimgv-x64_$VER.exe"
