#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "../address_map_arm.h"

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);

char display_char(char);

/* This program increments the contents of the red LED parallel port */
int main(void)
{
   volatile int * LEDR_ptr;   // virtual address pointer to red LEDs
   volatile int * HEX3_HEX0_ptr; // virtual address pointer to hex disaplays 3-0
   volatile int * HEX5_HEX4_ptr; // virtual address pointer to hex disaplays 5-4
   int scroll, count;
   
   char message[] = "Intel SoC FPGA      "; 

   char * pmessage;
   pmessage = message;

   int fd = -1;               // used to open /dev/mem for access to physical addresses
   void *LW_virtual;          // used to map physical addresses for the light-weight bridge
   


   // Create virtual memory access to the FPGA light-weight bridge
   if ((fd = open_physical (fd)) == -1)
      return (-1);
   if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
      return (-1);

   // Set virtual address pointer to I/O port
   LEDR_ptr = (int *) (LW_virtual + LEDR_BASE);
   HEX3_HEX0_ptr = (int *) (LW_virtual + HEX3_HEX0_BASE);
   HEX5_HEX4_ptr = (int *) (LW_virtual + HEX5_HEX4_BASE);

   scroll = 1;
   count = 0;

   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   *LEDR_ptr = 0;


   while (count < 1)
   {
      ++(*LEDR_ptr);

      *HEX5_HEX4_ptr = display_char(*pmessage) << 8; // hex5
      *HEX5_HEX4_ptr |= display_char(*(pmessage + 1)); // hex4
      *HEX3_HEX0_ptr = display_char(*(pmessage + 2)) << 24; // hex3 
      *HEX3_HEX0_ptr |= display_char(*(pmessage + 3)) << 16; // hex2 
      *HEX3_HEX0_ptr |= display_char(*(pmessage + 4)) << 8; // hex1
      *HEX3_HEX0_ptr |= display_char(*(pmessage + 5)); // hex0

      if (pmessage == (message + 14)){
         pmessage = message;
         count++;
      }
      else 
         if (scroll) ++pmessage;

      sleep(1);
   }

   printf("Has run enough times\n");

   unmap_physical (LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
   close_physical (fd);   // close /dev/mem
   return 0;
}

// Open /dev/mem, if not already done, to give access to physical addresses
int open_physical (int fd)
{
   if (fd == -1)
      if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1){
         printf ("ERROR: could not open \"/dev/mem\"...\n");
         return (-1);
      }
   return fd;
}

// Close /dev/mem to give access to physical addresses
void close_physical (int fd)
{
   close (fd);
}

/*
 * Establish a virtual address mapping for the physical addresses starting at base, and
 * extending by span bytes.
 */
void* map_physical(int fd, unsigned int base, unsigned int span)
{
   void *virtual_base;

   // Get a mapping from physical addresses to virtual addresses
   virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, base);
   if (virtual_base == MAP_FAILED)
   {
      printf ("ERROR: mmap() failed...\n");
      close (fd);
      return (NULL);
   }
   return virtual_base;
}

/*
 * Close the previously-opened virtual address mapping
 */
int unmap_physical(void * virtual_base, unsigned int span)
{
   if (munmap (virtual_base, span) != 0)
   {
      printf ("ERROR: munmap() failed...\n");
      return (-1);
   }
   return 0;
}

char display_char(char c)
{
   char seg7_code;
   switch (c)
   {
      case 'I': seg7_code = 0b0010000; break;
      case 'n': seg7_code = 0b1010100; break;
      case 't': seg7_code = 0b1111000; break;
      case 'e': seg7_code = 0b1111001; break;
      case 'l': seg7_code = 0b0111000; break;
      case 'S': seg7_code = 0b1101101; break;
      case 'o': seg7_code = 0b1011100; break;
      case 'C': seg7_code = 0b0111001; break;
      case 'F': seg7_code = 0b1110001; break;
      case 'P': seg7_code = 0b1110011; break;
      case 'G': seg7_code = 0b1111101; break;
      case 'A': seg7_code = 0b1110111; break;
      case ' ': seg7_code = 0b0000000; break;
      default: seg7_code = 0;
   }
   return seg7_code;
}
