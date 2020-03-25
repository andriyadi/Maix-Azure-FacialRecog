#ifndef __MY_CAM_OV2640_H
#define __MY_CAM_OV2640_H

#include <stdint.h>
#include <stdbool.h>

#define CAMERA_WIDTH 320
#define CAMERA_HEIGHT 240

bool camera_init();
uint32_t *camera_lcd_buffer();
uint32_t *camera_snapshot();
uint8_t *camera_ai_buffer();

#endif //__MY_CAM_OV2640_H
