#! /bin/bash
sudo mount /dev/sdb1 /mnt
file=/mnt/ED64P/OS64P.v64
if [ -e $file ]
then
	echo -e "File $file exists - mount ok"
	echo -e "copy..."
	sudo cp OS64P.v64 /mnt/ED64P/
	sudo cp ALT64.INI /mnt/ED64P/
	echo -e "umounting..."
	sudo umount /mnt
	echo -e "done..."
else
	echo -e "File $file doesnt exists - sdcard problem?"
fi
