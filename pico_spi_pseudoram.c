#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

const uint8_t LED_PIN        = PICO_DEFAULT_LED_PIN;
const uint8_t TEST_OUTPUT_GP = 28;

/* I'm using SPI1 for this test because it suits the project I have in mind */
#define PICO_SPI_RX_PIN  12
#define PICO_SPI_TX_PIN  15
#define PICO_SPI_SCK_PIN 14
#define PICO_SPI_CSN_PIN 13

#define FLASH_CMD_STATUS       0x05
#define FLASH_CMD_WRITE_EN     0x06
#define FLASH_CMD_READ         0x03
#define RAM_CMD_WRITE          0x02

#define FLASH_STATUS_BUSY_MASK 0x01

#define PRAM_CMD_WRITE         0x02
#define PRAM_CMD_READ          0x03
#define PRAM_CMD_RESET_ENABLE  0x66
#define PRAM_CMD_RESET         0x99
#define PRAM_CMD_READ_ID       0x9F

#define RUN_READ_ID_TEST       0

static inline void cs_select(uint cs_pin) {
  gpio_put(cs_pin, 0);
}

static inline void cs_deselect(uint cs_pin) {
  gpio_put(cs_pin, 1);
}

#if 0
void __not_in_flash_func(flash_write_enable)(spi_inst_t *spi, uint cs_pin) {
    cs_select(cs_pin);
    uint8_t cmd = FLASH_CMD_WRITE_EN;
    spi_write_blocking(spi, &cmd, 1);
    cs_deselect(cs_pin);
}

void __not_in_flash_func(flash_wait_done)(spi_inst_t *spi, uint cs_pin) {
    uint8_t status;
    do {
        cs_select(cs_pin);
        uint8_t buf[2] = {FLASH_CMD_STATUS, 0};
        spi_write_read_blocking(spi, buf, buf, 2);
        cs_deselect(cs_pin);
        status = buf[1];
    } while (status & FLASH_STATUS_BUSY_MASK);
}

void ram_read(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *buf, size_t len) {
    cs_select(cs_pin);

    uint8_t cmdbuf[4] = {
            FLASH_CMD_READ,
            addr >> 16,
            addr >> 8,
            addr
    };
    spi_write_blocking(spi, cmdbuf, 4);
    spi_read_blocking(spi, 0, buf, len);
    cs_deselect(cs_pin);

    //printf("\nram_read spi=%08X, addr=%08X, data = %02X, len=%d\n", spi, addr, *buf, len);
}

void ram_write(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t data[], size_t len)
{

  uint8_t cmdbuf[4] =
    {
     RAM_CMD_WRITE,
     addr >> 16,
     addr >> 8,
     addr
    };

//  flash_write_enable(spi, cs_pin);
  cs_select(cs_pin);
  spi_write_blocking(spi, cmdbuf, 4);
  spi_write_blocking(spi, data, len);
  cs_deselect(cs_pin);
  flash_wait_done(spi, cs_pin);
  
  //printf("\nram_write addr=%08X, data= %02X, len=%d\n", addr, data[0], len);
}
#endif

int main()
{
  bi_decl(bi_program_description("Pico SPI RAM test"));

  stdio_init_all();
  busy_wait_us_32(2000000);

  printf("SPI test running...\n");

  /* Just to show we're running */
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 1);

  /* AM wired these up for quad mode, leave off for now */
  gpio_init(16); gpio_set_dir( 16, GPIO_OUT ); gpio_pull_up( 16 ); gpio_put( 16, 1 );
  gpio_init(17); gpio_set_dir( 17, GPIO_OUT ); gpio_pull_up( 17 ); gpio_put( 17, 1 );

  /* Enable SPI 1 at 33 MHz and connect to GPIOs */
  spi_init(spi1, 33 * 1000 * 1000);
  gpio_set_function(PICO_SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_SPI_TX_PIN, GPIO_FUNC_SPI);

  /* Output for chip select on slave, starts high (unselected) */
  gpio_init(PICO_SPI_CSN_PIN);
  gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_OUT);
  gpio_put(PICO_SPI_CSN_PIN, 1);

  /* Datasheet says 150uS between power up and the reset command */
  busy_wait_us_32(200);

  /* Most examples I've seen don't bother with this reset, might be optional */
  uint8_t reset_cmd[] = { PRAM_CMD_RESET_ENABLE, 
			  PRAM_CMD_RESET };
  gpio_put(PICO_SPI_CSN_PIN, 0); 
  spi_write_blocking(spi1, reset_cmd, 2);
  gpio_put(PICO_SPI_CSN_PIN, 1);   


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
    cs_select(PICO_SPI_CSN_PIN);

    /* Write 4 data bytes into RAM at 0x000000 */
    uint8_t write_cmd[] = { PRAM_CMD_WRITE,
			    0, 0, 0,
			    0xAA, 0xBB, 0xCC, 0xDD };
    int wr = spi_write_blocking(spi1, write_cmd, sizeof(write_cmd));
    printf("Write returned: 0x%04X\n", wr);

    cs_deselect(PICO_SPI_CSN_PIN);


    cs_select(PICO_SPI_CSN_PIN);

    /* Read back 4 data bytes from 0x000000 */
    uint8_t read_cmd[] = { PRAM_CMD_READ,
			   0, 0, 0 };
    wr = spi_write_blocking(spi1, read_cmd, sizeof(read_cmd));
    printf("Write returned: 0x%04X\n", wr);

    uint32_t result=0;
    int rr = spi_read_blocking(spi1, 0, (uint8_t*)&result, 4 ); 
    printf("Read result is 0x%04X, returned: 0x%08X\n", rr, result);

    cs_deselect(PICO_SPI_CSN_PIN);
#endif
    busy_wait_us_32(2000000);

  }

#if 0
#define BUF_LEN 256
  
  uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

  // Initialize output buffer
  for (size_t i = 0; i < BUF_LEN; ++i) {
    out_buf[i] = i;
    in_buf[i] = 0;
  }

  printf("Writable: %d\n", spi_is_writable(spi1));


  ram_write(spi1, PICO_DEFAULT_SPI_CSN_PIN, 0, out_buf, BUF_LEN);
  ram_read(spi1, PICO_DEFAULT_SPI_CSN_PIN, 0, in_buf, BUF_LEN);

  for( size_t i=0; i<16; i++ ) {
    printf("%d) 0x%02X   \n",i,in_buf[i]);
  }
  printf("\n");
#endif

  while(1);
}
