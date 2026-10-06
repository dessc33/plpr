"""Genera la jerarquia COMPLETA de Lvl_Arena1v1 con la iluminacion Fortnite.

Uso:
  1. En el editor abre Lvl_Arena1v1, Ctrl+A en el Outliner, Ctrl+C y pega el texto en un archivo
     (por ejemplo Lvl_Arena1v1_original.t3d).
  2. python aplicar_iluminacion_1v1.py Lvl_Arena1v1_original.t3d Lvl_Arena1v1_JerarquiaCompleta.t3d
  3. Abre el .t3d generado, copia todo y pegalo en el nivel (tras borrar los actores, ver README).

Sustituye solo los 6 actores de iluminacion (DirectionalLight_2, ExponentialHeightFog_0, SkyAtmosphere_0,
SkyLight_1, VolumetricCloud_0 y el PostProcessVolume "ArenaLook"); props, PlayerStarts y vehiculos quedan intactos.
"""
import os
import re
import sys

REPLACED = {
    "DirectionalLight_2", "ExponentialHeightFog_0", "SkyAtmosphere_0",
    "SkyLight_1", "VolumetricCloud_0", "PostProcessVolume_2",
}


def main() -> None:
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))
    text = open(src, encoding="utf-8", errors="replace").read()

    kept, removed = [], []
    actor_re = re.compile(r"^\s*Begin Actor .*? Name=(\S+)", re.M)
    pos = 0
    for match in actor_re.finditer(text):
        end = text.index("\n", text.index("      End Actor", match.start())) + 1
        if match.start() < pos:
            continue
        kept.append(text[pos:match.start()])
        if match.group(1) in REPLACED:
            removed.append(match.group(1))
        else:
            kept.append(text[match.start():end])
        pos = end
    kept.append(text[pos:])
    out = "".join(kept)

    lighting = open(os.path.join(here, "Lvl_Arena1v1_SoloIluminacion.t3d"), encoding="utf-8").read()
    block = lighting[lighting.index("      Begin Actor"):lighting.index("   End Level")]
    out = out.replace("   End Level", block + "   End Level", 1)
    open(dst, "w", encoding="utf-8", newline="\r\n").write(out)
    print(f"Actores sustituidos: {sorted(removed)}")
    print(f"Escrito: {dst}")


if __name__ == "__main__":
    main()
