# Hamun Animation Editor — UI/UX Implementation Contract

## Visual target
The approved dark animation editor mockup is the design target, **not a representation of implemented features**. Use Blender's region/editor framework and Hamun renderer; Win32 may be a windowing layer but must not define widget appearance.

## Screen areas
| Area | Minimum behavior | Implementation |
| --- | --- | --- |
| Animation Assets (left) | Search/filter Actions, Blend Spaces and parameters; drag Actions onto graph | Planned |
| State Graph (center) | Pan/zoom, node placement, editable State and Transition nodes | Initial Blender node graph, colored nodes, manual links |
| State Properties (right) | Action, loop, playback speed, conditions, priority, blend duration | Initial properties directly on nodes |
| 3D Preview (right) | Armature animation, play/pause, skeleton overlay, camera controls | Initial evaluate/apply Action to selected armature only |
| Debug (bottom-right) | Current state, numeric/boolean inputs, active transition | Initial current state + numeric Speed |
| Timeline (bottom) | Base pose, upper/lower-body tracks, scrubbing and blend debugging | Planned (Blender Dope Sheet / NLA integration) |

## Interaction conventions
- Left-click selects a node; Shift-click multi-select; middle-drag pans; wheel zooms.
- Node link direction: Entry -> State; State -> Transition -> State. Each Transition must have exactly one incoming and outgoing link.
- Double-click State: future nested blend tree. In MVP, edit Action/Loop/Speed directly on node.
- Error feedback must report invalid entry wiring, missing Action and dangling transitions.
- Colors: Entry green, State blue, Transition amber. The runtime active node will eventually have a bright green edge.
- Do not assume numerical Speed thresholds are in centimeters/second; domain belongs to the caller/gameplay controller.

## Architecture boundary
Authoring graph in Blender -> validated versioned JSON -> future native C++ graph resource -> runtime state evaluator -> pose blending -> skeleton GPU skinning in Hamun Renderer.

**Do not claim native runtime or full live 3D crossfading until those are implemented and exercised.**

## Next engineering gates
1. Blender background-mode smoke test for addon enable, sample graph creation and JSON export.
2. UI smoke test on actual Hamun Blender fork. Fix API incompatibilities.
3. Stable IDs stored per State (avoid renaming IDs on node rename).
4. Type-checked transition parameters with scalar/bool support, priorities and interruption semantics.
5. Asset indexing and ergonomic multi-panel Animation workspace.
6. C++ runtime with clip time advancement, skeletal pose evaluation and crossfades.
7. Blend Spaces 1D/2D, sync groups, additive layered animation, events and profiling.
8. Automated Windows packaging and launcher integration.
