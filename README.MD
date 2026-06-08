# TressFX for Unreal Engine 5.7

An experimental port of **AMD TressFX** for **Unreal Engine 5.7**.

> **Work In Progress**
>
> This project is still under active development.

---

## Overview

This repository contains an experimental port of AMD TressFX for Unreal Engine 5.7.

The current implementation supports importing TressFX assets, physics simulation, and rendering inside the Asset Editor. However, there is still a rendering issue when placing hair into a level.

A Blender plugin is also included in this repository.

---

## Blender Plugin

The Blender plugin can be found under:

```text
Blender/
```

---

## Supported Features

- Import `.tfx` files
- Hair physics simulation
- Correct rendering inside the TFX Asset Editor
- Blender plugin support

---

## Known Issues

Currently, TressFX hair can be rendered correctly inside the **TFX Asset Editor**, but **cannot be rendered correctly inside the Level Viewport or during runtime**.

Interestingly, when the actor is selected, the selection outline correctly displays the hair silhouette, indicating that the geometry exists but is not being rendered correctly by the rendering pipeline.

The cause of this issue is still under investigation.

Contributions and suggestions are welcome.

---

## Current Status

### Supported

- Import `.tfx`
- Physics simulation
- Asset Editor preview rendering
- Blender plugin

### Not Supported

- Rendering hair inside the Level Viewport
- Runtime rendering

---

## Contributing

Pull requests, bug reports, and suggestions are welcome.