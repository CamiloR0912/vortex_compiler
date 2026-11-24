#!/bin/bash
# Compila y ejecuta cada .vx con ./vortexc --run y compara la salida con su .out.
#   bash tests/run_tests.sh            compara
#   bash tests/run_tests.sh --update   regenera los .out con la salida actual
cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0
shopt -s nullglob       # si no hay subcarpetas con .vx, el patrón no cuenta como archivo
for vx in tests/*.vx tests/*/*.vx; do
    out="${vx%.vx}.out"
    if [ "$1" = "--update" ]; then
        ./vortexc "$vx" --run > "$out" 2>&1
        continue
    fi
    if ./vortexc "$vx" --run 2>&1 | diff -q - "$out" > /dev/null; then
        echo "PASS  $vx"; pass=$((pass + 1))
    else
        echo "FAIL  $vx"; fail=$((fail + 1))
    fi
done
[ "$1" = "--update" ] && { echo "Archivos .out regenerados."; exit 0; }
echo "$pass passed, $fail failed."
[ "$fail" -eq 0 ]
