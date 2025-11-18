/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   custom_gpio_cfg.h
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   The file intends for GPIO initialization definition. 
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifndef __CUSTOM_GPIO_CFG_H__
#define __CUSTOM_GPIO_CFG_H__

/*========================================================================
| 
| GPIO initialization configurations.
|------------------------------------
| IMPORTANT NOTES:
|------------------
|
| This is the initialization list for GPIOs at the early of module booting.
| Developer can do configuring here if some GPIOs need to be initialized at
| the early booting. For example, some GPIO is used to control the power 
| supply of peripheral.
|
| Besides this config list, developer may call Ql_GPIO_Init() to initialize
| the parameters of I/O interfaces dynamically. But it's later than the 
| previous method on time sequence.
\=========================================================================*/
/*----------------------------------------------------------------------------------------------
{ Pin Name           |         Direction       |       Level         |   Pull Selection         }
 *---------------------------------------------------------------------------------------------*/

/* EINT CONFIGURATION - DISABLED
 * 
 * Boot-time GPIO configuration creates conflicts with EINT!
 * 
 * The official Quectel example (example_eint.c) shows:
 * 1. Open UART with FC_NONE
 * 2. Directly call Ql_EINT_Register (NO Ql_GPIO_Init!)
 * 3. Call Ql_EINT_Init
 * 
 * Configuring pins as GPIO (either at boot or in code) prevents EINT from claiming them.
 * EINT registration must be the FIRST operation on these pins.
 * 
 * NOTE: RI and DCD are excluded (used for I2C SCL/SDA)
 */
#if 0  // MUST be 0 - GPIO config prevents EINT from working!
GPIO_ITEM(PINNAME_DTR,            PINDIRECTION_IN,     PINLEVEL_LOW,  PINPULLSEL_PULLUP)
GPIO_ITEM(PINNAME_CTS,            PINDIRECTION_IN,     PINLEVEL_LOW,  PINPULLSEL_PULLUP)
GPIO_ITEM(PINNAME_RTS,            PINDIRECTION_IN,     PINLEVEL_LOW,  PINPULLSEL_PULLUP)
#endif

#endif //__CUSTOM_GPIO_CFG_H__
