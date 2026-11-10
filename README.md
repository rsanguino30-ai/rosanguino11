# TENSION — Build-Up FX

Plugin de efecto (VST3 / AU / Standalone) para generar tensión antes de un drop.
Una sola perilla **BUILD** controla todo: automatízala de 0 % a 100 % durante el
build-up y regrésala a 0 % justo en el drop.

## Qué hace la perilla BUILD

| Al subir BUILD… | Detalle |
|---|---|
| **Corta graves** | Filtro pasa-altos que sube de 20 Hz hasta el valor de *Low Cut*, con una pequeña resonancia que se acentúa para dar más tensión. |
| **Agrega reverb** | La reverb crece y la señal seca se va "lavando" dentro del espacio. |
| **Agrega ruido riser** | Ruido blanco filtrado cuya banda barre de graves a agudos (hasta el valor de *Noise Tone*). |
| **Abre el estéreo** | Ensanchamiento mid/side progresivo. |

En **0 %** el plugin es transparente: el drop entra con todo el grave y sin cola de reverb, así pega más fuerte.

## Controles

- **BUILD**: control principal (automatizable).
- **REVERB**: cuánta reverb hay al 100 % de build.
- **SIZE**: tamaño de la sala.
- **NOISE**: volumen máximo del riser de ruido.
- **NOISE TONE**: qué tan brillante llega a ser el barrido de ruido (3 kHz a 16 kHz).
- **LOW CUT**: frecuencia de corte al 100 % de build (40 Hz a 1500 Hz).
- **WIDTH**: cuánto se abre el estéreo.
- **CURVE**: forma de la respuesta. Valores bajos = la tensión se queda baja y explota al final (exponencial); 50 % = lineal; valores altos = sube rápido al inicio.
- **OUTPUT**: ganancia de salida. Incluye un soft-clipper suave que evita picos por encima de 0 dBFS.

## Uso rápido en tu DAW

1. Pon TENSION en el bus master o en un bus de grupo (todo menos el kick, si quieres que el kick se mantenga).
2. Dibuja automatización de **BUILD**: 0 % al inicio del build-up y 100 % en el último tiempo.
3. Justo en el drop, regresa BUILD a 0 %.

## Compilar

### Opción A: GitHub (sin instalar nada para Windows/Mac)
1. Sube esta carpeta a un repositorio de GitHub.
2. GitHub Actions compila solo (`.github/workflows/build.yml`).
3. Descarga los binarios en la pestaña **Actions → (último build) → Artifacts**:
   - `TENSION-Windows` → `TENSION.vst3`
   - `TENSION-macOS` → `TENSION.vst3` y `TENSION.component` (AU, Universal: Intel + Apple Silicon)

### Opción B: en tu computadora
Requisitos: CMake 3.22+, y Visual Studio 2022 (Windows) o Xcode (Mac).
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE se descarga automáticamente. Los plugins quedan en `build/Tension_artefacts/Release/`.

### Instalación
- **Windows**: copia `TENSION.vst3` a `C:\Program Files\Common Files\VST3\`
- **Mac**: `TENSION.vst3` → `~/Library/Audio/Plug-Ins/VST3/` y `TENSION.component` → `~/Library/Audio/Plug-Ins/Components/`

## Antes de vender
- **Nombre y marca**: cambia `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE` (4 letras, al menos una mayúscula) y `PRODUCT_NAME` en `CMakeLists.txt`. Verifica que el nombre "TENSION" no choque con otro producto.
- **Licencia de JUCE**: revisa los términos vigentes en juce.com (el plan gratuito tiene un límite de ingresos).
- **Mac**: firma y notariza el plugin con una cuenta de Apple Developer; si no, macOS lo bloquea.
- **Windows**: firmar el instalador es opcional pero evita advertencias de SmartScreen.
- **Pruébalo** en varios DAWs (Ableton, FL Studio, Logic, Reaper) y con distintos buffers y sample rates.

## Estructura
```
Source/PluginProcessor.*   DSP (filtro, ruido, reverb, estéreo)
Source/PluginEditor.*      Interfaz gráfica
tests/Render.cpp           Render offline de prueba (opcional: -DTENSION_BUILD_TESTS=ON)
```
