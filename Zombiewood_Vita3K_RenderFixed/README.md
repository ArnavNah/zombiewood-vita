# Zombiewood for Vita3K

Double-click **Launch.cmd**. The VPK is **Zombiewood.vpk**.
Allow about a minute for first startup and shader compilation.

This launcher installs the updated executable, adds missing game data, and uses
Vulkan with memory mapping disabled. It preserves existing data files and backs
up the earlier executable and configuration. The emulator, firmware and shader
compiler are already installed on this PC in the adjacent audit/runtime setup.
Close an existing Vita3K session before using the launcher.

## Controls

| Vita control | PC keyboard in the supplied setup | Action |
| --- | --- | --- |
| Left stick | W/A/S/D | Move; move the gold cursor in menus |
| D-pad | Arrow keys | Move; move the gold cursor in menus |
| Right stick | I/J/K/L | Aim and fire |
| Cross | X | Select at the game cursor; advance tutorial dialogue |
| Circle | C | Back/cancel |
| Start | Enter | Xperia Start / pause |
| L / R | Q / E | Previous / next weapon |
| Square / Triangle | Z / V | Previous / next weapon alternatives |

Menu navigation uses a cursor inside the game and never moves the Windows mouse.
Controller input hides the touch-stick graphics. Front touch displays them again;
the touch targets remain available so the APK's stick logic still works.
The tutorial controls when movement and firing become available.

The Xperia key mappings are for this Android game library inside the Vita port.
This package does not contain a separate Xperia Play Android APK.

## Changes

- Original APK loading artwork, Flash hint animation and progress bar restored.
  Loading buffers retain the previous image for partial animation updates.
- Controller menu cursor, Xperia Start and shoulder events, and automatic hiding
  of touch-stick graphics.
- Correct Android ARM file metadata and directory-entry layouts, including file
  size at byte 48. Unified save paths for open/stat/access/rename/remove.
- Existing legacy app-directory saves migrate when the corresponding data save
  is missing; original files are retained.
- Allocation failure checks and caller logging for further crash diagnosis.

## Verification and limits

The tutorial, combat, firing, hidden stick graphics, and loading transition have
been checked in Vita3K. Static checks cover all 364 Android imports and archive
integrity. See verification.json and runtime-validation.json for precise results.

The reported crash after several missions is still under investigation. The file
metadata defect is corrected, but a full repeated-mission regression test with
that correction is required before calling the crash resolved. Physical Vita and
Xperia Play hardware have not been tested, and no full campaign run is claimed.

Manual setup: install the VPK and copy data/zombiewood-audit into Vita3K's ux0/data.
Use Vulkan, enable mask-bit emulation, disable memory mapping, and enable CPU
optimization. The revised source is ../audit/render-investigation/source; the
primary project remains separate for the other editor.

Release update: continuous stereo PCM buffering, native-resolution middle device profile, shader caching, controller overhead and frame-pacing improvements. The physical Vita package is in ../FOR Vita. Constant 60 FPS and physical-device stability are not established.

