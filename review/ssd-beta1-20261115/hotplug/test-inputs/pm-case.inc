        case DEV_PM :
#if PM_USB_SSD
           PM_PayloadRead = dev_pmio_ior(ISA_IO_Addr,&ISA_Data) &&
               (ISA_IO_Addr & 7) == PORT_TR_L;
           if (PM_PayloadRead) {
               // Flag4 belongs to SM0; neither CPU IRQ line may consume it.
               if ((isa_pio->inte0 | isa_pio->inte1) & (1u << pis_interrupt4))
                   pm_payload_ready_fault();
               // Clear before the read opcode, not after the byte is queued.
               pio_interrupt_clear(isa_pio, isa_bus_DATA_READY_IRQ);
           }
#else
           dev_pmio_ior(ISA_IO_Addr,&ISA_Data);
#endif
           pm_do_ior();
          break; //DEV_PM    
