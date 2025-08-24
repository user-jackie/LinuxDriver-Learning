#!/bin/bash
file_name="keyirq"

ko_file="${file_name}.ko"
App_file="${file_name}App"
target_dir="/home/brojackie/linux/nfs/rootfs/lib/modules/4.1.15+"
cross_compiler="/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc"

make clean
make
"${cross_compiler}" "${App_file}.c" -o "$App_file"
sudo cp -rf "$ko_file" "$App_file" "$target_dir"
