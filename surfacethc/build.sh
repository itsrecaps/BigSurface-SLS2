#!/bin/zsh
set -e
ROOT=${0:A:h}
DEPS=$ROOT/build/deps; OUT=$ROOT/build/SurfaceTHC.kext; NAME=SurfaceTHC; BID=com.itsrecaps.SurfaceTHC; VER=0.4.1
mkdir -p $DEPS
[[ -d $DEPS/MacKernelSDK ]] || git clone --depth 1 https://github.com/acidanthera/MacKernelSDK.git $DEPS/MacKernelSDK
SDK=$DEPS/MacKernelSDK; TMP=$(mktemp -d)
rm -rf $OUT; mkdir -p $OUT/Contents/MacOS; cp $ROOT/Info.plist $OUT/Contents/Info.plist
FLAGS=(-arch x86_64 -mkernel -nostdinc -fno-builtin -fno-common -fno-stack-protector -mmacosx-version-min=13.0
       -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -isystem $SDK/Headers -O2 -g)
cat > $TMP/info.c <<EOC
#include <mach/mach_types.h>
extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL($BID, "$VER", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
EOC
clang $FLAGS -c $TMP/info.c -o $TMP/info.o
clang++ $FLAGS -std=gnu++17 -nostdinc++ -fno-exceptions -fno-rtti -fapple-kext -Wno-inconsistent-missing-override -c $ROOT/SurfaceTHC.cpp -o $TMP/thc.o
clang++ -arch x86_64 -mkernel -nostdlib -static -Xlinker -kext -Xlinker -export_dynamic -mmacosx-version-min=13.0 \
  $TMP/*.o $SDK/Library/x86_64/libkmod.a -lcc_kext -o $OUT/Contents/MacOS/$NAME
codesign -f -s - $OUT >/dev/null 2>&1 || true
rm -rf $TMP; echo "built $OUT"
