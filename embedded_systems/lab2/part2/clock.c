/*
   Real time clock that makes use of the interval timer with interrupts enabled
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../address_map_arm.h"
#include "../interrupt_ID.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Damian Pacynko");
MODULE_DESCRIPTION("Real time clock");

void * LW_virtual;         // Lightweight bridge base address
volatile int *timer_ptr;
volatile int *HEX3_HEX0_ptr;
volatile int *HEX5_HEX4_ptr;

int minutes, seconds, hundreths;

char hex_codes[] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{   
   if (hundreths == 99){
      if (seconds == 59){
         if (minutes == 59){
            minutes = 0;
         }
         else
            ++minutes;
         seconds = 0;
      }
      else  
         ++seconds;
      hundreths = 0;
   }
   else
      ++hundreths;

   *HEX3_HEX0_ptr = hex_codes[hundreths % 10];
   *HEX3_HEX0_ptr |= hex_codes[hundreths / 10] << 8;
   *HEX3_HEX0_ptr |= hex_codes[seconds % 10] << 16;
   *HEX3_HEX0_ptr |= hex_codes[seconds /10] << 24;

   *HEX5_HEX4_ptr = hex_codes[minutes % 10];
   *HEX5_HEX4_ptr |= hex_codes[minutes / 10] << 8;

   // reset interrupt flag
   *timer_ptr = 0;
   
 
   return (irq_handler_t) IRQ_HANDLED;
}

static int __init initialize_timer_handler(void)
{
   int value;
   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
   
   timer_ptr = LW_virtual + TIMER0_BASE;
   *(timer_ptr + 2) = 0x4240; // lower 16 bits of (1,000,000)2 = (F4240)16
   *(timer_ptr + 3) = 0xF; // upper 16 bits
   *(timer_ptr + 1) = 0b111; // [start][cont][ito]

   HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;
   *HEX3_HEX0_ptr = 0;

   HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;
   *HEX5_HEX4_ptr = 0;

   minutes = 0;
   seconds = 0;
   hundreths = 0;

   // Register the interrupt handler.
   value = request_irq (INTERVAL_TIMER_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED, 
      "clock", (void *) (irq_handler));
   return value;
}

static void __exit cleanup_timer_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   *(timer_ptr + 1) |= 0b1000; // stop the timer
   free_irq (INTERVAL_TIMER_IRQ, (void*) irq_handler);
}

module_init(initialize_timer_handler);
module_exit(cleanup_timer_handler);

