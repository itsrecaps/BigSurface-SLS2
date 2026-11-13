# BigSurface for Surface Laptop Studio 2

Builds [BigSurface](https://github.com/Xiashangning/BigSurface) with two patches for the SLS2:

- `patches/VoodooSerial-SLS2.patch` adds the SLS2 UART (8086:51A8) so the keyboard and battery chip can talk to macOS
- `patches/BigSurface-SLS2.patch` keeps the keyboard working even if the touchpad part fails to start

GitHub Actions builds it on a Mac runner on every push. The kext is in the run's Artifacts.

## NootedGreen (SLS2 stolen memory fix)

`nootedgreen/` is NootedGreen from JulioNectar/Hackintosh-XPS13Plus-IrisXe-Tahoe (commit 24450b4) with one fix:
the SLS2 firmware sets GMS = 0xFE (GGC 0xFEC1, 60 MiB). Apple's TGL kexts decode that as ~4 GiB, so the
accelerator fails to start and panics in `IOAccelTask::release` (CR2 0xc78). The fix replaces the decode in
`IGMemoryManager::initSegments` and `AppleIntelBaseController::FBMemMgr_Init` with the real size.
