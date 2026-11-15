# Handoff: Ryan's hackintosh projects (as of 2026-11-15)

Talk to Ryan in casual short texts (Gen Z style, no dashes, emojis only 😭 ✌ 🫩 🥀 💔 🙏). He often has no phone, so he pastes terminal text or sends photos of the screen. He runs boot/BitLocker/admin changes himself: write .bat scripts, he runs them as admin. Never publish SMBIOS serials (configs in the repo are redacted).

Repo: github.com/itsrecaps/BigSurface-SLS2 (private). CI builds kexts on `runs-on: macos-15-intel` (arm64 queue was backed up).

---

## 1. Laptop: Surface Laptop Studio 2 (WORKING, still laggy)

Hardware: i7-13800H, Iris Xe 8086:A7A0, RTX 4060 (disabled, can't drive the panel, Optimus), ALC274 audio behind SST, THC touchpad + touchscreen, SAM keyboard.
macOS Tahoe 26.7.1 (25G241). OpenCore 1.0.8 on the internal ESP (S: via `mountvol S: /S`). Boot macOS from Windows with `C:\temp\boot-to-macos.bat` (sets BCD bootsequence). SMBIOS MacBookPro16,2, CPUID spoofed to Comet Lake, SIP 0xA13, SecureBootModel Disabled.

### Works
- Keyboard + battery: patched BigSurface (SLS2 HID is on SAM TID 1, older Surfaces TID 2; touchpad made optional in the HID driver). SMCBatteryManager disabled (duplicate BatteryManager class).
- Trackpad cursor + click: own SurfaceTHC kext (Intel THC over HIDSPI, PIO polling, ADL opcode regs 0x1014 RX / 0x1018 TX). Report 0x80 = mouse, fed to a virtual IOHIDDevice. Needs boot-arg `-thctest`.
- Cursor glitch fixed (stolen memory budget fit in NootedGreen; GMS is only 60 MiB).
- Boot logo stretch fixed (UEFI Output Resolution = Max).
- Clamshell notification storm fixed (battery poll only notifies on change).

### Not working / open
- **Lag** (main issue). Graphics is NootedGreen (TGL spoof, experimental, Metal 2). Findings:
  - Screen scans at 120 Hz fine. GPU finishes each frame in ~4 ms. Kernel stamp waits all < 2 ms. Display transactions complete via IRQ, zero timeouts.
  - Ruled out: lost GT interrupts, IDLW busy-wait (now ngreenpreemptwait=250), IGScheduler5 duty-cycle throttle (never fired), blur/transparency, resolution.
  - While dragging Messages only ~7 frames/s get submitted. WindowServer logs `synchronize timed out` for the Messages context. Messages main thread is ~94% idle in mach_msg (not GPU bound, 7% CPU).
  - Current theory: apps' CADisplayLink ticks (vblank) aren't delivered at 120 Hz, so apps only draw when something else wakes them. Latest build counts vblank dispatches (`xe-sls2-vbl-hz`, `vbl-en/dis`). Next test on the laptop: `sleep 4; ioreg -lw0 -r -c IntelAccelerator | grep -oE '"xe-sls2-vbl-[a-z]+" = [0-9]+'` while dragging Messages.
- **Trackpad gestures**: touchpad acks SET_FEATURE 0x84 (Input Mode=3) and 0x85 with HIDSPI type 9 responses but keeps sending only report 0x80; GET_FEATURE readback times out. Needs more research into the mode switch.
- Audio: Tahoe removed AppleHDA; needs OCLP-Mod root patch + KDK (paused).
- Sleep, school WPA2-Enterprise wifi: not started.

---

## 2. Desktop: NEW BUILD (not started)

- CPU i9-14900KF (no iGPU, so the GPU must drive the display)
- Board ASUS TUF GAMING Z790-PLUS WIFI (confirm DDR5 vs D4 variant: DDR5 = WiFi 6E, D4 = AX201). Intel 2.5G Ethernet (AppleIGC), Realtek S1220A audio (AppleALC ALC1220 layouts, but Tahoe needs the AppleHDA root patch), Intel WiFi (itlwm/AirportItlwm, limited on Tahoe; use Ethernet first)
- GPU RTX 4070 via NullMoth nvidia-macos-driver (supports Turing+ incl. RTX 40, Sequoia 15.5+ / Tahoe, mostly tested on RTX 5060; needs OpenCore, specific boot-args, SIP changes, SecureBootModel Disabled; `-nvrmnopm` disables sleep support; `-nvrmnoflip` if desktop flashes/tears)
- Separate SSD for macOS
- **UEFI Secure Boot must stay ON** (games). Plan: create own PK/KEK/db keys, enroll alongside Microsoft keys, sign OpenCore.efi + drivers; re-sign on every OpenCore update (script it). Have the BitLocker recovery key ready. Apple Secure Boot (SecureBootModel) is separate and stays Disabled.
- 13th/14th gen hybrid CPU: standard Alder/Raptor Lake OpenCore setup (ProvideCurrentCpuInfo, CPU spoof as needed).

### Lessons from the laptop that apply
- Write short .bat scripts for anything on the ESP; back up config.plist before every change.
- Keep a known-good kext copy before swapping.
- Test risky kexts behind a boot-arg so a bad build can be disabled.
- Avoid powermetrics (froze the system).
