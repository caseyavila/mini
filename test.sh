#!/usr/bin/env sh

for d in tests/* ; do
    test=$(basename "$d")
    echo $test:
    ./mini "$@" $d/$test.mini
    ./$test < $d/input > output
    diff output $d/output.expected
    rm $test
done

rm output
