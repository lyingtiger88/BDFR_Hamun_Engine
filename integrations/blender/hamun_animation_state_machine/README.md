# Hamun Animation State Machine — MVP 0.1

This is a Blender-hosted authoring addon for the Hamun Engine fork. It does **not** yet add a native C++ Hamun runtime or bind animation clips to game entities.

## Install / try

1. In the Hamun Blender fork, open **Edit > Preferences > Add-ons > Install from Disk** (or use the relevant extension installation command for your Blender version).
2. Select `integrations/blender/hamun_animation_state_machine/__init__.py` and enable **Hamun Animation State Machine**. Alternatively install the whole `hamun_animation_state_machine` folder as a zipped legacy add-on.
3. Switch an editor to the **Node Editor**, select the **Hamun Animation** node-tree type. In the sidebar's **Hamun** tab choose **Create Idle / Walk Demo**.
4. Set the `Action` field of your states to action datablock names. Change transition parameters and thresholds.
5. Choose **Export Hamun State Machine**. Save to a `.json` file.

Note: Action references are strings for now; export does not package the animation clips themselves.

## Graph rules

- Exactly one Entry node, connected to a single State.
- State -> Transition -> State connections.
- A Transition must have exactly one input and output.
- Transition parameters are numeric; missing parameters never trigger transitions.
- Higher-priority conditions are evaluated first (one transition per update).
- Graphs are saved with the Blender project. JSON export is explicit.

## Sample runtime

`runtime_reference.py` is a dependency-free Python reference implementation that can evaluate exported JSON with parameters such as `{"Speed": 1.0}`. It does **not** interpolate animation poses or render characters. Hamun C++ runtime/clip loading and transition blending are next milestones.

## Next implementation milestones

- Define a C++ serializable animation graph resource and clip IDs.
- Runtime tick, transition conditions and crossfade pose mixer.
- Bind exported Blender actions to Hamun skeletal animation clips.
- 1D/2D Blend Spaces, transitions/Any State, layers, visual live debugging.
- Package the addon in `package_windows.ps1` and add Windows CI smoke tests.

This MVP is isolated from the existing Blender packaging and does not automatically appear in previously built installers.
