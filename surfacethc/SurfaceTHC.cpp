// SurfaceTHC probe: step 1 of a macOS driver for the Surface Laptop Studio 2 touchpad
// (Intel Touch Host Controller in HID-over-SPI mode, PCI 8086:51D0 / 8086:51D1).
// This build is READ ONLY: it maps the controller, reads registers and the ACPI _DSM
// parameters, and publishes them as properties. It never writes to the controller.
// Read the result with:  ioreg -lw0 -c SurfaceTHCProbe
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSArray.h>

class SurfaceTHCProbe : public IOService {
	OSDeclareDefaultStructors(SurfaceTHCProbe)
public:
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
private:
	IOMemoryMap *map {nullptr};
	void readDSM(IOACPIPlatformDevice *acpi, const char *name, const uint8_t *uuid, uint32_t rev, uint32_t maxFunc);
};

OSDefineMetaClassAndStructors(SurfaceTHCProbe, IOService)

// ACPI ToUUID byte order
static const uint8_t kHidSpiGuid[16]   = {0x36,0xc4,0x2a,0x6e, 0xcf,0x0f, 0xaf,0x41, 0xa2,0x65, 0xb3,0x2a,0x22,0x0d,0xcf,0xab};
static const uint8_t kQuickSpiGuid[16] = {0xb7,0x35,0x0d,0x30, 0x20,0xac, 0x3e,0x41, 0x8e,0x9c, 0x92,0xe4,0xda,0xfd,0x0a,0xfe};
static const uint8_t kPlatformGuid[16] = {0x82,0x56,0x00,0x84, 0x71,0x5b, 0xa4,0x41, 0x8d,0x66, 0x81,0x30,0xf7,0x87,0xa1,0x38};

static const uint32_t kRegs[] = {
	0x0000, 0x0004, 0x0008, 0x000C, 0x0010, 0x0014, 0x0018, 0x001C,
	0x1008, 0x1010, 0x1014, 0x1018, 0x101C, 0x1020, 0x1024, 0x1028,
	0x1040, 0x1044, 0x1048, 0x10B4, 0x10B8, 0x10BC, 0x10EC, 0x10F0,
	0x1108, 0x110C, 0x1110, 0x1114, 0x1170,
};

static void setNum(OSDictionary *d, const char *key, uint64_t v, int bits) {
	OSNumber *n = OSNumber::withNumber(v, bits);
	if (n) { d->setObject(key, n); n->release(); }
}

void SurfaceTHCProbe::readDSM(IOACPIPlatformDevice *acpi, const char *name, const uint8_t *uuid, uint32_t rev, uint32_t maxFunc) {
	OSDictionary *out = OSDictionary::withCapacity(8);
	if (!out) return;
	for (uint32_t f = 0; f <= maxFunc; f++) {
		OSData *u = OSData::withBytes(uuid, 16);
		OSNumber *r = OSNumber::withNumber(rev, 32);
		OSNumber *fn = OSNumber::withNumber(f, 32);
		OSArray *pkg = OSArray::withCapacity(1);
		OSObject *params[4] = {u, r, fn, pkg};
		OSObject *result = nullptr;
		char key[16];
		snprintf(key, sizeof(key), "func%u", f);
		IOReturn ret = (u && r && fn && pkg) ? acpi->evaluateObject("_DSM", &result, params, 4) : kIOReturnNoMemory;
		if (ret == kIOReturnSuccess && result) {
			out->setObject(key, result);
			result->release();
		} else {
			char err[32];
			snprintf(err, sizeof(err), "error 0x%x", ret);
			OSString *s = OSString::withCString(err);
			if (s) { out->setObject(key, s); s->release(); }
		}
		OSSafeReleaseNULL(u); OSSafeReleaseNULL(r); OSSafeReleaseNULL(fn); OSSafeReleaseNULL(pkg);
	}
	setProperty(name, out);
	out->release();
}

bool SurfaceTHCProbe::start(IOService *provider) {
	if (!IOService::start(provider)) return false;
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci) return false;

	OSDictionary *cfg = OSDictionary::withCapacity(8);
	if (cfg) {
		setNum(cfg, "vendor-device", pci->configRead32(kIOPCIConfigVendorID), 32);
		setNum(cfg, "command-status", pci->configRead32(kIOPCIConfigCommand), 32);
		setNum(cfg, "class-rev", pci->configRead32(kIOPCIConfigRevisionID), 32);
		setNum(cfg, "bar0", pci->configRead32(kIOPCIConfigBaseAddress0), 32);
		setNum(cfg, "bar1", pci->configRead32(kIOPCIConfigBaseAddress1), 32);
		setNum(cfg, "subsystem", pci->configRead32(kIOPCIConfigSubSystemVendorID), 32);
		setProperty("THC-config", cfg);
		cfg->release();
	}

	IOACPIPlatformDevice *acpi = OSDynamicCast(IOACPIPlatformDevice, pci->getProperty("acpi-device"));
	if (acpi) {
		setProperty("THC-acpi", acpi->getName());
		readDSM(acpi, "DSM-hidspi", kHidSpiGuid, 2, 6);
		readDSM(acpi, "DSM-quickspi", kQuickSpiGuid, 2, 3);
		readDSM(acpi, "DSM-platform", kPlatformGuid, 2, 2);
	} else {
		setProperty("THC-acpi", "not found");
	}

	pci->setMemoryEnable(true);
	map = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
	if (map) {
		setNum(getPropertyTable(), "THC-bar0-length", map->getLength(), 64);
		OSDictionary *regs = OSDictionary::withCapacity(sizeof(kRegs) / sizeof(kRegs[0]));
		if (regs) {
			for (size_t i = 0; i < sizeof(kRegs) / sizeof(kRegs[0]); i++) {
				if (kRegs[i] + 4 > map->getLength()) continue;
				uint32_t v = *reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress() + kRegs[i]);
				char key[12];
				snprintf(key, sizeof(key), "0x%04x", kRegs[i]);
				setNum(regs, key, v, 32);
			}
			setProperty("THC-regs", regs);
			regs->release();
		}
	} else {
		setProperty("THC-bar0-length", "map failed");
	}

	IOLog("SurfaceTHC: probe done on %s\n", provider->getName());
	registerService();
	return true;
}

void SurfaceTHCProbe::stop(IOService *provider) {
	OSSafeReleaseNULL(map);
	IOService::stop(provider);
}
