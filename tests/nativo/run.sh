#!/bin/bash
# Pruebas del backend nativo (codificador x86-64 y escritor ELF).
# Cada generador tests/nativo/X.c produce un ejecutable; se ejecuta y su
# salida se compara con tests/nativo/X.out.
#   bash tests/nativo/run.sh
cd "$(dirname "$0")/../.." || exit 1
tmp=$(mktemp -d)
pass=0; fail=0
for gen in tests/nativo/*.c; do
    name=$(basename "$gen" .c)
    if ! gcc -Wall -Wextra -o "$tmp/$name.gen" "$gen" x86enc.c elf.c; then
        echo "FAIL  $gen (no compila)"; fail=$((fail + 1)); continue
    fi
    "$tmp/$name.gen" "$tmp/$name"
    if "$tmp/$name" > "$tmp/$name.txt" 2>&1; echo "exit code: $?" >> "$tmp/$name.txt"
       diff -q "$tmp/$name.txt" "tests/nativo/$name.out" > /dev/null; then
        echo "PASS  $gen"; pass=$((pass + 1))
    else
        echo "FAIL  $gen"; diff "$tmp/$name.txt" "tests/nativo/$name.out"; fail=$((fail + 1))
    fi
done
rm -rf "$tmp"
echo "$pass passed, $fail failed."
[ "$fail" -eq 0 ]
