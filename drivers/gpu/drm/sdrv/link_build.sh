#!/bin/bash

#drivers/gpu/drm/sdrv
LINUX_DIR=../../../..
cd $LINUX_DIR
export CROSS_COMPILE=/tool/gcc_linaro/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
make -j64 ARCH=arm64  O=out_eng x9_ref_defconfig
make -j64 ARCH=arm64  O=out_eng
cp -v out_eng/drivers/gpu/drm/sdrv/semidrive_linked.o drivers/gpu/drm/sdrv/semidrive_linked.o_shipped
cd -
