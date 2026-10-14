# Hamun Engine — New Map Design Handoff

**Status:** Design handoff / blockout specification  
**Target:** Hamun Engine prototype  
**Scope:** Three original environment maps, engine-agnostic blockout first  
**Important:** These are proposed map designs, not finished or imported level assets. Geometry, lighting, navigation, and performance still need implementation and in-engine validation.

## Design goals

- Make each map visually distinct and useful for testing different engine systems.
- Keep the first pass blockout-friendly: modular geometry, clear scale references, simple materials.
- Separate gameplay routes from decorative detail so collision and navigation can be tested early.
- Avoid depending on proprietary assets. Use greybox primitives until the asset pipeline is ready.

## Map 01 — Red Basin

**Role:** Starter / traversal and combat test map  
**Visual identity:** A wide rust-red canyon, pale stone shelves, dry riverbed, wind-carved arches.

### Layout
- Central basin is the main orientation landmark.
- Two elevated rim routes connect the west and east overlooks.
- A narrow canyon path provides a short, risky route through the center.
- Three traversable height bands: basin floor, middle shelves, upper rim.
- One sheltered ruin serves as a spawn / checkpoint landmark.

### Blockout dimensions
- Suggested footprint: 1,200 m × 900 m.
- Main path width: 8–14 m.
- Secondary paths: 4–7 m.
- Vertical separation between shelves: 12–28 m.
- Place a landmark or sightline break every 100–150 m.

### Systems to validate
- Large-world camera framing and distance fog.
- Terrain collision and steep-slope limits.
- Navigation between height bands.
- LOD / culling and streaming boundaries.

## Map 02 — Copperline Industrial

**Role:** Dense navigation, interiors, and modular-kit test  
**Visual identity:** Abandoned copper refinery, gantries, pipes, service corridors, loading yard, amber work lights.

### Layout
- A rectangular central processing hall forms the main landmark.
- A ground-level service loop connects loading yard, workshop, and pump room.
- Catwalks form an upper loop with at least two vertical connections.
- One readable shortcut opens between the workshop and loading yard.
- Keep the critical route visually legible with light, color, and signage.

### Blockout dimensions
- Suggested footprint: 700 m × 500 m.
- Main hall: 100 m × 70 m × 32 m high.
- Standard corridor width: 3–5 m.
- Catwalk width: 2–3 m.
- Door clear height: 2.5–3.5 m.

### Systems to validate
- Modular snapping and grid alignment.
- Interior lighting, shadow cost, and occlusion.
- Doorway collision and player clearance.
- Multi-level navigation and traversal transitions.

## Map 03 — Glass Delta

**Role:** Water, reflections, and open-to-enclosed transition test  
**Visual identity:** Shallow turquoise channels, dark basalt islands, translucent mineral formations, misty wetlands.

### Layout
- A branching water channel splits the map into three island zones.
- Two bridges and one shallow ford provide different crossing choices.
- A crystal field is the dominant distant landmark.
- A partially enclosed cavern connects the delta to a small underground chamber.
- The main route remains visible from at least one elevated viewpoint.

### Blockout dimensions
- Suggested footprint: 1,000 m × 800 m.
- Main water channel: 20–45 m wide.
- Bridges: 5–8 m wide.
- Shallow ford: 10–18 m wide.
- Cavern ceiling: 12–25 m above the floor.

### Systems to validate
- Water surface and shoreline collision.
- Transparent / translucent material sorting.
- Fog transitions between exterior and cavern.
- Reflection quality and performance fallback.

## Shared authoring rules

- Use metres as the design unit; document the engine's actual world-unit conversion before final import.
- Prefix blockout objects by purpose: `GEO_`, `COL_`, `NAV_`, `LIGHT_`, `SPAWN_`, `LANDMARK_`.
- Keep collision separate from render meshes.
- Use simple greybox materials first; do not bake final lighting before scale and routes are approved.
- Mark spawn points, checkpoints, traversal links, blocked routes, and test cameras explicitly.
- Prefer reusable modular pieces for doors, stairs, bridges, rails, pipes, and platform edges.
- Save each map as its own level/scene and keep shared materials and props in a common asset folder.

## Suggested repository layout

```text
Content/
  Maps/
    RedBasin/
      RedBasin_Blockout
      Docs/
    CopperlineIndustrial/
      CopperlineIndustrial_Blockout
      Docs/
    GlassDelta/
      GlassDelta_Blockout
      Docs/
  Environment/
    Materials/
    Modular/
    Props/
  Tests/
    MapTraversal
    Collision
    Lighting
```

Adapt this folder structure to Hamun's actual asset and scene format once that format is confirmed.

## Acceptance checklist for the first playable blockout

- [ ] All three maps open without missing assets.
- [ ] Player / test camera can traverse every intended main route.
- [ ] No unintended gaps, stuck points, or collision-only invisible walls.
- [ ] Spawn, checkpoint, exit, and test-camera markers are identifiable.
- [ ] Each map demonstrates its intended system focus.
- [ ] Frame time and memory are recorded on the target test machine.
- [ ] Screenshots are captured from the actual Hamun build, not concept art.
- [ ] Known issues and unsupported engine features are documented.

## Not included in this handoff

This document does not claim that map meshes, terrain, textures, navigation, or playable levels have already been created. The next implementation milestone is a greybox of **Red Basin** first, followed by a traversal/collision pass before duplicating the pipeline for the other two maps.
