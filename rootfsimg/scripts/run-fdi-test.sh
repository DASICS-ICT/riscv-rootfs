# set -x
echo =========== Test Begin: memory protection ===========
/root/fdi-test-ofb
dmesg | grep "DASICS EXCEPTION" | tail -n1
echo =========== Test End: memory protection =============

echo
echo

echo =========== Test Begin: jump protection ===========
/root/fdi-test-jump
dmesg | grep "DASICS EXCEPTION" | tail -n1
echo =========== Test End: jump protection =============

echo
echo

echo =========== Test Begin: control flow check ===========
/root/fdi-test-cfi
dmesg | grep "DASICS EXCEPTION" | tail -n1
echo =========== Test End: control flow check =============

echo
echo

echo =========== Test Begin: maincall ===========
/root/fdi-test-maincall
echo =========== Test End: maincall =============

echo
echo

echo =========== Test Begin: fine-grained protection ===========
/root/fdi-test-finegrain
dmesg | grep "DASICS EXCEPTION" | tail -n1
echo =========== Test End: fine-grained protection =============

echo
echo

echo "=========== Test Begin: flexible protection ==========="
i=0
while [ $i -lt 5 ]; do
    random_number=$(( $(awk 'BEGIN {srand(); print int(rand()*100)}') ))  # 0-99 的随机数
    echo "测试第 $((i+1)) 个函数: $random_number"
    /root/fdi-test-finegrain "$random_number"
    dmesg | grep "DASICS EXCEPTION" | tail -n1
    sleep 0.5  # 可选，控制每次循环的间隔
    i=$((i + 1))
done
echo "=========== Test End: fine-grained protection ============="