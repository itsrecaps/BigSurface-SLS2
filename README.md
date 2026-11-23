# macOS Tahoe on the Surface Laptop Studio 2

Hackintosh project for a Surface Laptop Studio 2 (i7-13800H, Iris Xe 8086:A7A0, RTX 4060, 64 GB RAM, AX210 wifi).
Running **macOS Tahoe 26.7.1 (25G241)** with OpenCore 1.0.8, based on
[JulioNectar/Hackintosh-XPS13Plus-IrisXe-Tahoe](https://github.com/JulioNectar/Hackintosh-XPS13Plus-IrisXe-Tahoe).

## Status

| Thing | Status |
| --- | --- |
| Install / boot | Works. OpenCore lives on the internal SSD's EFI partition, no USB needed |
| Graphics acceleration | **Works.** Metal 2, 1536 MB, 2400x1600 with the patched NootedGreen in this repo |
| Cursor | Glitched blob (hardware cursor). Not fixed yet |
| Smoothness | Scrolling laggy. Debug boot args still on (IGLogLevel=3, -v, debug=0x100), turn them off next |
| USB | Works with the custom USBToolBox map (UTBMap.kext) |
| Wifi | Works on WPA2 personal (AirportItlwm). School WPA2 Enterprise not working yet |
| Bluetooth | Kexts in, not tested |
| Keyboard | Working. BigSurface + SSDT-SSH + TigerLake GPIO, SAM HID on TID 1 |
| Battery % | Working. BigSurface battery driver (SMCBatteryManager must be off) |
| Touchpad | Not working. No macOS driver exists. Started one: `surfacethc/` (read only probe so far) |
| Audio | Not working. Tahoe dropped AppleHDA, needs the AppleHDA root patch + AppleALC layout 35 |
| Sleep | Probably impossible, firmware only has S4/S5 (no S3) |
| dGPU (RTX 4060) | Disabled (-wegnoegpu). Never works on macOS |

## Repo layout

- `efi/EFI` - the EFI that's on the SSD. **Serials are removed**, put your own SMBIOS (MacBookPro16,2) back in before using
  - `config.plist` - graphics accelerated config (what boots now)
  - `config-accel-thc.plist` - same plus the touchpad probe kext
  - `config-safemode.plist` - Safe Mode, no graphics driver (the "get me back in" config)
  - `config-install.plist` - iGPU spoof without NootedGreen, used to run the repo's install.sh
  - every config has `Misc/Debug/Target = 0` (see "SSD log freeze" below)
- `nootedgreen/` - NootedGreen with the SLS2 stolen memory fix (GitHub Actions builds it)
- `patches/` - BigSurface + VoodooSerial patches for the SLS2 keyboard UART (GitHub Actions builds it)
- `surfacethc/` - start of a touchpad driver for the Intel THC controller (GitHub Actions builds it)
- `scripts/windows/` - the .bat helpers used from Windows

## Big fixes found along the way

1. **Boot hang after `ktriage_register_subsystem_strings`**: the SLS2 has no PNP0B00 RTC (only the Surface `SRTC`).
   Fixed with `SSDT-RTC0` (fake RTC0, Darwin only).
2. **No USB at all**: needs the USBToolBox map. XHCI is cut to 15 ports (dropped SS04).
3. **Recovery entry hidden / dmg not loading**: `HideAuxiliary = false` and `HfsPlus.efi`.
4. **SSD log freeze**: booting OpenCore from the internal SSD froze at `EB|LOG:EXITBS:START` while the same EFI booted
   fine from USB. Cause: OpenCore file logging (`Target = 65`) writing to the SSD at ExitBootServices. `Target = 0` fixed it.
5. **Graphics panic** (`IOAccelTask::release`, CR2 0xc78, through `IGAccelTask::withOptions`): NOT the macOS version.
   The SLS2 firmware sets GGC = 0xFEC1 (GMS 0xFE = 60 MiB). Apple's TGL kexts decode GMS as *32 MiB, wrap to ~4 GiB, the
   accelerator's allocators fail and it panics. The patched NootedGreen replaces that decode in
   `IGMemoryManager::initSegments` and `AppleIntelBaseController::FBMemMgr_Init` with the real 60 MiB.
   GGC was read with OpenShell: `pci 00 02 00`, row 0x50.
6. **Holding Volume Down to boot USB breaks OpenCore's Enter key** (it thinks a key is held). Use
   `scripts/windows/boot-to-macos.bat` instead (one time boot into OpenCore).
7. **Windows clock 5 hours ahead**: macOS keeps the RTC in UTC. In Windows (admin):
   `reg add "HKLM\SYSTEM\CurrentControlSet\Control\TimeZoneInformation" /v RealTimeIsUniversal /t REG_QWORD /d 1 /f`
8. **Windows freezes when picked from the OpenCore menu**. Boot Windows from Windows Boot Manager (first in boot order).

## Booting

- Windows: just turn it on (Windows Boot Manager is first)
- macOS: in Windows run `scripts/windows/boot-to-macos.bat` as admin, pick **Tahoe2671**
- Turn off macOS auto updates. Setup Assistant downloaded 26.7.3 by itself the first time.

## To do

1. Turn off the debug boot args for speed
2. Fix the cursor blob
3. Install the BigSurface keyboard build, then battery
4. Touchpad: run the probe, then port Linux's Intel THC QuickSPI driver step by step
5. Audio (AppleHDA root patch)
6. Delete the old 26.7.3 volume ("Macintosh HD") once everything's stable
7. Windows through OpenCore (Booter quirks)
