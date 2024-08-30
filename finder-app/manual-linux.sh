#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

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

    # TODO: Add your kernel build steps here
    # clean from possible old artifacts
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    # use the def config for qemu virt
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # build the kernel
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

    # build modules 
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

    # build devicetree
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

    # copy kernel image
    cp ./arch/${ARCH}/boot/Image ${OUTDIR}/
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox

    # clean
    make distclean

    # use defconfig
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
# compile
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

ROOTFS=${OUTDIR}/rootfs

# install rootfs
make CONFIG_PREFIX=${ROOTFS} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# change to rootfs
cd ${ROOTFS}

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# get sysroot from compiler
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cp -L $SYSROOT/lib/ld-linux-aarch64.* lib

cp -L $SYSROOT/lib64/libresolv.so.* lib64
cp -L $SYSROOT/lib64/libm.so.* lib64
cp -L $SYSROOT/lib64/libc.so.* lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/nul c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE} all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
TARGET=${ROOTFS}/home
cp ${FINDER_APP_DIR}/finder-test.sh ${TARGET}
cp ${FINDER_APP_DIR}/conf ${TARGET} -r
cp ${FINDER_APP_DIR}/conf/ ${ROOTFS} -r
cp ${FINDER_APP_DIR}/finder.sh ${TARGET}
cp ${FINDER_APP_DIR}/writer ${TARGET}
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${TARGET}

# TODO: Chown the root directory
sudo chown -R root:root ${ROOTFS}

# TODO: Create initramfs.cpio.gz
cd ${ROOTFS}
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

# compress file
cd ${OUTDIR}
rm -f initramfs.cpio.gz
gzip -f initramfs.cpio