#!/usr/bin/env sh

for d in tests/* ; do
    echo $(basename "$d"):
    ./mini $d/$(basename "$d").mini
    ./a.out < $d/input > output
    diff output $d/output.expected
done

rm output
