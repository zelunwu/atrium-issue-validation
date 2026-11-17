# Graphics coverage audit (Atrium `63ea4ed`, 2026-11-17)

The C++ executable produces 147 PNGs, numbered `000`–`146`. The executable invokes the original graphics implementations except for the narrowly documented readback and fixture changes generated in `native/build/`. This is a visual baseline for the listed scenes, not proof of exhaustive branch or system integration coverage.

| Source / interface | Images and execution | Remaining gap |
| --- | --- | --- |
| `glass.hlsl` VS/PS | 001–011, 020–023; original HLSL on D3D11; 144–146 via `GlassRenderer::Present` (basic, layered, complex) | Other parameter combinations, ROI offsets and sizes |
| `backdrop_edge.hlsl` PS; `compare.hlsl` CS | 015 and 014; original HLSL on D3D11; renderer loads both for 144–146 | Full renderer readback queue, capture ROI, multi-monitor boundaries |
| `corner_profile.h`, `callout_profile.h`, `continuous_corners.h` | 012–013, 016–017, 025; original CPU/Direct2D code | Boundary matrix and cache lifecycle |
| `wifi_glyph.cpp`, `round_stroke.h`, `hover_wave.cpp` | 018–019, also used by UI scenes | Signal threshold and animation state matrix |
| `ui_font.cpp` | 024 and UI text scenes; original DirectWrite code with bundled MiSans | Font fallback across Windows versions |
| `bar_painter.cpp` | 026; original `Animate` and `Draw` with synthetic bar scene | Drag, alert, auto-hide and every status variant |
| `glass_panel.cpp` | Linked by the executable; panel synchronization and ink helpers are outside the numbered image fixtures | `InkFade`, `InkField`, panel tone and generation messages need focused state tests; they do not independently draw pixels |
| `widget_faces.cpp` | 027–104; every supported kind × size × material combination | Content variants, live network and photo data |
| `Canvas` in `surface.cpp` | 105–126; 22 scenes covering fill, stroke, text, images, blur, clip, mask, ripple, callout and geometry | Every argument and nested state combination |
| `glass_menu.cpp`, `calendar_panel.cpp` | 127–128; original paint paths with fixed menu/date | Hover, scrolling and interaction states |
| `notifications.cpp` | 129–130; popup and centre at a fixed visible animation state | Stacking, privacy and dismissal states |
| `control_center.cpp` | 131–135; main, Wi-Fi, Bluetooth, sound, cast pages | Live radio, audio, display lists and interactions |
| `window_preview.cpp` | 136; overlay with synthetic window | DWM thumbnail is a separate composition visual and absent from PNG |
| `start_menu.cpp` | 137, 139–141; home, all apps, power and search with synthetic catalog | Folder, resize, drag, sync and other interaction states |
| `tray_panel.cpp` | 138; synthetic tray icons | Real Explorer tray, hosted icon and click paths |
| `glass_renderer.cpp`, `gpu.cpp`, `capture.cpp`, `desktop_backdrop.cpp` | 144–146 call actual `UpdateBackdrop` and both `Present` overloads, including a DirectComposition swap chain; the harness selects a hardware D3D11 device or WARP | Live capture, `Gpu::ForMonitor`, desktop backdrop lifecycle, frame pacing, full readback queue and window exclusion |
| `desktop_widgets.cpp`, `stage_manager.cpp`, `window_snapshot.cpp` | 142 invokes Stage Manager's picture paint with a synthetic perspective pose; 143 invokes DesktopWidgets container paint with a fixed widget | Desktop placement, window parking, perspective animation and live snapshots |

Private UI page changes use methods added only to generated copies of headers and sources in `native/build/`; the Atrium repository is unmodified. The synthetic catalog, tray icons, date, weather and desktop data keep images shareable and repeatable. Control Center background readers are disabled in its generated copy. Notification entry animation is set to its visible state for a useful static baseline. The `Surface` readback hook records the painted D2D buffer before presentation.

**Scope limit:** “all graphics interfaces” cannot mean every runtime state or every possible pixel output. The table lists each graphics module found under `native/core/render` and `native/core/ui`, marks the directly exercised paths, and calls out the system-dependent paths that do not have a trustworthy unattended image fixture yet. The current 147-image pass must not be described as full product visual acceptance.
