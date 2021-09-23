#
# Copyright (c) 2017 The Altra64 project contributors
# Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
# See LICENSE file in the project root for full license information.
#

#! /bin/bash
sudo mount /dev/sdb1 /mnt
file=/mnt/ED64P/OS64P.V64
if [ -e $file ]
then
	echo -e "File $file exists - mount ok"
	echo -e "copy..."
	sudo cp ../bin/OS64P.V64 /mnt/ED64P/
	sudo cp ../res/ALT64.INI /mnt/ED64P/
	echo -e "umounting..."
	sudo umount /mnt
	echo -e "done..."
else
	echo -e "File $file doesnt exists - sdcard problem?"
fi
