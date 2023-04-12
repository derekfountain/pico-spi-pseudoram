/*

MIT Licence:

Copyright 2023 Derek Fountain

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

*/

/*
 * This is an example of, and timing test of, using a psuedo-RAM
 * device as external SPI storage on a Raspberry Pi Pico.
 *
 * The specific chip I'm using is:
 *
 * https://www.mouser.co.uk/ProductDetail/AP-Memory/APS6404L-3SQR-SN?qs=IS%252B4QmGtzzqsn3S5xo%2FEEg%3D%3D
 *
 * I happen to have it wired up on one of Andrew Menadue's little
 * boards:
 * 
 * https://github.com/blackjetrock/picoram
 * https://www.youtube.com/watch?v=YS-xCZsu00U
 *
 * but you don't need that if you have some other way of
 * connecting it to the Pico.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

/*
 * A non-overclocked Pico can drive SPI at 62.5MHz maximum.
 */
#if 0
#define OVERCLOCK 170000
#endif

/* Set this to get things working with the simple test */
#define RUN_READ_ID_TEST       0

/* Test pin, hooked up to 'scope */
const uint8_t TEST_OUTPUT_GP = 28;

/* I'm using SPI1 for this test because it suits the project I have in mind */
#define PICO_SPI_RX_PIN        12
#define PICO_SPI_TX_PIN        15
#define PICO_SPI_SCK_PIN       14
#define PICO_SPI_CSN_PIN       13

/* Commands for the memory chip I'm using */
#define PRAM_CMD_WRITE         0x02
#define PRAM_CMD_READ          0x03
#define PRAM_CMD_FAST_READ     0x0B
#define PRAM_CMD_RESET_ENABLE  0x66
#define PRAM_CMD_RESET         0x99
#define PRAM_CMD_READ_ID       0x9F




static inline void cs_select(uint cs_pin)
{
  gpio_put(cs_pin, 0);
}

static inline void cs_deselect(uint cs_pin)
{
  gpio_put(cs_pin, 1);
}

static inline void blip_test_pin( void )
{
  gpio_put( TEST_OUTPUT_GP, 1 );
  __asm volatile ("nop");  __asm volatile ("nop");
  __asm volatile ("nop");  __asm volatile ("nop");
  __asm volatile ("nop");  __asm volatile ("nop");
  __asm volatile ("nop");  __asm volatile ("nop");
  __asm volatile ("nop");  __asm volatile ("nop");
  __asm volatile ("nop");  __asm volatile ("nop");
  gpio_put( TEST_OUTPUT_GP, 0 );
}

int main()
{
  bi_decl(bi_program_description("Pico SPI RAM test"));

#ifdef OVERCLOCK
  set_sys_clock_khz( OVERCLOCK, 1 );
#endif

  stdio_init_all();
  busy_wait_us_32(2000000);

  printf("SPI test running...\n");

  /* Test pin, shows on 'scope */
  gpio_init(TEST_OUTPUT_GP); gpio_set_dir(TEST_OUTPUT_GP, GPIO_OUT); gpio_put(TEST_OUTPUT_GP, 0);

  /* AM wired these up for quad mode on his board, leave off for now */
  gpio_init(16); gpio_set_dir( 16, GPIO_OUT ); gpio_pull_up( 16 ); gpio_put( 16, 1 );
  gpio_init(17); gpio_set_dir( 17, GPIO_OUT ); gpio_pull_up( 17 ); gpio_put( 17, 1 );

  /*
   * Enable SPI 1 at <n> MHz and connect to GPIOs. Second param is a baudrate, giving it
   * a frequency like this seems rather silly. You get what the hardware can give you.
   * Might as well ask for the theoretical maximum though.
   */
  spi_init(spi1, 62 * 1000 * 1000);
  gpio_set_function(PICO_SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_SPI_TX_PIN, GPIO_FUNC_SPI);

  /* Output for chip select on slave, starts high (unselected) */
  gpio_init(PICO_SPI_CSN_PIN);
  gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_OUT);
  gpio_put(PICO_SPI_CSN_PIN, 1);

  /* Datasheet says 150uS between power up and the reset command */
  busy_wait_us_32(200);

  /* All examples I've seen don't bother with this reset, might be optional */
  uint8_t reset_cmd[] = { PRAM_CMD_RESET_ENABLE, 
			  PRAM_CMD_RESET };
  gpio_put(PICO_SPI_CSN_PIN, 0); 
  spi_write_blocking(spi1, reset_cmd, 2);
  gpio_put(PICO_SPI_CSN_PIN, 1);   


#if !RUN_READ_ID_TEST
/*
 * Create a big buffer, I want to see how long it takes to
 * copy it to and from the RAM device
 */
#define BUF_LEN 1024*100
  
  uint8_t out_buf[BUF_LEN];
  uint8_t in_buf[BUF_LEN];

  for(size_t i = 0; i < BUF_LEN; i++)
  {
    out_buf[i] = i & 0xFF;
    in_buf[i] = 0;
  }
#endif

  while( 1 )
  {
#if RUN_READ_ID_TEST
    cs_select(PICO_SPI_CSN_PIN);

    /* Read ID, on the chip I'm using takes 0x9F as the command followed by 3 "don't care"s */
    uint8_t read_cmd[] = { PRAM_CMD_READ_ID,
			   0, 0, 0 };
    int wr = spi_write_blocking(spi1, read_cmd, sizeof(read_cmd)); 
    printf("Write returned: 0x%04X\n", wr);
			
    /* Chip I'm using returns 0x0D, 0x5D according to the datasheet */
    uint8_t result=0;
    int rr1 = spi_read_blocking(spi1, 0, &result, 1 ); 
    printf("Read result is 0x%04X, returned: 0x%02X\n", rr1, result);

    int rr2 = spi_read_blocking(spi1, 0, &result, 1 ); 
    printf("Read result is 0x%04X, returned: 0x%02X\n", rr2, result);

    cs_deselect(PICO_SPI_CSN_PIN);
#else

    /* This test writes 0xAA, 0xBB, 0xCC, 0xDD into the RAM device... */
    cs_select(PICO_SPI_CSN_PIN);

    /* Write 4 data bytes into RAM at 0x000000 */
    uint8_t write_cmd[] = { PRAM_CMD_WRITE,
			    0, 0, 0,
			    0xAA, 0xBB, 0xCC, 0xDD };
    int wr = spi_write_blocking(spi1, write_cmd, sizeof(write_cmd));
    printf("Write returned: 0x%04X\n", wr);

    cs_deselect(PICO_SPI_CSN_PIN);


    /* ...and then reads it back again */
    cs_select(PICO_SPI_CSN_PIN);

    /* Read back 4 data bytes from 0x000000. Send the read command: */
    uint8_t read_cmd[] = { PRAM_CMD_FAST_READ,
			   0, 0, 0 };
    wr = spi_write_blocking(spi1, read_cmd, sizeof(read_cmd));
    printf("Write returned: 0x%04X\n", wr);

    /*
     * Now do the read. This is a "fast frequency read." According to the datasheet
     * there's a period of what looks like 9 "wait" cycles before the data is ready.
     * How do I deal with that? Well it turns out that reading a single byte of
     * "don't care" data keeps things in sync. I can't claim to understand exactly
     * what's happening here.
     */
    uint32_t result=0;
    int rr = spi_read_blocking(spi1, 0, (uint8_t*)&result, 1 );   /* Wait state byte */
        rr = spi_read_blocking(spi1, 0, (uint8_t*)&result, 4 ); 
    printf("Read result is 0x%04X, returned: 0x%08X\n", rr, result);

    cs_deselect(PICO_SPI_CSN_PIN);




    /*
     * This tests my particular use case which is to dump 100KB out
     * to the RAM device, then read it back in again. I want to know
     * how quickly it can do it
     */
    cs_select(PICO_SPI_CSN_PIN);

#if 1
    blip_test_pin();
#endif

    /* Write data buffer into RAM at 0x000000 */
    uint8_t write_data_cmd[] = { PRAM_CMD_WRITE,
				 0, 0, 0 };
        wr = spi_write_blocking(spi1, write_data_cmd, sizeof(write_data_cmd));
    int wd = spi_write_blocking(spi1, out_buf,        sizeof(out_buf));

#if 1
    blip_test_pin();
#endif

    printf("Write returned:      0x%04X\n", wr);
    printf("Write data returned: 0x%04X\n", wd);

    cs_deselect(PICO_SPI_CSN_PIN);

    busy_wait_us_32(20);

    cs_select(PICO_SPI_CSN_PIN);

#if 1
    blip_test_pin();
#endif

    spi_write_blocking(spi1, read_cmd, sizeof(read_cmd));
    int rd = spi_read_blocking(spi1, 0, in_buf, 1 );            /* Wait state byte */
        rd = spi_read_blocking(spi1, 0, in_buf, sizeof(in_buf) ); 

#if 1
    blip_test_pin();
#endif

    printf("Read data result is 0x%04X\n", rd);
    cs_deselect(PICO_SPI_CSN_PIN);

#if 1
    uint8_t data_ok = 1;
    for(size_t i = 0; i < BUF_LEN; i++)
    {
      if( out_buf[i] != in_buf[i] )
      {
	printf("Data mismatch at byte %i, 0x%02X != 0x%02X\n", i, out_buf[i], in_buf[i]);
	data_ok = 0;
      }
    }
    if( data_ok )
      printf("Data read matches data written :)\n");
#endif

#endif
    busy_wait_us_32(2000000);

  }
}
