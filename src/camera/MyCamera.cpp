#include "MyCamera.hpp"

#include <stdio.h>
//////////// HAL ///////////////
#include "sysctl.h"
#include "fpioa.h"
#include "dvp.h"
#include "stdlib.h"
#include "utils.h"
#include "plic.h"
#include "math.h"

#ifdef __cplusplus
extern "C" {
#endif
    #include "ov2640.h"
#ifdef __cplusplus
}
#endif

#define PLL0_OUTPUT_FREQ 800000000UL
#define PLL1_OUTPUT_FREQ 400000000UL

volatile static uint8_t g_dvp_finish_flag = 0;
volatile static uint8_t g_ram_mux = 0;

uint32_t g_lcd_gram0[CAMERA_WIDTH * CAMERA_HEIGHT * 2/4] __attribute__((aligned(64)));
uint32_t g_lcd_gram1[CAMERA_WIDTH * CAMERA_HEIGHT * 2/4] __attribute__((aligned(64)));

uint8_t g_ai_buf[CAMERA_WIDTH * CAMERA_HEIGHT *3] __attribute__((aligned(128)));

static int on_irq_dvp(void* ctx)
{
    if (dvp_get_interrupt(DVP_STS_FRAME_FINISH))
    {
        /* switch gram */
        dvp_set_display_addr(g_ram_mux ? (uint32_t)((long)g_lcd_gram0) : ((long)g_lcd_gram1));

        dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
        g_dvp_finish_flag = 1;
    }
    else
    {
        if(g_dvp_finish_flag == 0)
            dvp_start_convert();
        dvp_clear_interrupt(DVP_STS_FRAME_START);
    }

    return 0;
}

static void io_mux_init(void)
{
    /* Init DVP IO map and function settings */
    fpioa_set_function(42, FUNC_CMOS_RST);
    fpioa_set_function(44, FUNC_CMOS_PWDN);
    fpioa_set_function(46, FUNC_CMOS_XCLK);
    fpioa_set_function(43, FUNC_CMOS_VSYNC);
    fpioa_set_function(45, FUNC_CMOS_HREF);
    fpioa_set_function(47, FUNC_CMOS_PCLK);
    fpioa_set_function(41, FUNC_SCCB_SCLK);
    fpioa_set_function(40, FUNC_SCCB_SDA);

    // /* Init SPI IO map and function settings */
    // fpioa_set_function(38, FUNC_GPIOHS0 + DCX_GPIONUM);
    // fpioa_set_function(36, FUNC_SPI0_SS3);
    // fpioa_set_function(39, FUNC_SPI0_SCLK);
    // fpioa_set_function(37, FUNC_GPIOHS0 + RST_GPIONUM);

    // sysctl_set_spi0_dvp_data(1);
}

bool camera_init() {
    sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);

    io_mux_init();

    printf("DVP init\n");
    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    dvp_set_output_enable(DVP_OUTPUT_AI, 1);
    dvp_set_output_enable(DVP_OUTPUT_DISPLAY, 1);
    dvp_set_image_format(DVP_CFG_RGB_FORMAT);
//    dvp_set_image_format(DVP_CFG_YUV_FORMAT);
    dvp_set_image_size(CAMERA_WIDTH, CAMERA_HEIGHT);
    ov2640_init();

    dvp_set_ai_addr((uint32_t)((long)g_ai_buf), (uint32_t)((long)(g_ai_buf + CAMERA_WIDTH * CAMERA_HEIGHT)), (uint32_t)((long)(g_ai_buf + CAMERA_WIDTH * CAMERA_HEIGHT * 2)));
    dvp_set_display_addr((uint32_t)((long)g_lcd_gram0));

    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
    dvp_disable_auto();

    /* DVP interrupt config */
    printf("DVP interrupt config\n");
    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);

    /* enable global interrupt */
    sysctl_enable_irq();

    /* Camera start */
    printf("Camera start\n");
    g_ram_mux = 0;
    g_dvp_finish_flag = 0;
    dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);

    return true;
}

int reverse_u32pixel(uint32_t* addr, uint32_t length)
{
  if(NULL == addr)
    return -1;

  uint32_t data;
  uint32_t* pend = addr+length;
  for(;addr<pend;addr++)
  {
	  data = *(addr);
	  *(addr) = ((data & 0x000000FF) << 24) | ((data & 0x0000FF00) << 8) | 
                ((data & 0x00FF0000) >> 8) | ((data & 0xFF000000) >> 24) ;
  }  //1.7ms
  
  
  return 0;
}

uint32_t *camera_lcd_buffer() {
    while (g_dvp_finish_flag == 0)
        ;

    g_ram_mux ^= 0x01;
    uint32_t *img = (g_ram_mux ? g_lcd_gram0 : g_lcd_gram1);
    reverse_u32pixel(img, CAMERA_WIDTH*CAMERA_HEIGHT/2);
    g_dvp_finish_flag = 0;

    return img;
}

uint32_t *camera_snapshot() {
    while (g_dvp_finish_flag == 0)
        ;

    g_ram_mux ^= 0x01;
    g_dvp_finish_flag = 0;

    uint32_t *img = (g_ram_mux ? g_lcd_gram0 : g_lcd_gram1);
    reverse_u32pixel(img, CAMERA_WIDTH*CAMERA_HEIGHT/2);

    return img;
}

uint8_t *camera_ai_buffer() {
    return g_ai_buf;
}
