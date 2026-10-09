# Hamun Atmospheric Courtyard — lighting, rain and fog showcase

## Purpose

A recognizable architectural test environment instead of five test cubes.
It contains a long stone courtyard, repeated columns and beams, a central
walkway, lamps, water-reference patches and foliage silhouettes.

**Important:** the generated glTF is **geometry and PBR material metadata**.
The amber lamp meshes **do not yet emit point light**, the puddles are only
material references (not physical reflections), and **fog, rain particles, wet
surface reflections, volumetric shafts and shadows are NOT implemented by this
asset**. Never treat a screenshot of this geometry as proof these effects work.

## Generate and open

From repository root (Python 3, no dependencies):

```sh
python Tools/Showcase/generate_weather_courtyard.py
```

This writes `Samples/WeatherShowcase/AtmosphericCourtyard.gltf` with an embedded
binary buffer. Open an existing Hamun project in HamunEditor and use **Import
Asset** to import the glTF. The imported asset becomes editable using the
existing Outliner and Inspector. To ensure scene persistence, save it as a
`.hamunscene`. Alternatively open the glTF through the available scene-file
workflow where supported.

## Required end-to-end renderer validation (future milestones)

| Mode | What to demonstrate | Pass criterion |
|---|---|---|
| Baseline | Static PBR mesh and camera | All courtyard architecture draws on DX11 and DX12 |
| Daylight | Directional sun with editable azimuth/intensity | Surfaces visibly respond without changing albedo |
| Night | Four local amber lights | Local illumination and falloff; switch each light independently |
| Rain | Particle rainfall with adjustable rate | Falling drops, correct depth and visible frame-rate counter |
| Wet | Material wetness and puddle reflection | Reflection changes with camera angle and rain intensity |
| Fog | Exponential distance fog | Distant columns fade more than near ones; density slider |
| Storm | Rain + fog + night lighting | Stable frame timing; disable effects one at a time |
| Compatibility | DX12 vs DX11 | Same composition, deliberate documented fallbacks |

Implement effects as **real engine passes/components**, not a prerecorded
background video or flat transparent overlay. Use fixed reproducible camera
poses and capture FPS, frame time, GPU name, graphics API and resolution.

### Weather preset specification (not yet wired to runtime)

- `ClearDay`: sun intensity 1.0, rain 0, fog density 0.0
- `FogMorning`: sun intensity 0.45, rain 0, fog density 0.055
- `WetEvening`: sun intensity 0.18, rain 0.65, fog density 0.018
- `StormNight`: sun intensity 0.05, rain 1.0, fog density 0.075

Suggested controls: `1..4` switch preset, `F` fog toggle,
`R` rainfall toggle, `L` local-light toggle,
`P` pause weather animation, `F9` screenshot.

## Test separation

The scene importer test can pass even while dynamic lighting, rain, and fog
remain unavailable. Maintain separate test statuses for the geometry importer
and each visual effect. The next engine milestones are:
1. Light components and shader uniforms for directional/point lights.
2. Distance fog parameters and fog composition in the scene shader.
3. Rain emitter simulation and visible depth-tested droplets.
4. Wet material response and optional screen-space reflections.
5. Editor environment panel and reproducible screenshot baselines.
