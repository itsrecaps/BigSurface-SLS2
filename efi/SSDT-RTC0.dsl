/*
 * Fake legacy RTC for macOS on the Surface Laptop Studio 2.
 * The SLS2 firmware has no PNP0B00 device at all (its clock is the Surface SAM "SRTC" ACPI000E),
 * and macOS needs one to boot.
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "RTC0", 0x00000000)
{
    External (_SB_.PC00.LPCB, DeviceObj)

    Scope (_SB.PC00.LPCB)
    {
        Device (RTC0)
        {
            Name (_HID, EisaId ("PNP0B00"))
            Name (_CRS, ResourceTemplate ()
            {
                IO (Decode16, 0x0070, 0x0070, 0x01, 0x08)
                IRQNoFlags () {8}
            })
            Method (_STA, 0, NotSerialized)
            {
                If (_OSI ("Darwin"))
                {
                    Return (0x0F)
                }
                Return (Zero)
            }
        }
    }
}
