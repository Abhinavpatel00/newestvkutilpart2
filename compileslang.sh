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

    # Only compile vertex stage if vs_main exists
    if grep -q "\bvs_main\b" "$file"; then
        compile_stage vertex   vs_main "$file" "$OUT_DIR/$name.vert.spv"
    fi

    # Only compile fragment stage if fs_main exists
    if grep -q "\bfs_main\b" "$file"; then
        compile_stage fragment fs_main "$file" "$OUT_DIR/$name.frag.spv"
    fi

    # Only compile compute stage if cs_main exists
    if grep -q "\bcs_main\b" "$file"; then
        compile_stage compute  cs_main "$file" "$OUT_DIR/$name.comp.spv"
    fi

done

echo
echo "Done."
