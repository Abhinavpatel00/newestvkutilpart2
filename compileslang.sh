
#!/usr/bin/env bash

set -e

SRC_DIR="shaders"
OUT_DIR="compiledshaders"

mkdir -p "$OUT_DIR"

echo "Compiling Slang shaders..."
echo

compile_stage ()
{
    local stage="$1"
    local entry="$2"
    local src="$3"
    local out="$4"

    if [ ! -f "$out" ] || [ "$src" -nt "$out" ]; then
        echo "  $stage → $out"
    /opt/shader-slang-bin/bin/slangc "$src" \
            -target spirv \
            -entry "$entry" \
            -stage "$stage" \
            ${SLANG_DEFINES:-} \
            -o "$out"
    fi
}

for file in "$SRC_DIR"/*.slang; do

    name=$(basename "$file" .slang)

    compile_stage vertex   vs_main "$file" "$OUT_DIR/$name.vert.spv"
    compile_stage fragment fs_main "$file" "$OUT_DIR/$name.frag.spv"
    compile_stage compute  cs_main "$file" "$OUT_DIR/$name.comp.spv" || true

done

echo
echo "Done."
