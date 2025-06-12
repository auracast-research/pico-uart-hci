/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #ifndef _TUSB_CONFIG_H_
 #define _TUSB_CONFIG_H_
 
 #ifdef __cplusplus
  extern "C" {
 #endif
 
 #define CFG_TUD_ENABLED         (1)
 
 // Legacy RHPORT configuration
 #define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
 #ifndef BOARD_TUD_RHPORT
 #define BOARD_TUD_RHPORT        (0)
 #endif
 // end legacy RHPORT
 
 //------------------------
 // DEVICE CONFIGURATION //
 //------------------------
 
 // Enable 2 CDC classes
 #if HCI_DEBUG_LOG
    #define CFG_TUD_CDC             (2)
#else
    #define CFG_TUD_CDC             (1)
#endif
 // Set CDC FIFO buffer sizes
 #define CFG_TUD_CDC_RX_BUFSIZE  (2048)
 #define CFG_TUD_CDC_TX_BUFSIZE  (2048)
 
 #ifdef __cplusplus
  }
 #endif
 
 #endif /* _TUSB_CONFIG_H_ */