# BigSurface for Surface Laptop Studio 2

Builds [BigSurface](https://github.com/Xiashangning/BigSurface) with two patches for the SLS2:

- `patches/VoodooSerial-SLS2.patch` adds the SLS2 UART (8086:51A8) so the keyboard and battery chip can talk to macOS
- `patches/BigSurface-SLS2.patch` keeps the keyboard working even if the touchpad part fails to start

GitHub Actions builds it on a Mac runner on every push. The kext is in the run's Artifacts.
