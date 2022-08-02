/*
   Stop watch with interval timer interrupts and KEY interrupts
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
MODULE_DESCRIPTION("Stopwatch");

void display_hex(volatile int*, volatile int*);


void * LW_virtual;         // Lightweight bridge base address
volatile int *timer_ptr;
volatile int *KEY_ptr;
volatile int *SW_ptr;
volatile int *LED_ptr;
volatile int *HEX3_HEX0_ptr;
volatile int *HEX5_HEX4_ptr;

int minutes, seconds, hundreths;

char hex_codes[] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{   
   if (hundreths == 0){
      hundreths = 99;
      if (seconds == 0){
         seconds = 59;
         if (minutes == 0){
            seconds = 0;
            hundreths = 0;
         }
         else
            --minutes;
      }
      else  
         --seconds;
   }
   else
      --hundreths;

   display_hex(HEX3_HEX0_ptr, HEX5_HEX4_ptr);

   // reset interrupt flag
   *timer_ptr = 0;
   
 
   return (irq_handler_t) IRQ_HANDLED;
}

irq_handler_t key_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
   if ((*(KEY_ptr + 3) & 0b1111) == 0b0001){
      if ((*timer_ptr & 0b10) == 0b10)
         *(timer_ptr + 1) = 0b1011;
      else 
      *(timer_ptr + 1) = 0b0111;
   }
   else if ((*(KEY_ptr + 3) & 0b1111) == 0b0010){
      if (*SW_ptr >= 99)
         hundreths = 99;
      else  
         hundreths = *SW_ptr;
   }
   else if ((*(KEY_ptr + 3) & 0b1111) == 0b0100){
      if (*SW_ptr >= 59)
         seconds = 59;
      else  
         seconds = *SW_ptr;
   }
   else if ((*(KEY_ptr + 3) & 0b1111) == 0b1000){
      if (*SW_ptr >= 59)
         minutes = 59;
      else  
         minutes = *SW_ptr;
   }
   
   *LED_ptr += 1;

   display_hex(HEX3_HEX0_ptr, HEX5_HEX4_ptr);

   *(KEY_ptr + 3) = 0xF; 
   
   return (irq_handler_t) IRQ_HANDLED;
}


static int __init initialize_stopwatch_handler(void)
{
   int value;
   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
   
   KEY_ptr = LW_virtual + KEY_BASE;    // init virtual address for KEY port
   // Clear the PIO edgecapture register (clear any pending interrupt)
   *(KEY_ptr + 3) = 0xF; 
   // Enable IRQ generation for the 4 buttons
   *(KEY_ptr + 2) = 0xF; 

   SW_ptr = LW_virtual + SW_BASE;

   LED_ptr = LW_virtual + LEDR_BASE;
   *LED_ptr = 0x200;

   timer_ptr = LW_virtual + TIMER0_BASE;
   *(timer_ptr + 2) = 0x4240; // lower 16 bits of (1,000,000)2 = (F4240)16
   *(timer_ptr + 3) = 0xF; // upper 16 bits
   *(timer_ptr + 1) = 0b111; // [start][cont][ito]

   HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;

   HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;

   // turn into function after
   display_hex(HEX3_HEX0_ptr, HEX5_HEX4_ptr);

   minutes = 59;
   seconds = 59;
   hundreths = 99;

   // Register the interrupt handler.
   value = request_irq (INTERVAL_TIMER_IRQ, (irq_handler_t) timer_irq_handler, IRQF_SHARED, 
      "timer_irq_handler", (void *) (timer_irq_handler));
      
   value = request_irq (KEYS_IRQ, (irq_handler_t) key_irq_handler, IRQF_SHARED, 
      "key_irq_handler", (void *) (key_irq_handler));
   return value;
}

static void __exit cleanup_stopwatch_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   *(timer_ptr + 1) |= 0b1000; // stop the timer
   *LED_ptr = 0;
   free_irq (INTERVAL_TIMER_IRQ, (void*) timer_irq_handler);
   free_irq (KEYS_IRQ, (void*) key_irq_handler);
}

// *l points to HEX3-0, *u points to HEX5-4
void display_hex(volatile int *l, volatile int *u)
{
   *l = hex_codes[hundreths % 10];
   *l |= hex_codes[hundreths / 10] << 8;
   *l |= hex_codes[seconds % 10] << 16;
   *l |= hex_codes[seconds /10] << 24;

   *u = hex_codes[minutes % 10];
   *u |= hex_codes[minutes / 10] << 8;
}

module_init(initialize_stopwatch_handler);
module_exit(cleanup_stopwatch_handler);