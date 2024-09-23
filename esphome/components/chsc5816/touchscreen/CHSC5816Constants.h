/**
 *
 * @license MIT License
 *
 * Copyright (c) 2022 lewis he
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      CHSC5816Constants.h
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2023-04-17
 *
 */

#pragma once

#define CHSC5816_SLAVE_ADDRESS (0x2E)

#define CHSC5816_REG_CMD_BUFF (0x20000000U)
#define CHSC5816_REG_RSP_BUFF (0x20000000U)
#define CHSC5816_REG_IMG_HEAD (0x20000014U)
// #define CHSC5816_REG_IMG_HEAD                  (0x14000020U)
#define CHSC5816_REG_POINT (0x2000002CU)
#define CHSC5816_REG_WR_BUFF (0x20002000U)
#define CHSC5816_REG_RD_BUFF (0x20002400U)
#define CHSC5816_REG_HOLD_MCU (0x40007000U)
#define CHSC5816_REG_AUTO_FEED (0x40007010U)
#define CHSC5816_REG_REMAP_MCU (0x40007000U)
#define CHSC5816_REG_RELEASE_MCU (0x40007000U)
#define CHSC5816_REG_BOOT_STATE (0x20000018U)
// #define CHSC5816_REG_BOOT_STATE                (0x18000020U)

#define CHSC5816_HOLD_MCU_VAL (0x12044000U)
#define CHSC5816_AUTO_FEED_VAL (0x0000925aU)
#define CHSC5816_REMAP_MCU_VAL (0x12044002U)
#define CHSC5816_RELEASE_MCU_VAL (0x12044003U)

#define CHSC5816_REG_VID_PID_BACKUP (40 * 1024 + 0x10U)

#define CHSC5816_SIG_VALUE (0x43534843U)
/*ctp work staus*/
#define CHSC5816_POINTING_WORK (0x00000000U)
#define CHSC5816_READY_UPGRADE (1 << 1)
#define CHSC5816_UPGRAD_RUNING (1 << 2)
#define CHSC5816_SLFTEST_RUNING (1 << 3)
#define CHSC5816_SUSPEND_GATE (1 << 16)
#define CHSC5816_GUESTURE_GATE (1 << 17)
#define CHSC5816_PROXIMITY_GATE (1 << 18)
#define CHSC5816_GLOVE_GATE (1 << 19)
#define CHSC5816_ORIENTATION_GATE (1 << 20)
