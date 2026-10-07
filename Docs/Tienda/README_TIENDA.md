# Tienda de objetos (Slate + fortnite-api.com)

Recreación de la tienda de Fortnite para el lobby. Se construye enteramente en **Slate** (`SArenaShopView`, `SArenaShopTile`, `SArenaShopDetail`, `SArenaShopButton`, `SArenaGradient` en `ArenaShopWidget.cpp`) y se monta dentro de `UArenaShopWidget` (un `UUserWidget`) para que el lobby la coloque como una página más.

## Datos: nada fijo en el código
- Al construirse el lobby, `UArenaShopWidget::Refresh()` pide `https://fortnite-api.com/v2/shop?language=es` y guarda la respuesta en `Saved/ArenaShop/shop_es_AAAA-MM-DD.json`.
- La tienda rota a las **00:00 UTC** (02:00 en España en horario de verano, 01:00 en invierno). `NativeTick` comprueba cada 30 s si el día UTC ha cambiado y vuelve a descargar; si la API todavía sirve el día anterior justo tras la rotación, reintenta a los 5 min. El contador "Nuevos objetos en HH:MM:SS" de la cabecera cuenta hasta esa hora.
- Las imágenes se descargan **sólo cuando el tile se dibuja** (primer `Tick` del tile), 4 en paralelo, se decodifican y reducen a 640 px en un hilo del pool (`FImageUtils::DecompressImage` + `FImage::ResizeTo`) y se convierten en textura en el hilo del juego. Se cachean en `Saved/ArenaShop/Images/` y en memoria.

## Qué se lee de cada oferta
| API | Uso |
|---|---|
| `layout.name` / `layout.category` / `layout.index` | Sección (título en Burbank inclinada) y orden de secciones. Las ofertas sin `layout` van a "MÁS OFERTAS". |
| `tileSize` (`Size_1_x_1`, `Size_2_x_1`, `Size_3_x_1`, `Size_4_x_1`) | Ancho en unidades; la fila tiene 4 unidades y un `SWrapBox` las empaqueta como la web. Un 1x1 es 330x450 (ratio 1.364). |
| `newDisplayAsset.renderImages[0].image` | Arte completo del tile (incluye fondo). Se recorta en modo *cover* con `UVRegion`. Si no existe: `bundle.image`, `brItems[0].images.featured/icon`, `cars.images.large`, `instruments.images.large`, `tracks.albumArt`. |
| `colors.color1/color3` | Degradado del fondo del tile; si faltan, color por rareza. |
| `banner.value` | Pastilla blanca "1.200 PAVOS DE DESCUENTO" sobre el nombre. |
| `finalPrice` / `regularPrice` | Precio con icono de paVos y, si hay descuento, precio anterior tachado. |
| `bundle.name`, `brItems[].name/description/type/rarity/series/set/introduction` | Nombre del tile y página de detalle (clic): descripción, conjunto, "Introducido en...", contenido del lote. |
| `sortPriority` | Orden dentro de la sección (mayor primero, como Fortnite). |
| `vbuckIcon` | Icono de paVos cuando el lobby no aporta `CurrencyIcon`. |

## Efectos al pasar el ratón (como en Fortnite)
En 0,16 s con `FCurveSequence` (ease out):
- el tile crece un 3,5 % desde su centro y se ilumina (velo blanco al 7 %),
- se enciende un borde blanco de 2,5 px redondeado,
- el arte hace zoom del 8 % (recorte UV, así las esquinas redondeadas no se rompen); los iconos sin arte de tile hacen zoom del 7 % y suben 6 px,
- suena `ArenaUISounds::PlayHover`.
Al soltar el ratón vuelve con la misma animación. Clic → página de detalle a pantalla completa con los colores de la oferta, arte grande, nombre, tipo/rareza/serie, descripción, conjunto, precio y botones COMPRAR (toast: no disponible en esta recreación) y ATRÁS.

## Integración en el lobby (`ArenaLobbyWidget`)
- La pestaña TIENDA llama a `HandleShopTab` → `ShowShop()`: oculta lobby/taquilla y la barra superior (la tienda es a pantalla completa, como en Fortnite), marca la pestaña y llama a `Refresh(false)` (usa la caché si es el mismo día).
- `ShowLobby()`/`ShowLocker()` cierran la tienda. ESC/Retroceso/B del mando: primero cierra la página de detalle, después vuelve al lobby (`HandleShopReturn`). La rueda de emotes (B) no se abre con la tienda abierta.
- El lobby pasa sus fuentes (`HeadingFontFace`, `BodyFontFace`, `BoldFontFace`) y `CurrencyIcon` al widget de tienda.

## Dependencias del módulo
En `Arena.Build.cs` deben estar (añade las que falten):
```csharp
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "HTTP", "Json", "ImageCore" });
```
`HTTP` ya lo usa el lobby (avatares). `ImageCore` aporta `FImage`/`ResizeTo`; `Json` el parser.

## Ajustes
- `UArenaShopWidget::Language` (`es` por defecto), `ShopUrl`, `MaxTextureSize` (640), `WalletVBucks` (saldo mostrado).
- Tamaños y colores en `ArenaShopStyle` (`TileAspect`, `TileGap`, `ContentMaxWidth`, `HoverSeconds`).
- Para forzar una descarga: borrar `Saved/ArenaShop/` o llamar a `Refresh(true)` (botón REINTENTAR del estado de error).
