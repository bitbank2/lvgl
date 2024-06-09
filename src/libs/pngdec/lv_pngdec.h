/**
 * @file lv_pngdec.h
 *
 */

#ifndef LV_PNGDEC_H
#define LV_PNGDEC_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_conf_internal.h"
#if LV_USE_PNGDEC

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Register the PNG decoder functions in LVGL
 */
void lv_pngdec_init(void);

void lv_pngdec_deinit(void);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_PNGDEC*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PNGDEC_H*/
