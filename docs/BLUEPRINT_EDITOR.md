# Hamun Blueprint Graph Editor — first interactive slice

Use **Blueprint > Open Blueprint Graph...** from HamunEditor.

- Press **1** Event Begin Play, **2** Print String, **3** Return,
  **4** Branch, **5** Add Float to add node cards.
- Left-drag any card to position it.
- Click a node's right-hand execution socket, then another node's
  left-hand socket to connect them. An input has one incoming edge.
- **Ctrl+S** saves a `.hamunblueprint` text graph. **Ctrl+O** opens one.
- Stored records are versioned and validated before replacing the current graph.

**Current limitations:** This is an initial native Win32 *visual graph editor*.
Graph connections are persisted, **not compiled into HamunGraph bytecode**.
The nodes do not execute gameplay logic yet. Typed data pins, undo/redo,
zoom/pan, context menu, graph search, graph debugger, docking, execution wiring
and runtime component binding remain necessary for a genuine UE5-level workflow.

The next milestone is graph compiler + runtime bindings, followed by
property details and dockable tabbed editor workspace. No Epic/Unreal UI
assets or source code are included.
