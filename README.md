# waydemo

**waydemo** is a minimal **Wayland‑native demo application** showcasing how to combine:

- **Wayland client API**
- **libdecor** for Client‑Side Decorations (CSD)
- **EGL + OpenGL ES**
- **Runara** as a GPU text and primitive renderer

This project intentionally avoids legacy and heavyweight stacks.

Designed for modern compositors such as **GNOME**, **Weston**, and **Vortex**.

---

## Features

- Native Wayland client
- Automatic client-side decorations via **libdecor**
- Proper resize / move / close handling
- EGL window with OpenGL ES rendering
- Text and primitives rendered using **Runara**
- Single-file demo (`waydemo.c`) for clarity and learning

# Build
```bash
cc waydemo.c -o waydemo \
  `pkg-config --cflags --libs wayland-client libdecor-0 egl glesv2` \
  -lrunara
```