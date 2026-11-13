/*
 * SSDT-SSH: Surface Serial Hub (SAM over UART) for macOS on the Surface Laptop Studio 2.
 * The firmware defines \_SB.SSH in SSDT "SsdtSurf", but macOS never ends up with it in the
 * ACPI plane, so BigSurface (keyboard + battery) has nothing to attach to. This is a copy of
 * the device, Darwin only. If the original table ever loads, this one just fails to load.
 */
DefinitionBlock ("", "SSDT", 2, "SLS2", "SSH", 0x00000000)
{
    External (_SB_.GPI0, DeviceObj)
    External (_SB_.PC00.UA00, DeviceObj)

    Scope (\_SB)
    {
        Device (SSH)
        {
            Name (_HID, "MSHW0084")
            Name (_S0W, 0x03)
            Name (_DEP, Package (0x01) { \_SB.PC00.UA00 })
            Method (_STA, 0, NotSerialized)
            {
                If (_OSI ("Darwin"))
                {
                    Return (0x0F)
                }
                Return (Zero)
            }

            Name (SBUF, ResourceTemplate ()
            {
                UartSerialBusV2 (0x003D0900, DataBitsEight, StopBitsOne,
                    0xC0, LittleEndian, ParityTypeNone, FlowControlHardware,
                    0x0040, 0x0040, "\\_SB.PC00.UA00",
                    0x00, ResourceConsumer, , Exclusive,
                    )
                GpioInt (Edge, ActiveHigh, SharedAndWake, PullNone, 0x0000,
                    "\\_SB.GPI0", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x00AE
                    }
                GpioIo (Shared, PullNone, 0x0000, 0x0000, IoRestrictionInputOnly,
                    "\\_SB.GPI0", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x00AE
                    }
            })
            Method (_CRS, 0, NotSerialized)
            {
                Return (SBUF)
            }

            Method (_DSM, 4, NotSerialized)
            {
                If ((Arg0 == ToUUID ("d5e383e1-d892-4a76-89fc-f6aaae7ed5b5")))
                {
                    If ((Arg1 == Zero))
                    {
                        If ((Arg2 == Zero)) { Return (Buffer (0x02) { 0xE1, 0x07 }) }
                        If ((Arg2 == 0x05)) { Return (Zero) }
                        If ((Arg2 == 0x06)) { Return (0xC8) }
                        If ((Arg2 == 0x07)) { Return (0x96) }
                        If ((Arg2 == 0x08)) { Return (Zero) }
                        If ((Arg2 == 0x09)) { Return (0x30) }
                        If ((Arg2 == 0x0A)) { Return (0x02) }
                    }
                }
                Return (Buffer (One) { 0x00 })
            }
        }
    }
}
