/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _OV2640_H
#define _OV2640_H

#include <stdint.h>

#define OV2640_ADDR         0x60

#define BANK_SEL                0xFF
#define BANK_SEL_DSP            0x00
#define BANK_SEL_SENSOR         0x01

#define COM7                0x12
#define COM7_SRST           0x80
#define COM7_RES_UXGA       0x00 /* UXGA */
#define COM7_RES_SVGA       0x40 /* SVGA */
#define COM7_RES_CIF        0x20 /* CIF  */
#define COM7_ZOOM_EN        0x04 /* Enable Zoom */
#define COM7_COLOR_BAR      0x02 /* Enable Color Bar Test */
#define COM7_GET_RES(x)     ((x)&0x70)

int ov2640_init(void);
int ov2640_read_id(uint16_t *manuf_id, uint16_t *device_id);

#endif /* _OV2640_H */

