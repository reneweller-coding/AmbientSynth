# AmbientSynth for Meta Quest

Native OpenXR app, no game engine: `NativeActivity` + `android_native_app_glue`,
EGL/GLES 3, the Khronos OpenXR loader, `XR_EXT_hand_tracking`, Oboe for audio,
and the unchanged synthesizer core from `../Core`.

```
Quest/
  CMakeLists.txt        NDK build of libambientquest.so (links AmbientCore, oboe, openxr_loader)
  AndroidManifest.xml   NativeActivity, hasCode=false, hand-tracking permission/features, VR category
  src/main.cpp          the app: OpenXR session, hands -> GestureLayer -> Engine, Oboe, GLES scene, OSC bridge
  fetch_thirdparty.ps1  downloads the OpenXR loader (prefab AAR) and Oboe into ../ThirdParty
  build_apk.ps1         CMake/NDK -> aapt2 -> jar -> zipalign -> apksigner (debug key)
```

## Build

```
powershell -File Quest\fetch_thirdparty.ps1
powershell -File Quest\build_apk.ps1
adb install -r build-quest\AmbientSynthQuest.apk
```

Needs: NDK r27 (`C:\Android-Buildtools\sdk\ndk\27.2.12479018`), build-tools 34,
platform android-34, JDK 17 — see the parameters at the top of `build_apk.ps1`.

## Config (optional)

`adb push ambient.cfg /sdcard/Android/data/com.reneweller.ambientsynth.quest/files/ambient.cfg`

```
osc_host=192.168.1.20     # bridge mode: stream hands/head as OSC to the desktop plugin
osc_port=9000
audio=1                   # 0 = no on-device audio (pure bridge)
preset=Sleep Concert      # a full preset by name
```

## What it does

* Every frame: hand joints → palm position, pinch (thumb tip to index tip),
  palm roll → `GestureLayer::setHand`; head pose → `setHead`; then
  `GestureLayer::update` writes parameters into the engine. Right pinch is
  the clutch, hand distance is the morph, heights are depth and brightness,
  tilts are Cosmos send and far level (defaults from the core).
* Audio: Oboe low-latency float stream, `Engine::process` in the callback.
* Picture: soft points. Sounding notes sit around the listener by pitch
  class, octave as height, distance as radius (near warm, far blue); the
  brain's root is orange on the floor; hands are green, red while pinching,
  with a dotted bridge between them that fills up with the morph position.
* Bridge: with `osc_host` set, the same hand/head data goes out as OSC so the
  desktop plugin can be played from the headset.
* Hand menu (`ambient/Menu.h`): hold the **left** pinch to open a head-locked
  panel (text drawn as dots with a 5×7 font), choose with the **right** hand's
  height, activate with the **right** pinch. Items: morph on/off, A = now,
  B = now, A/B previous/next preset (the names show on the panel), record
  start/stop, calibrate. While the menu is open the clutched mappings hold.
* Calibration: on first start (no `calib.txt`) and from the menu, 8 seconds of
  "hands together and apart, low and high, near and far"; the ranges are
  saved to `calib.txt` in the app's external data folder.
* Recording: `rec-YYYYMMDD-HHMMSS.wav` (32-bit float, the stream's rate) in the
  same folder, written by a background thread from a lock-free ring.
* Notes glow with their envelope level (`Engine::noteLevel`).

## Status

Compiles and packages; not yet run on a device (no headset attached to the
build machine). First on-device checks: session state flow, swapchain format,
hand-tracking permission prompt, Oboe stream start, pinch calibration.
