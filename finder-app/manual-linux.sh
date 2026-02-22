#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u
sudo echo

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    echo "\nCleaning Kernel"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "Build Kernel Config"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "Building Kernel"
    make -j10 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # echo "Building modules"
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    echo "Building dtbs"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs


fi

echo "Adding the Image in outdir"
cd ${OUTDIR}/linux-stable
cp arch/arm64/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/bin
mkdir -p ${OUTDIR}/rootfs/dev
mkdir -p ${OUTDIR}/rootfs/etc
mkdir -p ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/lib
mkdir -p ${OUTDIR}/rootfs/lib64
mkdir -p ${OUTDIR}/rootfs/proc
mkdir -p ${OUTDIR}/rootfs/sbin
mkdir -p ${OUTDIR}/rootfs/sys
mkdir -p ${OUTDIR}/rootfs/tmp
mkdir -p ${OUTDIR}/rootfs/usr/bin
mkdir -p ${OUTDIR}/rootfs/usr/lib
mkdir -p ${OUTDIR}/rootfs/usr/sbin
mkdir -p ${OUTDIR}/rootfs/var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# Make and install busybox
echo "Distclean busybox"
make distclean
echo "defconfig busybox"
make defconfig
echo "build busybox"
make -j10 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
echo "install busybox"
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

INTR_LIB=`${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"`
SO_LIB=`${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"`

# making the list just file names
INTR_LIB=`echo $INTR_LIB | sed -E "s/.*program interpreter: (.*)\]/\1/"`
SO_LIB=`echo $SO_LIB | sed -E "s/0x0000000000000001 \(NEEDED\) Shared library: \[//g" | sed -E "s/\]//g"`
FIND_PATH=`which ${CROSS_COMPILE}readelf | sed -E "s/bin\/${CROSS_COMPILE}readelf//"`

# Add library dependencies to rootfs
echo "from: ${FIND_PATH}"
echo "Copy ${INTR_LIB} -> lib"
echo "Copy ${SO_LIB} -> lib64"

echo "cp -L ${FIND_PATH}/aarch64-none-linux-gnu/libc/${INTR_LIB} ${OUTDIR}/rootfs/lib"
cp -L ${FIND_PATH}/aarch64-none-linux-gnu/libc/${INTR_LIB} ${OUTDIR}/rootfs/lib

cd ${FIND_PATH}/aarch64-none-linux-gnu/libc/lib64
echo "cp -Lt ${OUTDIR}/rootfs/lib64/ ${SO_LIB}"
cp -Lt ${OUTDIR}/rootfs/lib64/ ${SO_LIB}
cd ${OUTDIR}/rootfs

# Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} clean writer


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer autorun-qemu.sh finder-test.sh finder.sh ${OUTDIR}/rootfs/home/
cp -r ../conf ${OUTDIR}/rootfs/home/
cp -r ../conf ${OUTDIR}/rootfs/

# Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root .

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio

