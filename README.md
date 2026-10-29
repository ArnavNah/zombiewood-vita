# Zombiewood for PS Vita

Android ARMv7 v1.5.3 port with Vita controls, stereo PCM output, vitaGL rendering,
offline operation, corrected file metadata/save handling, and the original
loading animation.

## Download the complete files

Install Git LFS before cloning. The game assets and packaged binaries use LFS;
an ordinary source ZIP may contain pointer files instead of the game data.

```sh
git lfs install
git clone https://github.com/ArnavNah/zombiewood-vita.git
cd zombiewood-vita
git lfs pull
```

## Hardware package

Use [FOR Vita](FOR%20Vita/). Copy its `data` directory to `ux0:` so the assets are
at `ux0:data/zombiewood/`, then install `Zombiewood.vpk` with VitaShell.
Read [INSTALL.txt](FOR%20Vita/INSTALL.txt) for kuBridge, shader compiler, optional
FdFix setup and controls. The Sony shader compiler is not bundled.

The release uses 960×544 output, the game's middle device profile, conservative ETC1
decoding and shader caching. Diagnostic input, tracing and profiling are disabled.
The included SHA256SUMS.txt lists the package file checksums.

## PC testing

[Zombiewood_Vita3K_RenderFixed](Zombiewood_Vita3K_RenderFixed/) contains a separate
Vita3K-compatible build with software ETC1 decoding and complete data. Vita3K and
its firmware storage are not bundled. Close Vita3K, then run in PowerShell:

```powershell
./Zombiewood_Vita3K_RenderFixed/Launch.ps1 -Vita3KPath 'C:/path/to/Vita3K.exe'
```

The hardware VPK and emulator VPK use different texture and executable-loader
paths; use the package for the intended device.

## Source and building

The repository root contains the matching final port source. Dependencies in
`lib/` are vendored, including local compatibility fixes; no submodule checkout
is needed. Use a compatible softfp VitaSDK with kuBridge, vitaShaRK,
SceShaccCgExt and the other libraries listed in CMakeLists.txt installed.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DUSE_KUBRIDGE=ON -DPORT_VITA3K_COMPAT=OFF -DPORT_SHADER_CACHE=ON
cmake --build build --parallel 4
```

VitaGL builds from its bundled source. The hardware data path defaults to
`ux0:data/zombiewood/`. Original author/license notices are retained in source
files; vitaGL license texts are also in the hardware package.

## Validation and remaining limits

Vita3K rendered menus, original loading artwork and Survivalist combat. Eight
deterministic PCM tests passed for left/right ordering, arbitrary split writes,
paused buffering and output retry. All 364 Android import names are mapped;
VPK and OBB CRCs and packaged executable identity passed verification.

Physical-Vita testing is still outstanding. Stereo buffering was corrected,
but an audible device listening check remains. Constant 60 FPS is not verified:
non-loading combat measurements in Vita3K averaged about 39–54 FPS per bucket.
The earlier fourth-mission crash has not been established as resolved. Test
long sessions and suspend/resume on hardware before relying on stability.

Game and assets: Gameloft. Third-party components retain their own licenses.
This repository contains the requested source, assets and release packages;
temporary profiling builds, emulator firmware/storage, backups and unrelated
Air Attack HD files are excluded.

## Hardware startup revision 01.54

Revision 01.53 was reported to stop on the packaged Android startup image on a
modded Vita-1000, with audio but no Gameloft logo. The data files and compiled
paths are consistent locally; the files actually on the device were not inspected.
Revision 01.54 uses kuBridge with checked executable-memory protection, conservative
ETC1 decoding, a 128 MB application heap and 24 MB renderer memory headroom.
It checks required data paths and records startup stages in
ux0:data/zombiewood/hardware-startup.log. Install only the updated hardware VPK;
the data is unchanged. This revision needs a physical-device retest and does not
establish that the startup failure is resolved. The PC package is retained.

## Compatibility review revision 01.55

The current hardware VPK includes corrected pthread_once completion, serialized
Android mutex/condition creation, preservation of requested thread stack sizes,
64-bit semaphore deadlines and safe logger initialization. The ARM compatibility
test passed 12/12 checks through Vita3K; the test runner is not in the VPK.
See [REVIEW.md](REVIEW.md) for findings, the Backstab port comparison and limits.
Physical-device startup is still not confirmed fixed. Install the updated VPK
over the current application; game data is unchanged.
