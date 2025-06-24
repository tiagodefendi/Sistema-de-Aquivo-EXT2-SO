dd if=/dev/zero of=./myext2image.img bs=1024 count=64K

mkfs.ext2 -b 1024 ./myext2image.img

e2fsck myext2image.img
e2label myext2image.img SO_Trabalho

debugfs myext2image.img

sudo mount myext2image.img /mnt
mount -o loop myext2image.img /mnt/ext2

sudo umount /mnt