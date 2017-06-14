apt-get install bc
echo "cloning kernel source"
git clone https://github.com/raspberrypi/linux kernel
cd kernel
FIRMWARE_HASH=$(zgrep "* firmware as of" /usr/share/doc/raspberrypi-bootloader/changelog.Debian.gz | head -1 | awk '{ print $5 }')
KERNEL_HASH=$(wget https://raw.github.com/raspberrypi/firmware/$FIRMWARE_HASH/extra/git_hash -O -)
echo "checking out branch"
git checkout $KERNEL_HASH
echo "cleaning kernel source"
make mrproper
echo "applying kernel configuration"
sudo modprobe configs
zcat /proc/config.gz > .config
make -j6 oldconfig
echo "building kernel"
make -j6
cd ..
echo "builing module"
make -j6 -C kernel M=$(pwd) modules








