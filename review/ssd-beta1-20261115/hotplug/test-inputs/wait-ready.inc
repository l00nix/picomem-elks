__force_inline void pm_payload_wait_ready(void)
{
    unsigned polls = 4096;
    while (!pio_interrupt_get(isa_pio, isa_bus_DATA_READY_IRQ))
        if (!--polls) pm_payload_ready_fault();
}