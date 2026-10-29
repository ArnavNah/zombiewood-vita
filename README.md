# Zombiewood for PS Vita

Android ARMv7 v1.5.3 port, hardware revision **01.55**.

**Hardware status:** a Vita-1000 startup failure was reported with revision 01.53.
Revisions 01.54/01.55 address compatibility issues, but successful physical-device
startup has not been confirmed. A constant 60 FPS is not established.

## Download

Install Git LFS, then clone the repository to get actual game files:

```sh
git lfs install
git clone https://github.com/ArnavNah/zombiewood-vita.git
cd zombiewood-vita
git lfs pull
```

Source ZIP downloads may contain LFS pointers instead of the large assets.

## Install on Vita

Use [FOR Vita](FOR%20Vita/): copy its `data` directory to `ux0:`, then install
`Zombiewood.vpk` with VitaShell. The library must be at
`ux0:data/zombiewood/libZombieWood.so`.

Read [INSTALL.txt](FOR%20Vita/INSTALL.txt) for kuBridge, shader compiler, optional
FdFix setup and controls. The Sony shader compiler is not included.
When upgrading from 01.53/01.54, install the VPK; game data is unchanged.
Keep existing save files when updating.

## Original game files

The original APK is retained, and both OBBs remain in the runtime data folder
so they are stored once in the current checkout:

- [Main APK — Zombiewood v1.5.3](originals/Zombiewood-v1.5.3.apk)
- [Main OBB](FOR%20Vita/data/zombiewood/main.1501.com.gameloft.android.ANMP.GloftZOHM.obb)
- [Patch OBB](FOR%20Vita/data/zombiewood/patch.15325.com.gameloft.android.ANMP.GloftZOHM.obb)

The APK library matches the packaged `libZombieWood.so`. APK and hardware-package
checksum lists are included in their respective folders. Game and assets: Gameloft.

## Source and building

The root contains the final port source; `lib/` contains its vendored dependencies
and license notices. Use a compatible softfp VitaSDK with kuBridge, vitaShaRK,
SceShaccCgExt and the other libraries listed in CMakeLists.txt installed.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DUSE_KUBRIDGE=ON -DPORT_VITA3K_COMPAT=OFF \
  -DPORT_SAFE_TEXTURES=ON -DPORT_SHADER_CACHE=ON
cmake --build build --parallel 4
```

The hardware release uses native 960x544, conservative ETC1 decoding, shader
caching and the middle game device profile. It includes Vita controls, continuous
stereo PCM buffering, corrected file/save compatibility and original loading art.
Temporary tracing, test input and profiling are disabled in the packaged VPK.
A small hardware startup log records initialization stages and memory counts.

See [REVIEW.md](REVIEW.md) for the code review. Automated ARM checks passed 12/12
thread tests and 8/8 PCM tests through Vita3K; these do not establish hardware boot.
Package import, executable identity, VPK CRC and both OBB CRC checks passed.

The duplicate PC package, unused alternate implementation files, emulator
storage, temporary builds and local backups are excluded from this checkout.
