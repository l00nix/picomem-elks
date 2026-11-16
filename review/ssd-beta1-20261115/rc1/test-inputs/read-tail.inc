if (ISA_WasRead) {
#if PM_USB_SSD
    // Release READY only after PIO has executed the data output instruction.
    if (PM_PayloadRead) {
        pio_sm_put(isa_pio, isa_bus_sm, (uint32_t) ISA_Data);
        pm_payload_wait_ready();
        gpioc_hi_out_clr(1<<PIN_IORDY-32);
    } else
#endif
    {
        gpioc_hi_out_clr(1<<PIN_IORDY-32);
        pio_sm_put(isa_pio, isa_bus_sm, (uint32_t) ISA_Data);
    }
    ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm);      // Wait that the data is sent to wait for the next ALE  
   }
   else {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   }
