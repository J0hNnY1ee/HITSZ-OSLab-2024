# config
diskrandom="./disk2"
diskereal="~/ddriver"

filename="$((RANDOM)).txt"
BLOCKSIZE=1024
BLOCKINDEX=500

# 偏移位置
offset=$((BLOCKINDEX * BLOCKSIZE))


# 写磁盘
echo -n -e "$filename\0" | dd of="$diskrandom" bs=1 seek=$offset conv=notrunc >/dev/null 2>&1
dd if="$diskrandom" of=~/ddriver bs=512 count=8192 >/dev/null 2>&1

# 编译src
cd ..; mkdir build >/dev/null 2>&1; cd build

cmake .. >/dev/null 2>&1; make >/dev/null 2>&1;
if [ $? -eq 0 ]; then
    echo "" >/dev/null
else
    echo "Test Fail : 编译失败"
    exit 1
fi

# 挂载文件系统
cd ..
rm -rf ./tests/test_mnt; mkdir ./tests/test_mnt >/dev/null 2>&1

./build/demo  ./tests/test_mnt
if [ $? -eq 0 ]; then
    echo "" >/dev/null
else
    echo "Test Fail : 挂载失败"
    exit 1
fi

# 执行ls
ls ./tests/test_mnt >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "" >/dev/null
else
    fusermount -u ./tests/test_mnt
    echo "Test Fail : ls执行失败，钩子函数实现有误"
    exit 1
fi


# 获取输出结果
result=$(ls ./tests/test_mnt)
if [ "$result" = "$filename" ]; then
    echo "" >/dev/null
else
    fusermount -u ./tests/test_mnt
    echo "Test Fail : ls显示的结果不对"
    echo "你的结果 : $result"
    echo "正确结果 : $filename"
    exit 1
fi


# 卸载文件系统
sleep 1
fusermount -u ./tests/test_mnt
if [ $? -eq 0 ]; then
    echo "Test Pass!!!"
else
    echo "Test Fail : 卸载失败"
fi

