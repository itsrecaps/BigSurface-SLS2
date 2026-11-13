// SurfaceTHC probe: step 1 of a macOS driver for the Surface Laptop Studio 2 touchpad
// (Intel Touch Host Controller in HID-over-SPI mode, PCI 8086:51D0 / 8086:51D1).
// Step 2: brings the controller up the way Linux intel-quickspi does (quiesce, SPI config,
// ACPI _RST, wait for RESET_RESPONSE, then ask for the HIDSPI device descriptor) using PIO
// and polling only (no DMA, no interrupts). Results are published under "THC-bringup".
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
#include "intel-thc-hw.h"
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>
#include <mach/mach_time.h>

class SurfaceTHCProbe : public IOService {
	OSDeclareDefaultStructors(SurfaceTHCProbe)
public:
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
private:
	IOMemoryMap *map {nullptr};
	uint32_t bodyAddr {0};
	volatile bool listening {false}, stopping {false};
	char acpiName[8] {0};
	IOACPIPlatformDevice *acpiDev {nullptr};
	IOService *mouse {nullptr};
	static bool checkThcTest() { int v = 0; return PE_parse_boot_argn("-thctest", &v, sizeof(v)); }
	static void listenEntry(void *arg, wait_result_t);
	void listen();
	void readDSM(IOACPIPlatformDevice *acpi, const char *name, const uint8_t *uuid, uint32_t rev, uint32_t maxFunc);
	uint64_t dsmInt(IOACPIPlatformDevice *acpi, const uint8_t *uuid, uint32_t func, bool *ok);
	uint32_t rd(uint32_t off) { return *reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress() + off); }
	void wr(uint32_t off, uint32_t v) { *reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress() + off) = v; }
	void wbits(uint32_t off, uint32_t mask, uint32_t v) { wr(off, (rd(off) & ~mask) | (v & mask)); }
	bool pollBits(uint32_t off, uint32_t mask, uint32_t want, uint32_t ms);
	int pioPrepare(uint8_t op, uint32_t addr, uint32_t size);
	int pioWait();
	int pioRead(uint32_t addr, uint32_t size, uint32_t *buf, uint32_t *got);
	int pioWrite(uint32_t addr, uint32_t size, const uint32_t *buf);
	bool waitNonDmaInt(uint32_t ms, uint32_t *cause);
	void bringUp(IOACPIPlatformDevice *acpi);
	OSDictionary *log {nullptr};
	void note(const char *k, uint64_t v) { setNum2(log, k, v); }
	void noteStr(const char *k, const char *v) { OSString *s = OSString::withCString(v); if (s) { log->setObject(k, s); s->release(); } }
	void noteBytes(const char *k, const void *p, uint32_t n) { OSData *d = OSData::withBytes(p, n); if (d) { log->setObject(k, d); d->release(); } }
	static void setNum2(OSDictionary *d, const char *key, uint64_t v) { OSNumber *n = OSNumber::withNumber(v, 64); if (n) { d->setObject(key, n); n->release(); } }
};

OSDefineMetaClassAndStructors(SurfaceTHCProbe, IOService)

// ---------- SurfaceTHCMouse: the touchpad's built-in mouse collection (report id 0x80) ----------
// Until a precision-touchpad driver switches it to multitouch mode, the SLS2 touchpad sends plain
// relative mouse reports: [buttons][dx int8][dy int8]. Expose that as a standard HID mouse.
class SurfaceTHCMouse : public IOHIDDevice {
	OSDeclareDefaultStructors(SurfaceTHCMouse)
public:
	IOReturn newReportDescriptor(IOMemoryDescriptor **desc) const override;
	OSString *newTransportString() const override { return OSString::withCString("SPI"); }
	OSString *newProductString() const override { return OSString::withCString("Surface Touchpad (THC)"); }
	OSString *newManufacturerString() const override { return OSString::withCString("Microsoft"); }
	OSNumber *newVendorIDNumber() const override { return OSNumber::withNumber(0x045E, 32); }
	OSNumber *newProductIDNumber() const override { return OSNumber::withNumber(0x0C46, 32); }
	OSNumber *newPrimaryUsagePageNumber() const override { return OSNumber::withNumber(0x01, 32); }
	OSNumber *newPrimaryUsageNumber() const override { return OSNumber::withNumber(0x02, 32); }
	void send(const uint8_t *r3);
private:
	IOBufferMemoryDescriptor *buf {nullptr};
	void free() override { OSSafeReleaseNULL(buf); IOHIDDevice::free(); }
};
OSDefineMetaClassAndStructors(SurfaceTHCMouse, IOHIDDevice)

static const uint8_t kMouseDesc[] = {
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
	0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
	0x95, 0x01, 0x75, 0x05, 0x81, 0x03,
	0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
	0xC0, 0xC0,
};

IOReturn SurfaceTHCMouse::newReportDescriptor(IOMemoryDescriptor **desc) const {
	IOBufferMemoryDescriptor *b = IOBufferMemoryDescriptor::withBytes(kMouseDesc, sizeof(kMouseDesc), kIODirectionNone);
	if (!b) return kIOReturnNoMemory;
	*desc = b;
	return kIOReturnSuccess;
}

void SurfaceTHCMouse::send(const uint8_t *r3) {
	if (!buf) buf = IOBufferMemoryDescriptor::withCapacity(3, kIODirectionNone);
	if (!buf) return;
	buf->setLength(3);
	buf->writeBytes(0, r3, 3);
	handleReport(buf, kIOHIDReportTypeInput);
}


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


// ---------- bring-up (ported from Linux intel-thc-dev.c / quickspi, GPL-2.0) ----------
#define SPI_WRITE_IO_MODE   BIT(13)
#define SPI_IO_MODE_SHIFT   14
#define NONDMA_STS          THC_M_PRT_READ_DMA_INT_STS_NONDMA_INT_STS

uint64_t SurfaceTHCProbe::dsmInt(IOACPIPlatformDevice *acpi, const uint8_t *uuid, uint32_t func, bool *ok) {
	OSData *u = OSData::withBytes(uuid, 16);
	OSNumber *r = OSNumber::withNumber(2, 32);
	OSNumber *fn = OSNumber::withNumber(func, 32);
	OSArray *pkg = OSArray::withCapacity(1);
	OSObject *params[4] = {u, r, fn, pkg};
	OSObject *res = nullptr;
	uint64_t v = 0; *ok = false;
	if (u && r && fn && pkg && acpi->evaluateObject("_DSM", &res, params, 4) == kIOReturnSuccess && res) {
		if (OSNumber *n = OSDynamicCast(OSNumber, res)) { v = n->unsigned64BitValue(); *ok = true; }
		else if (OSData *d = OSDynamicCast(OSData, res)) {
			const uint8_t *b = (const uint8_t *)d->getBytesNoCopy();
			for (unsigned i = 0; i < d->getLength() && i < 8; i++) v |= (uint64_t)b[i] << (8 * i);
			*ok = d->getLength() > 0;
		}
	}
	OSSafeReleaseNULL(res); OSSafeReleaseNULL(u); OSSafeReleaseNULL(r); OSSafeReleaseNULL(fn); OSSafeReleaseNULL(pkg);
	return v;
}

bool SurfaceTHCProbe::pollBits(uint32_t off, uint32_t mask, uint32_t want, uint32_t ms) {
	for (uint32_t i = 0; i < ms * 10; i++) {
		if ((rd(off) & mask) == want) return true;
		IODelay(100);
	}
	return (rd(off) & mask) == want;
}

int SurfaceTHCProbe::pioPrepare(uint8_t op, uint32_t addr, uint32_t size) {
	uint32_t sts = rd(THC_M_PRT_SW_SEQ_STS_OFFSET);
	if (sts & THC_M_PRT_SW_SEQ_STS_THC_SS_CIP) return -1;
	wr(THC_M_PRT_SW_SEQ_STS_OFFSET, sts | THC_M_PRT_SW_SEQ_STS_THC_SS_ERR | THC_M_PRT_SW_SEQ_STS_TSSDONE);
	uint32_t ctrl = (size << 16) | ((uint32_t)op << 8);   // BC 31:16, CMD 15:8, no CD_IE (polling)
	wbits(THC_M_PRT_SW_SEQ_CNTRL_OFFSET, THC_M_PRT_SW_SEQ_CNTRL_THC_SS_BC | THC_M_PRT_SW_SEQ_CNTRL_THC_SS_CMD | THC_M_PRT_SW_SEQ_CNTRL_THC_SS_CD_IE, ctrl);
	wr(THC_M_PRT_SW_SEQ_DATA0_ADDR_OFFSET, addr);
	return 0;
}

int SurfaceTHCProbe::pioWait() {
	for (int i = 0; i < 100000; i++) {   // ~1 s
		uint32_t sts = rd(THC_M_PRT_SW_SEQ_STS_OFFSET);
		if (!(sts & THC_M_PRT_SW_SEQ_STS_THC_SS_CIP) && (sts & THC_M_PRT_SW_SEQ_STS_TSSDONE)) return 0;
		IODelay(10);
	}
	return -2;
}

int SurfaceTHCProbe::pioRead(uint32_t addr, uint32_t size, uint32_t *buf, uint32_t *got) {
	if (size == 0 || size > 64) return -3;
	if (pioPrepare(THC_PIO_OP_SPI_TIC_READ, addr, size)) return -1;
	wbits(THC_M_PRT_SW_SEQ_CNTRL_OFFSET, THC_M_PRT_SW_SEQ_CNTRL_TSSGO, THC_M_PRT_SW_SEQ_CNTRL_TSSGO);
	if (pioWait()) return -2;
	uint32_t sts = rd(THC_M_PRT_SW_SEQ_STS_OFFSET);
	if (sts & THC_M_PRT_SW_SEQ_STS_THC_SS_ERR) { wr(THC_M_PRT_SW_SEQ_STS_OFFSET, sts); return -4; }
	*got = (rd(THC_M_PRT_SW_SEQ_CNTRL_OFFSET) >> 16) & 0xFFFF;
	for (uint32_t i = 0; i < (*got + 3) / 4 && i < 16; i++) buf[i] = rd(THC_M_PRT_SW_SEQ_DATA1_OFFSET + 4 * i);
	wr(THC_M_PRT_SW_SEQ_STS_OFFSET, sts | THC_M_PRT_SW_SEQ_STS_THC_SS_ERR | THC_M_PRT_SW_SEQ_STS_TSSDONE);
	return 0;
}

int SurfaceTHCProbe::pioWrite(uint32_t addr, uint32_t size, const uint32_t *buf) {
	if (size == 0 || size > 64) return -3;
	if (pioPrepare(THC_PIO_OP_SPI_TIC_WRITE, addr, size)) return -1;
	for (uint32_t i = 0; i < (size + 3) / 4; i++) wr(THC_M_PRT_SW_SEQ_DATA1_OFFSET + 4 * i, buf[i]);
	wbits(THC_M_PRT_SW_SEQ_CNTRL_OFFSET, THC_M_PRT_SW_SEQ_CNTRL_TSSGO, THC_M_PRT_SW_SEQ_CNTRL_TSSGO);
	if (pioWait()) return -2;
	uint32_t sts = rd(THC_M_PRT_SW_SEQ_STS_OFFSET);
	wr(THC_M_PRT_SW_SEQ_STS_OFFSET, sts | THC_M_PRT_SW_SEQ_STS_THC_SS_ERR | THC_M_PRT_SW_SEQ_STS_TSSDONE);
	return (sts & THC_M_PRT_SW_SEQ_STS_THC_SS_ERR) ? -4 : 0;
}

bool SurfaceTHCProbe::waitNonDmaInt(uint32_t ms, uint32_t *cause) {
	for (uint32_t i = 0; i < ms; i++) {
		if (rd(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET) & NONDMA_STS) {
			*cause = rd(THC_M_PRT_DEV_INT_CAUSE_REG_VAL_OFFSET);
			wr(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET, NONDMA_INT_STS_BIT);
			return true;
		}
		IOSleep(1);
	}
	*cause = rd(THC_M_PRT_DEV_INT_CAUSE_REG_VAL_OFFSET);
	return false;
}

static uint8_t freqDiv(uint32_t f) {
	static const uint32_t fr[] = {7812500, 10416700, 15625000, 17857100, 20833000, 25000000, 31250000, 35714200, 41666700, 50000000};
	static const uint8_t dv[] = {2, 1, 1, 7, 6, 5, 4, 3, 3, 2};
	for (int i = 9; i >= 0; i--) if (f >= fr[i]) return dv[i];
	return 0;
}

void SurfaceTHCProbe::bringUp(IOACPIPlatformDevice *acpi) {
	log = OSDictionary::withCapacity(32);
	if (!log) return;
	bool ok1, ok2, ok3, ok4, ok5, ok6, ok7;
	uint32_t hdr = (uint32_t)dsmInt(acpi, kHidSpiGuid, 1, &ok1);
	uint32_t bdy = (uint32_t)dsmInt(acpi, kHidSpiGuid, 2, &ok2);
	uint32_t out = (uint32_t)dsmInt(acpi, kHidSpiGuid, 3, &ok3);
	uint32_t rop = (uint32_t)dsmInt(acpi, kHidSpiGuid, 4, &ok4) & 0xFF;
	uint32_t wop = (uint32_t)dsmInt(acpi, kHidSpiGuid, 5, &ok5) & 0xFF;
	uint32_t iom = (uint32_t)dsmInt(acpi, kHidSpiGuid, 6, &ok6);
	uint32_t freq = (uint32_t)dsmInt(acpi, kQuickSpiGuid, 1, &ok7);
	if (!(ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7)) { noteStr("result", "DSM read failed"); goto done; }
	{
	uint32_t rmode = (iom >> SPI_IO_MODE_SHIFT) & 3;
	uint32_t wmode = (iom & SPI_WRITE_IO_MODE) ? rmode : 0;
	uint8_t div = freqDiv(freq);
	const uint32_t mps = 0x80;   // ithc: 0x80 unless ACPI limit_packet_size == 1
	note("ctrl-before", rd(THC_M_PRT_CONTROL_OFFSET));
	note("spicfg-before", rd(THC_M_PRT_SPI_CFG_OFFSET));

	// 1. quiesce
	uint32_t ctrl = rd(THC_M_PRT_CONTROL_OFFSET);
	if (!(ctrl & THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN))
		wbits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN);
	if (!pollBits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_HW_STS, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_HW_STS, 1000)) {
		noteStr("result", "quiesce timeout"); goto done;
	}
	// 2. SPI port: CS assert delay, port type SPI (0)
	wbits(THC_M_PRT_SPI_DUTYC_CFG_OFFSET, THC_M_PRT_SPI_DUTYC_CFG_SPI_CSA_CK_DELAY_EN | THC_M_PRT_SPI_DUTYC_CFG_SPI_CSA_CK_DELAY_VAL,
	      THC_M_PRT_SPI_DUTYC_CFG_SPI_CSA_CK_DELAY_EN | (THC_CSA_CK_DELAY_VAL_DEFAULT << __builtin_ctz(THC_M_PRT_SPI_DUTYC_CFG_SPI_CSA_CK_DELAY_VAL)));
	wbits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_PORT_TYPE, 0);
	// 3. addresses
	wr(THC_M_PRT_DEV_INT_CAUSE_ADDR_OFFSET, hdr);
	wr(THC_M_PRT_RD_BULK_ADDR_1_OFFSET, bdy);
	wr(THC_M_PRT_RD_BULK_ADDR_2_OFFSET, bdy);
	wr(THC_M_PRT_WR_BULK_ADDR_OFFSET, out);
	// 4. read/write config (32 MHz -> no half divider, not low freq)
	wbits(THC_M_PRT_SPI_DUTYC_CFG_OFFSET, THC_M_PRT_SPI_DUTYC_CFG_SPI_TCRF_HALF_DIV_EN | THC_M_PRT_SPI_DUTYC_CFG_SPI_TCWF_HALF_DIV_EN, 0);
	wbits(THC_M_PRT_SPARE_REG_OFFSET, THC_M_PRT_SPARE_REG_SPI_CLK_INV_ENABLE, 0);
	wbits(THC_M_PRT_SPI_CFG_OFFSET,
	      THC_M_PRT_SPI_CFG_SPI_TCRF | THC_M_PRT_SPI_CFG_SPI_TRMODE | THC_M_PRT_SPI_CFG_SPI_LOW_FREQ_EN | THC_M_PRT_SPI_CFG_SPI_RD_MPS |
	      THC_M_PRT_SPI_CFG_SPI_TCWF | THC_M_PRT_SPI_CFG_SPI_TWMODE | THC_M_PRT_SPI_CFG_SPI_WR_MPS,
	      ((uint32_t)div << 4) | (rmode << 2) | (mps << 7) | ((uint32_t)div << 20) | (wmode << 18) | ((mps & 0xFF) << 24));
	// ADL opcode layout (from linux-surface ithc): 0x1014 = RX, 0x1018 = TX, 0x101C unused.
	// Each register holds the same opcode in its header/quad/dual/single bytes.
	auto opc = [](uint32_t, uint32_t op) -> uint32_t { return op * 0x01010101u; };
	wr(THC_M_PRT_SPI_ICRRD_OPCODE_OFFSET, opc(rmode, rop));
	wr(THC_M_PRT_SPI_DMARD_OPCODE_OFFSET, opc(wmode, wop));
	note("txop-after", rd(THC_M_PRT_SPI_DMARD_OPCODE_OFFSET));
	note("spicfg-after", rd(THC_M_PRT_SPI_CFG_OFFSET));
	note("rdop-after", rd(THC_M_PRT_SPI_ICRRD_OPCODE_OFFSET));
	// 5. interrupt config (status only, global interrupt left OFF: we poll)
	wbits(THC_M_PRT_INT_STATUS_OFFSET, THC_M_PRT_INT_STATUS_TXN_ERR_INT_STS | THC_M_PRT_INT_STATUS_FATAL_ERR_INT_STS, 0xFFFFFFFF);
	wbits(THC_M_PRT_SW_SEQ_STS_OFFSET, THC_M_PRT_SW_SEQ_STS_THC_SS_ERR | THC_M_PRT_SW_SEQ_STS_TSSDONE, 0xFFFFFFFF);
	uint32_t r1 = rd(THC_M_PRT_READ_DMA_CNTRL_1_OFFSET) & ~THC_M_PRT_READ_DMA_CNTRL_IE_EOF;
	wr(THC_M_PRT_READ_DMA_CNTRL_1_OFFSET, r1);
	wr(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET, THC_M_PRT_READ_DMA_INT_STS_EOF_INT_STS | NONDMA_STS);
	wr(THC_M_PRT_READ_DMA_INT_STS_2_OFFSET, THC_M_PRT_READ_DMA_INT_STS_EOF_INT_STS);
	wbits(THC_M_PRT_WRITE_DMA_CNTRL_OFFSET, THC_M_PRT_WRITE_DMA_CNTRL_THC_WRDMA_IE_IOC_DMACPL, 0);
	wr(THC_M_PRT_WRITE_INT_STS_OFFSET, THC_M_PRT_WRITE_INT_STS_THC_WRDMA_ERROR_STS | THC_M_PRT_WRITE_INT_STS_THC_WRDMA_IOC_STS);
	wr(THC_M_PRT_READ_DMA_CNTRL_1_OFFSET, r1 | THC_M_PRT_READ_DMA_CNTRL_IE_NDDI);
	wbits(THC_M_PRT_TSEQ_CNTRL_1_OFFSET, THC_M_PRT_TSEQ_CNTRL_1_INT_EDG_DET_EN, THC_M_PRT_TSEQ_CNTRL_1_INT_EDG_DET_EN);
	wbits(THC_M_PRT_DEVINT_CFG_1_OFFSET,
	      THC_M_PRT_DEVINT_CFG_1_THC_M_PRT_INTTYP_OFFSET | THC_M_PRT_DEVINT_CFG_1_THC_M_PRT_INTTYP_LEN |
	      THC_M_PRT_DEVINT_CFG_1_THC_M_PRT_EOF_OFFSET | THC_M_PRT_DEVINT_CFG_1_THC_M_PRT_INTTYP_DATA_VAL,
	      THC_BIT_OFFSET_INTERRUPT_TYPE | (THC_BIT_LENGTH_INTERRUPT_TYPE << 5) | (THC_BIT_OFFSET_LAST_FRAGMENT_FLAG << 10) | (THC_BITMASK_INVALID_TYPE_DATA << 16));
	wbits(THC_M_PRT_DEVINT_CFG_2_OFFSET,
	      THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_UFSIZE_OFFSET | THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_UFSIZE_LEN |
	      THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_UFSIZE_UNIT | THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_FTYPE_IGNORE | THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_FTYPE_VAL,
	      THC_BIT_OFFSET_MICROFRAME_SIZE | (THC_BIT_LENGTH_MICROFRAME_SIZE << __builtin_ctz(THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_UFSIZE_LEN)) |
	      (THC_UNIT_MICROFRAME_SIZE << __builtin_ctz(THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_UFSIZE_UNIT)) |
	      THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_FTYPE_IGNORE | THC_M_PRT_DEVINT_CFG_2_THC_M_PRT_FTYPE_VAL);

	wbits(THC_M_PRT_DEVINT_CFG_2_OFFSET, BIT(24) | BIT(25) | BIT(27), 0);
	note("cfg1", rd(THC_M_PRT_DEVINT_CFG_1_OFFSET)); note("cfg2", rd(THC_M_PRT_DEVINT_CFG_2_OFFSET));

	// 6. reset the touch IC through ACPI, then unquiesce and wait for RESET_RESPONSE
	IOReturn rr = acpi->evaluateObject("_RST");
	note("acpi-rst", rr);
	wbits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN, 0);
	uint32_t cause = 0, buf[16] = {0}, got = 0;
	bool irq = waitNonDmaInt(5000, &cause);
	note("reset-irq", irq); note("reset-cause", cause);
	note("int-status", rd(THC_M_PRT_INT_STATUS_OFFSET));
	note("err-cause", rd(THC_M_PRT_ERR_CAUSE_OFFSET));
	if (!irq) { noteStr("result", "no RESET_RESPONSE interrupt"); goto done; }
	listening = true;   // the touch IC answered, so after this we keep listening for touch reports
	bodyAddr = bdy;
	uint32_t len = ((cause >> 8) & 0x3FFF) * 4;
	note("reset-len", len);
	int e = pioRead(bdy, len ? len : 4, buf, &got);
	note("reset-pio", (uint64_t)(int64_t)e); noteBytes("reset-body", buf, got > 64 ? 64 : got);

	// 7. DEVICE_DESCRIPTOR request: output header {type=1, len=0, id=0}
	struct Try { uint32_t mode, op, reg; };
	const Try tries[] = { {wmode, wop, opc(wmode, wop)} };
	irq = false;
	OSArray *att = OSArray::withCapacity(4);
	note("dmard-op", rd(THC_M_PRT_SPI_DMARD_OPCODE_OFFSET));
	for (unsigned t = 0; t < 1 && !irq; t++) {
		wbits(THC_M_PRT_SPI_CFG_OFFSET, THC_M_PRT_SPI_CFG_SPI_TWMODE, tries[t].mode << 18);
		wr(THC_M_PRT_SPI_DMARD_OPCODE_OFFSET, tries[t].reg);
		wr(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET, NONDMA_INT_STS_BIT);
		uint32_t req = 0x00000001;
		e = pioWrite(out, 4, &req);
		irq = (e == 0) && waitNonDmaInt(1500, &cause);
		char msg[96];
		snprintf(msg, sizeof(msg), "try%u mode%u op0x%x reg0x%x rb0x%x pio%d irq%d cause0x%x", t, tries[t].mode, tries[t].op,
		         tries[t].reg, rd(THC_M_PRT_SPI_DMARD_OPCODE_OFFSET), e, irq, cause);
		OSString *m = OSString::withCString(msg);
		if (att && m) att->setObject(m);
		OSSafeReleaseNULL(m);
		IOLog("SurfaceTHC: %s %s\n", acpi->getName(), msg);
	}
	if (att) { log->setObject("desc-attempts", att); att->release(); }
	note("desc-irq", irq); note("desc-cause", cause);
	if (!irq) { noteStr("result", "no DEVICE_DESCRIPTOR response"); goto done; }
	len = ((cause >> 8) & 0x3FFF) * 4;
	note("desc-len", len);
	bzero(buf, sizeof(buf)); got = 0;
	e = pioRead(bdy, len ? (len > 64 ? 64 : len) : 28, buf, &got);
	note("desc-pio", (uint64_t)(int64_t)e); noteBytes("desc-body", buf, got > 64 ? 64 : got);
	const uint8_t *b = (const uint8_t *)buf;
	if (!e && b[0] == 7 && got >= 28) {
		const uint8_t *d = b + 4;   // body header is 4 bytes
		auto le16 = [&](int o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
		note("report-desc-len", le16(4)); note("max-input-len", le16(6)); note("max-output-len", le16(8));
		note("vendor-id", le16(12)); note("product-id", le16(14)); note("version-id", le16(16));
		noteStr("result", "OK got device descriptor");
		// Step 2 for multitouch: ask for the HID report descriptor (HIDSPI output type 2) and try to
		// read it in 64-byte PIO chunks (address increment is enabled in DEVINT_CFG_2).
		uint32_t rdLen = le16(4);
		if (rdLen && rdLen <= 4096) {
			uint32_t rq = 0x00000002;
			int we = pioWrite(out, 4, &rq);
			uint32_t c2 = 0;
			bool ri = (we == 0) && waitNonDmaInt(2000, &c2);
			note("rdesc-write", (uint64_t)(int64_t)we); note("rdesc-irq", ri); note("rdesc-cause", c2);
			if (ri) {
				uint32_t total = ((c2 >> 8) & 0x3FFF) * 4;
				note("rdesc-total", total);
				uint8_t *rd = (uint8_t *)IOMalloc(4608);
				if (rd && total && total <= 4608) {
					bzero(rd, 4608);
					uint32_t off = 0; int pe = 0;
					while (off < total && !pe) {
						uint32_t chunk = total - off > 64 ? 64 : total - off, g = 0, cb[16] = {0};
						pe = pioRead(bdy + off, (chunk + 3) & ~3u, cb, &g);
						if (!pe) memcpy(rd + off, cb, chunk);
						off += chunk;
					}
					note("rdesc-pio", (uint64_t)(int64_t)pe);
					noteBytes("rdesc", rd, total);
				}
				if (rd) IOFree(rd, 4608);
			}
		}
	} else noteStr("result", "descriptor response looked wrong");
	}
done:
	note("ctrl-after", rd(THC_M_PRT_CONTROL_OFFSET));
	if (!listening)   // leave the controller quiet again
		wbits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN);
	setProperty("THC-bringup", log);
	if (OSString *r = OSDynamicCast(OSString, log->getObject("result")))
		IOLog("SurfaceTHC: %s bring-up: %s\n", acpi->getName(), r->getCStringNoCopy());
	OSSafeReleaseNULL(log);
}

// ---------- listen mode: poll for touch reports for 10 minutes and record what arrives ----------
void SurfaceTHCProbe::listenEntry(void *arg, wait_result_t) {
	SurfaceTHCProbe *self = static_cast<SurfaceTHCProbe *>(arg);
	self->listen();
	self->release();
	thread_terminate(current_thread());
}

void SurfaceTHCProbe::listen() {
	uint64_t t0 = mach_absolute_time(), freq_ns;
	absolutetime_to_nanoseconds(1, &freq_ns);
	uint32_t count = 0, readErr = 0;
	OSArray *first = OSArray::withCapacity(16);
	OSDictionary *types = OSDictionary::withCapacity(8);
	char key[16];
	for (uint32_t loops = 0; !stopping && (mouse || loops < 45000); loops++) {   // 90 s unless it turned out to be the touchpad
		uint32_t cause;
		if (!(rd(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET) & NONDMA_STS)) { IOSleep(2); continue; }
		cause = rd(THC_M_PRT_DEV_INT_CAUSE_REG_VAL_OFFSET);
		wr(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET, NONDMA_INT_STS_BIT);
		uint32_t len = ((cause >> 8) & 0x3FFF) * 4, got = 0, buf[16] = {0};
		int e = pioRead(bodyAddr, len == 0 ? 4 : (len > 64 ? 64 : len), buf, &got);
		if (e) readErr++;
		count++;
		const uint8_t *b = (const uint8_t *)buf;
		// HIDSPI body: [type][len lo][len hi][report id][content...]; touchpad mouse report = id 0x80, 3 bytes
		if (!e && b[0] == 1 && b[3] == 0x80 && got >= 7) {
			if (!mouse) {
				SurfaceTHCMouse *m = OSTypeAlloc(SurfaceTHCMouse);
				if (m && m->init() && m->attach(this)) {
					if (m->start(this)) { mouse = m; IOLog("SurfaceTHC: %s touchpad mouse online\n", acpiName); }
					else { m->detach(this); m->release(); }
				} else OSSafeReleaseNULL(m);
			}
			if (mouse) static_cast<SurfaceTHCMouse *>(mouse)->send(b + 4);
		}
		snprintf(key, sizeof(key), "type%u", b[0]);
		OSNumber *old = OSDynamicCast(OSNumber, types->getObject(key));
		OSNumber *nn = OSNumber::withNumber((old ? old->unsigned32BitValue() : 0) + 1, 32);
		if (nn) { types->setObject(key, nn); nn->release(); }
		if (first && first->getCount() < 16) {
			char line[200];
			uint64_t ms = (mach_absolute_time() - t0) / 1000000;
			int n = snprintf(line, sizeof(line), "t+%llums cause0x%08x len%u pio%d:", ms, cause, len, e);
			for (uint32_t i = 0; i < 40 && i < got && n < (int)sizeof(line) - 4; i++) n += snprintf(line + n, sizeof(line) - n, " %02x", b[i]);
			OSString *str = OSString::withCString(line);
			if (str) { first->setObject(str); str->release(); }
			if (first->getCount() == 1) IOLog("SurfaceTHC: %s first report: %s\n", acpiName, line);
		}
		if (count == 1 || (count % 500) == 0) {
			OSDictionary *d = OSDictionary::withCapacity(4);
			if (d) {
				setNum2(d, "reports", count); setNum2(d, "read-errors", readErr);
				// Registry collections are frozen once published, so publish copies.
				OSArray *fc = first ? OSArray::withArray(first) : nullptr;
				OSDictionary *tc = OSDictionary::withDictionary(types);
				if (fc) { d->setObject("first", fc); fc->release(); }
				if (tc) { d->setObject("by-type", tc); tc->release(); }
				setProperty("THC-listen", d); d->release();
			}
		}
	}
	OSDictionary *d = OSDictionary::withCapacity(4);
	if (d) {
		setNum2(d, "reports", count); setNum2(d, "read-errors", readErr);
		OSArray *fc = first ? OSArray::withArray(first) : nullptr;
		OSDictionary *tc = types ? OSDictionary::withDictionary(types) : nullptr;
		if (fc) { d->setObject("first", fc); fc->release(); }
		if (tc) { d->setObject("by-type", tc); tc->release(); }
		OSString *st = OSString::withCString(stopping ? "stopped" : "finished 90 s window");
		if (st) { d->setObject("state", st); st->release(); }
		setProperty("THC-listen", d); d->release();
	}
	OSSafeReleaseNULL(first); OSSafeReleaseNULL(types);
	// Put everything back: quiesce, wait for the THC to go idle, and reset the touch IC again so
	// it is in the same state the firmware left it (round 0.3 left it running and hung a reboot).
	wbits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_EN);
	pollBits(THC_M_PRT_CONTROL_OFFSET, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_HW_STS, THC_M_PRT_CONTROL_THC_DEVINT_QUIESCE_HW_STS, 1000);
	if (acpiDev) acpiDev->evaluateObject("_RST");
	wr(THC_M_PRT_READ_DMA_INT_STS_1_OFFSET, NONDMA_INT_STS_BIT);
	listening = false;
	IOLog("SurfaceTHC: %s listen done, %u reports\n", acpiName, count);
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
	if (!checkThcTest()) setProperty("THC-bringup", "skipped (no -thctest boot-arg)");

	// Only touch the controller when the -thctest boot-arg is set, so a normal boot is read-only.
	if (map && acpi && map->getLength() >= 0x1400 && checkThcTest()) {
		pci->setBusMasterEnable(false);
		strlcpy(acpiName, acpi->getName(), sizeof(acpiName));
		acpiDev = acpi;
		bringUp(acpi);
		if (listening) {
			OSDictionary *d = OSDictionary::withCapacity(1);
			if (d) { OSString *st = OSString::withCString("waiting for touches"); if (st) { d->setObject("state", st); st->release(); } setProperty("THC-listen", d); d->release(); }
			thread_t th;
			retain();
			if (kernel_thread_start(&SurfaceTHCProbe::listenEntry, this, &th) == KERN_SUCCESS) thread_deallocate(th);
			else { release(); listening = false; }
		}
	}
	IOLog("SurfaceTHC: probe done on %s\n", provider->getName());
	registerService();
	return true;
}

void SurfaceTHCProbe::stop(IOService *provider) {
	stopping = true;
	for (int i = 0; i < 100 && listening; i++) IOSleep(10);
	if (mouse) { mouse->stop(this); mouse->detach(this); OSSafeReleaseNULL(mouse); }
	OSSafeReleaseNULL(map);
	IOService::stop(provider);
}
