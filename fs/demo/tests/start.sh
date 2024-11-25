dd if=./disk of=~/ddriver bs=512 count=8192

mkdir mnt >/dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "" >/dev/null
else
    echo "" >/dev/null
fi