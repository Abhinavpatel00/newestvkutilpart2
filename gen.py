import os
import re

TEXTURE_DIR = "data/block"
OUT_FILE = "generated_voxel_materials.h"

def sanitize(name):
    name = name.replace(".png", "")
    name = re.sub(r"[^a-zA-Z0-9_]", "_", name)
    return name.upper()

textures = sorted([
    f for f in os.listdir(TEXTURE_DIR)
    if f.endswith(".png")
])

enum_entries = []
material_entries = []

for tex in textures:
    enum_name = sanitize(tex)

    enum_entries.append(f"    VOXEL_{enum_name},")

    material_entries.append(f"""
    [VOXEL_{enum_name}] =
    {{
        .face_tex = {{
            "data/block/{tex}",
            "data/block/{tex}",
            "data/block/{tex}",
            "data/block/{tex}",
            "data/block/{tex}",
            "data/block/{tex}"
        }},
        .debug_name = "{enum_name}"
    }},""")

with open(OUT_FILE, "w") as f:

    f.write("// AUTO-GENERATED FILE. DO NOT EDIT.\n\n")

    f.write("typedef enum\n{\n")
    f.write("    VOXEL_AIR = 0,\n")

    for e in enum_entries:
        f.write(e + "\n")

    f.write("\n    VOXEL_COUNT\n")
    f.write("} VoxelType;\n\n")

    f.write("VoxelMaterial voxel_materials[VOXEL_COUNT] =\n{\n")

    f.write("""
    [VOXEL_AIR] =
    {
        .debug_name = "AIR"
    },
""")

    for m in material_entries:
        f.write(m + "\n")

    f.write("};\n")

print(f"Generated {OUT_FILE} with {len(textures)} voxel types.")
