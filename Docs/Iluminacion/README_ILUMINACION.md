# Arena – Iluminación estilo Fortnite (Lobby y Arena 1v1) · UE 5.8.3

## Qué estaba mal (según tus capturas)

**Lobby** – todo azul y "lavado":
- `ExponentialHeightFog` con **niebla volumétrica** activa (densidad 0.009, albedo azul, 35 m) *delante* del personaje: la cámara está a 6,5 m y la niebla dispersaba luz azul sobre la skin.
- `DirectionalLight` azul (130,145,255) a 2,5 lux y `SkyLight` capturando el panorama unlit azul → tinte azul en la piel.
- El panorama (material Unlit = emisivo) y el `StageFloor` azul rebotaban luz azul vía Lumen sobre el personaje.

**Arena 1v1** – sobreexpuesto, plano, gris:
- Post-process con exposición **Manual**, bias 0.35 y sin cámara física = exposición fija 1.0 con un sol de 10 lux → suelo quemado.
- `ApplyArena1v1Look` (C++) además multiplicaba el sol ×1.25 y el skylight ×1.7 en cada BeginPlay, encima de lo anterior.
- `r.DefaultFeature.LocalExposure.*ContrastScale=0.8` en DefaultEngine.ini aplana luces y sombras en TODO el juego (imagen sin contraste).

**Personaje** – poco "nítido" frente a la taquilla de Fortnite:
- Rig de focos del lobby (`ArenaLobby.cpp`) con la key de radio 60 cm (speculares anchos y blandos), rims flojos (180 cd) por debajo de la key y sin relleno → cara con media sombra y sin el contorno brillante en pelo y hombros que da Fortnite.
- Anisotropía por defecto (4–8) → la rejilla del suelo del 1v1 se emborrona a distancia.

## Qué cambia

| Archivo | Cambio |
|---|---|
| `Config/DefaultEngine.ini` | Local Exposure desactivada (1.0), exposición automática por histograma por defecto, motion blur y lens flare OFF, Lumen por hardware (`r.Lumen.HardwareRayTracing=True`, el proyecto ya tiene RT), `r.Tonemapper.Sharpen=1.0`, línea RHI duplicada eliminada. |
| `Config/DefaultScalability.ini` (nuevo) | En calidad Epic: `r.MaxAnisotropy=16` y más resolución de sombra virtual para luces locales. Van aquí porque la escalabilidad pisa lo que se ponga en `[SystemSettings]`. |
| `Source/Arena/ArenaLobby.h/.cpp` | Rig de taquilla Fortnite: **key** 320 cd (radio 30 → speculares crisp) arriba a la izquierda de cámara, **fill** 110 cd muy suave a la derecha, **dos rims** 420/315 cd (radio 8) desde atrás-arriba apuntando a la cabeza → contorno brillante en pelo y hombros. Contact shadows en los focos. `LobbyExposureBias` -0.5 → -0.3. Nueva propiedad `FillLightIntensity`. |
| `Source/Arena/ArenaPlayerController.cpp` | `ApplyArena1v1Look` ya **no** se apila sobre el nivel: si existe un PostProcessVolume con tag `ArenaLook` (el del mapa) no hace nada. Si no existe, aplica un fallback con los mismos valores Fortnite (histograma, bias 0.4, rango 1–10 EV, saturación 1.15, contraste 1.08, sin motion blur) y sólo sube luces claramente bajas (no multiplica). |
| `Lvl_Lobby_JerarquiaCompleta.t3d` | Jerarquía completa del lobby con la nueva iluminación. |
| `Lvl_Lobby_SoloIluminacion.t3d` | Sólo los 4 actores de luz del lobby (Fog, SkyLight, DirectionalLight, ArenaLook). |
| `Lvl_Arena1v1_SoloIluminacion.t3d` | Sólo los 6 actores de la carpeta `Sky` + `ArenaLook` del 1v1. |
| `aplicar_iluminacion_1v1.py` | Genera la jerarquía completa del 1v1 a partir de tu export (los ~130 props no cambian, por eso no se reescriben a mano). |

### Valores – Lobby
- **Fog**: densidad 0.006, `StartDistance=850` (el personaje está a 650 cm de la cámara → la niebla sólo toca el bosque de fondo), máx. opacidad 0.45, **volumétrica OFF**. La bruma Niagara de Halloween se mantiene.
- **DirectionalLight**: blanco neutro (255,247,238), 1.5 lux, Pitch -35 / Yaw 200 (relleno desde el lado contrario a la key light que crea `ArenaLobby.cpp`), `LightSourceAngle=2` (sombra suave), `Movable`.
- **SkyLight**: 0.5, color cálido (255,232,205) que compensa el azul del panorama capturado, hemisferio inferior gris oscuro.
- **Plane (panorama) y StageFloor**: `bAffectDynamicIndirectLighting=False` → Lumen no rebota azul sobre la skin. El panorama además `CastShadow=False`.
- **ArenaLook**: histograma, bias -0.3, rango 1–8 EV, saturación 1.12, contraste 1.10, FilmToe 0.6, bloom 0.25, viñeta 0.3 (como el lobby de Fortnite), AO 0.7/60, motion blur / lens flare / fringe / grano = 0, Local Exposure = 1.0. Tag `ArenaLook`.

### Valores – Arena 1v1
- **Sol**: 9 lux, cálido (255,246,232), Pitch -48 / Yaw 30, `LightSourceAngle=0.8` (sombras definidas pero no duras), `ContactShadowLength=0.04`, `bAtmosphereSunLight=True`, `Movable`.
- **SkyLight**: 1.0, captura en tiempo real, hemisferio inferior (0.10,0.11,0.13), `Movable`.
- **Fog**: densidad 0.008, falloff 0.15, `StartDistance=2500`, máx. 0.6, volumétrica OFF (bruma sólo en la distancia).
- **ArenaLook**: histograma, bias +0.4, rango 1–10 EV, velocidades 4/2, saturación 1.15, contraste 1.08, FilmSlope 0.9 / Toe 0.6, bloom 0.3 (umbral 1.0), viñeta 0.15, AO 0.6/100, motion blur / lens flare / fringe / grano = 0, Local Exposure = 1.0. Tag `ArenaLook`.
- SkyAtmosphere y VolumetricCloud sin cambios.

## Cómo aplicarlo

1. Copia `Config/DefaultEngine.ini`, `Config/DefaultScalability.ini`, `Source/Arena/ArenaLobby.h`, `Source/Arena/ArenaLobby.cpp` y `Source/Arena/ArenaPlayerController.cpp` sobre los tuyos y recompila.
2. **Lobby**: abre `Lvl_Lobby`. Opción rápida: en el Outliner borra `Halloween_Atmospheric_Fog`, `SkyLight`, `DirectionalLight` y `ArenaLook`; abre `Lvl_Lobby_SoloIluminacion.t3d`, copia TODO el texto y pégalo en el viewport (Ctrl+V). Opción completa: Ctrl+A + Supr en el Outliner y pega `Lvl_Lobby_JerarquiaCompleta.t3d` (incluye los flags de rebote del panorama y del suelo; si usas la opción rápida, marca a mano en `Plane` y `StageFloor` → Lighting → *Affect Dynamic Indirect Lighting = OFF*).
3. **Arena 1v1**: abre `Lvl_Arena1v1`, borra los 5 actores de la carpeta `Sky` (`DirectionalLight`, `ExponentialHeightFog`, `SkyAtmosphere`, `SkyLight`, `VolumetricCloud`) y `ArenaLook`; pega `Lvl_Arena1v1_SoloIluminacion.t3d`. Si quieres el archivo de jerarquía completa, usa `aplicar_iluminacion_1v1.py` sobre tu export.
4. Guarda los mapas. Los `.umap` son binarios: no se pueden editar fuera del editor, por eso los cambios van como T3D (lo que el editor copia/pega).
5. En el juego, calidad de **Texturas**, **Sombras** y **Antialiasing** en Épico (los valores de `DefaultScalability.ini` y el TSR al 200 % de historial sólo se aplican en ese nivel).

## Ajuste fino
- Más/menos brillo global: `AutoExposureBias` del `ArenaLook` de cada mapa (±0.3 por paso).
- Si la skin se ve demasiado clara u oscura en el lobby: `LobbyExposureBias` en el actor `ArenaLobby` (por defecto -0.3; la cámara del lobby manda sobre el volumen).
- Contorno más o menos marcado en el personaje: `RimLightIntensity` (420). Cara más plana/más dramática: `FillLightIntensity` (110; bajar = más contraste).
- Nitidez: `r.Tonemapper.Sharpen` (1.0 ahora; 0.6–1.2 es el rango razonable, por encima salen halos). TSR sigue a 100 % con historial al 200 %.
- Si prefieres sombras más duras tipo Fortnite competitivo: `LightSourceAngle` del sol a 0.5.
- Si Lumen por hardware baja demasiado los FPS: `r.Lumen.HardwareRayTracing=False` en `DefaultEngine.ini` (vuelve al Lumen por software que ya usabas).
