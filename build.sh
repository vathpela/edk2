#!/bin/bash
#
# Build recipe for the git submodule based quark tree
#
# First initialise the submodules
git submodule init
git submodule update
# add the external crypto libraries
pushd CryptoPkg/Library/OpensslLib
if [ ! -f openssl-0.9.8w.tar.gz ]; then
    wget http://www.openssl.org/source/old/0.9.x/openssl-0.9.8w.tar.gz
fi
if [ ! -d openssl-0.9.8w ]; then
    tar xfz openssl-0.9.8w.tar.gz
    cd openssl-0.9.8w
    patch -b -p0 <../EDKII_openssl-0.9.8w.patch
    cd ..
    sh ./Install.sh
fi
popd
#
# Need to apply patch for generating correct hashes on IA32
if [ ! -f SecurityPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.c.orig ]; then
    patch -p1 -b < DxeImageVerificationLib-fix.diff
fi
# now initialise the edk2environment using the QuarkPlatformPkg Overrides
cp .module/edk2/edksetup.sh .
mkdir Conf
. ./edksetup.sh
cp QuarkPlatformPkg/Override/BaseTools/Conf/build_rule.template Conf/build_rule.txt
cp QuarkPlatformPkg/Override/BaseTools/Conf/tools_def.template Conf/tools_def.txt
make -C BaseTools
. ./edksetup.sh
# OK now we're ready to build.  This is for the default debug package
# alternatively, you can just comment this out and run quarkbuild.sh but
# it does a lot of irrelevant other stuff
type=DEBUG
flags="-DDEBUG_PRINT_ERROR_LEVEL=0x80000042 -DDEBUG_PROPERTY_MASK=0x27"
#type=RELEASE
#flags="-DDEBUG_PRINT_ERROR_LEVEL=0x80000000 -DDEBUG_PROPERTY_MASK=0x23"
##
# You probably have gcc 4.8 or 4.9, but this doesn't seem to matter
toolchain=GCC47
build -a IA32 -b ${type} -y Report.log -t ${toolchain} -p QuarkPlatformPkg/QuarkPlatformPkg.dsc ${flags} -DSECURE_BOOT || exit 1
# finally, the spi flash tools are going to need the capsule creator, so build it
make -C QuarkPlatformPkg/Tools/CapsuleCreate
#
# FIXME: This should be in QuarkPlatformPkg.fdf
# Now split the FV files apart again
./QuarkPlatformPkg/Tools/QuarkSpiFixup/QuarkSpiFixup.py QuarkPlatform ${type} ${toolchain}
# finally put the capsule build where the spi tools can find it
mkdir Build/QuarkPlatform/${type}_${toolchain}/FV/Tools
# and the shell
cp EdkShellBinPkg/FullShell/Ia32/Shell_Full.efi Build/QuarkPlatform/${type}_${toolchain}/FV/Shell.efi
mv QuarkPlatformPkg/Tools/CapsuleCreate/CapsuleCreate Build/QuarkPlatform/${type}_${toolchain}/FV/Tools
##
# Now begin the process of creating the BootRom
##
cd Build
if [ ! -f spi-flash-tools_v1.0.1.tar.gz ]; then
    wget -O spi-flash-tools_v1.0.1.tar.gz https://github.com/01org/Galileo-Runtime/blob/master/spi-flash-tools_v1.0.1.tar.gz?raw=true
fi
if [ ! -f sysimage_v1.0.1.tar.gz ]; then
    wget -O sysimage_v1.0.1.tar.gz https://github.com/01org/Galileo-Runtime/blob/master/sysimage_v1.0.1.tar.gz?raw=true
fi
if [ ! -d spi-flash-tools_v1.0.1 ]; then
    tar xfz spi-flash-tools_v1.0.1.tar.gz
fi
if [ ! -d sysimage_v1.0.1.tar.gz ]; then
    tar xfz sysimage_v1.0.1.tar.gz
fi
# create our own build directory and copy in a few hand made files
if [ ! -d mysysimage ]; then
    mkdir mysysimage
    pushd mysysimage
    ln -s ../sysimage_v1.0.1/config .
    ln -s ../sysimage_v1.0.1/inf .
    ln -s ../QuarkPlatform/${type}_${toolchain}/FV .
    cp ../../layout.conf .
    popd
    cp ../galileo-platform-data.ini spi-flash-tools_v1.0.1/platform-data/platform-data.ini
fi
cd mysysimage
rm -f layout.mk
make -f ../spi-flash-tools_v1.0.1/Makefile
cd ../spi-flash-tools_v1.0.1/platform-data/
./platform-data-patch.py -i ../../mysysimage/Flash-missingPDAT.bin
mv Flash+PlatformData.bin ../../mysysimage/
echo
echo everything is ready, the image to flash is
echo    Build/mysysimage/Flash+PlatformData.bin
#
# The flash command I use is
#  flashrom -p buspirate_spi:dev=/dev/ttyUSB0,pullups=on -w Flash+PlatformData.bin
