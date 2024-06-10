/**
 * @file lv_jpegdec.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../../lvgl.h"
#if LV_USE_JPEGDEC

#include "lv_jpegdec.h"
#include "jpegdec.h"
#include <stdlib.h>

/*********************
 *      DEFINES
 *********************/

#define DECODER_NAME    "JPEGDEC"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_result_t decoder_info(lv_image_decoder_t * decoder, const void * src, lv_image_header_t * header);
static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
static void decoder_close(lv_image_decoder_t * dec, lv_image_decoder_dsc_t * dsc);
static lv_draw_buf_t * decode_jpeg_data(JPEGIMAGE * jpg, const void * jpeg_data, size_t jpeg_data_size, int iBPP);
/**********************
 *  STATIC VARIABLES
 **********************/
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the JPEG decoder functions in LVGL
 */
void lv_jpegdec_init(void)
{
    lv_image_decoder_t * dec = lv_image_decoder_create();
    lv_image_decoder_set_info_cb(dec, decoder_info);
    lv_image_decoder_set_open_cb(dec, decoder_open);
    lv_image_decoder_set_close_cb(dec, decoder_close);
}

void lv_jpegdec_deinit(void)
{
    lv_image_decoder_t * dec = NULL;
    while((dec = lv_image_decoder_get_next(dec)) != NULL) {
        if(dec->info_cb == decoder_info) {
            lv_image_decoder_delete(dec);
            break;
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Get info about a JPEG image
 * @param decoder   pointer to the decoder where this function belongs
 * @param src       can be file name or pointer to a C array
 * @param header    image information is set in header parameter
 * @return          LV_RESULT_OK: no error; LV_RESULT_INVALID: can't get the info
 */
static lv_result_t decoder_info(lv_image_decoder_t * decoder, const void * src, lv_image_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/
    uint8_t cBuf[32];
    uint16_t w = 0, h = 0, iMarker = 0;
    int iBpp;
    int i = 2, j = 2; /* Offset of first marker */
    lv_fs_file_t f;
    lv_fs_res_t res;
    uint32_t rn, iFileSize;
    const lv_image_dsc_t * img_dsc = src;
    lv_image_src_t src_type = lv_image_src_get_type(src);          /*Get the source type*/

    /*If it's a JPEG file...*/
    if(src_type == LV_IMAGE_SRC_FILE) {
        const char * fn = src;
        /* Read the width and height from the file */
        res = lv_fs_open(&f, fn, LV_FS_MODE_RD);
        lv_fs_seek(&f, 0, LV_FS_SEEK_END);
        lv_fs_tell(&f, &rn);
        lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
        iFileSize = (int)rn;
        if(res != LV_FS_RES_OK) return LV_RESULT_INVALID;
        lv_fs_read(&f, cBuf, sizeof(cBuf), &rn);
    }
    else {
        memcpy(cBuf, img_dsc->data, sizeof(cBuf));
        iFileSize = img_dsc->data_size;
    }

    if(cBuf[0] == 0xff && cBuf[1] == 0xd8) {  // a JPEG file
        while(i < 32 && iMarker != 0xffc0 && j < iFileSize) {
            iMarker = MOTOSHORT(&cBuf[i]) & 0xfffc;
            if(iMarker < 0xff00) { // invalid marker, could be generated by "Arles Image Web Page Creator" or Accusoft
                i += 2;
                continue; // skip 2 bytes and try to resync
            }
            if(iMarker == 0xffc0)  // the one we're looking for
                break;
            j += 2 + MOTOSHORT(&cBuf[i + 2]); /* Skip to next marker */
            if(j < iFileSize) { // need to read more
                if(src_type == LV_IMAGE_SRC_FILE) {
                    lv_fs_seek(&f, j, LV_FS_SEEK_SET);
                    lv_fs_read(&f, cBuf, sizeof(cBuf), &rn);
                }
                else {
                    memcpy(cBuf, &img_dsc->data[j], sizeof(cBuf));
                }
                i = 0;
            }
        } // while searching for tags
        if(src_type == LV_IMAGE_SRC_FILE) {
            lv_fs_close(&f);
        }
        if(iMarker != 0xffc0) {
            return LV_RESULT_INVALID; /* error - invalid file? */
        }
        else {
            iBpp = cBuf[i + 4]; // bits per sample
            h = MOTOSHORT(&cBuf[i + 5]);
            w = MOTOSHORT(&cBuf[i + 7]);
            iBpp = iBpp * cBuf[i + 9]; /* Bpp = number of components * bits per sample */
            //ucSubSample = cBuf[i+11];
            iMarker = MOTOSHORT(&cBuf[i]);
            if(iMarker != 0xffc0) {  /* only baseline images are supported */
                LV_LOG_WARN("Unsupported JPEG file options\n");
                return LV_RESULT_INVALID;
            }
            //sprintf(szOptions, ", type = %s, color subsampling = %d:%d", szJPEGTypes[iMarker & 3], (ucSubSample>>4),(ucSubSample & 0xf));
            if(iBpp == 8 || LV_COLOR_DEPTH == 8)
                header->cf = LV_COLOR_FORMAT_L8; // tell it we'll return grayscale
            else if(LV_COLOR_DEPTH == 16)
                header->cf = LV_COLOR_FORMAT_RGB565;
            else
                header->cf = LV_COLOR_FORMAT_ARGB8888;
            header->w = w;
            header->h = h;
            return LV_RESULT_OK;
        }
    }
    return LV_RESULT_INVALID;         /*If didn't succeeded earlier then it's an error*/
}

/**
 * Open a PNG image and decode it into dsc.decoded
 * @param decoder   pointer to the decoder where this function belongs
 * @param dsc       decoded image descriptor
 * @return          LV_RESULT_OK: no error; LV_RESULT_INVALID: can't open the image
 */
static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);

    uint8_t * jpeg_data = NULL;
    size_t jpeg_data_size = 0;
    JPEGIMAGE * jpg = NULL;
    int iBPP; // selects RGB565 or RGB8888 output

    iBPP = LV_COLOR_DEPTH / 8;
    jpg = (JPEGIMAGE *)lv_malloc(sizeof(JPEGIMAGE));
    if(!jpg) {
        LV_LOG_WARN("allocation of JPEGIMAGE structure failed\n");
        return LV_RESULT_INVALID; // not enough memory for JPEGIMAGE structure
    }

    if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char * fn = dsc->src;
        if(lv_strcmp(lv_fs_get_ext(fn), "jpg") == 0) {              /*Check the extension*/
            lv_fs_file_t f;
            uint32_t u32Size, u32BytesRead;
            lv_fs_res_t res = lv_fs_open(&f, fn, LV_FS_MODE_RD);
            if(res != LV_FS_RES_OK) {
                lv_free(jpg);
                return LV_RESULT_INVALID; /* File open error */
            }
            if(lv_fs_seek(&f, 0, LV_FS_SEEK_END) != 0) {
                lv_fs_close(&f);
                lv_free(jpg);
                return LV_RESULT_INVALID;
            }
            lv_fs_tell(&f, &u32Size); /* Get the file size */
            lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
            jpeg_data = (uint8_t *)lv_malloc(u32Size);
            if(!jpeg_data) {  /* Not enough RAM */
                lv_fs_close(&f);
                lv_free(jpg);
                return LV_RESULT_INVALID;
            }
            res = lv_fs_read(&f, jpeg_data, u32Size, &u32BytesRead);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK || u32BytesRead != u32Size) {
                lv_fs_close(&f);
                lv_free(jpeg_data);
                lv_free(jpg);
                return LV_RESULT_INVALID;
            }
        }
    }
    else if(dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        jpeg_data = (uint8_t *)img_dsc->data;
        jpeg_data_size = img_dsc->data_size;
    }
    else {
        lv_free(jpg);
        return LV_RESULT_INVALID;
    }

    lv_draw_buf_t * decoded = decode_jpeg_data(jpg, jpeg_data, jpeg_data_size, iBPP);
    lv_free(jpg); // no longer needed
    if(dsc->src_type == LV_IMAGE_SRC_FILE) lv_free((void *)jpeg_data);

    if(!decoded) {
        LV_LOG_WARN("Error decoding JPEG\n");
        return LV_RESULT_INVALID;
    }
    LV_LOG_WARN("jpeg decode succeeded\n");
    lv_draw_buf_t * adjusted = lv_image_decoder_post_process(dsc, decoded);
    if(adjusted == NULL) {
        LV_LOG_WARN("jpeg post process failed\n");
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }

    /*The adjusted draw buffer is newly allocated.*/
    if(adjusted != decoded) {
        lv_draw_buf_destroy(decoded);
        decoded = adjusted;
    }

    dsc->decoded = decoded;

    if(dsc->args.no_cache) return LV_RESULT_OK;

#if LV_CACHE_DEF_SIZE > 0
    lv_image_cache_data_t search_key;
    search_key.src_type = dsc->src_type;
    search_key.src = dsc->src;
    search_key.slot.size = decoded->data_size;

    lv_cache_entry_t * entry = lv_image_decoder_add_to_cache(decoder, &search_key, decoded, NULL);

    if(entry == NULL) {
        return LV_RESULT_INVALID;
    }
    dsc->cache_entry = entry;
#endif

    return LV_RESULT_OK;    /*If not returned earlier then it failed*/
}

/**
 * Close JPEG image and free data
 * @param decoder   pointer to the decoder where this function belongs
 * @param dsc       decoded image descriptor
 * @return          LV_RESULT_OK: no error; LV_RESULT_INVALID: can't open the image
 */
static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);

    if(dsc->args.no_cache || LV_CACHE_DEF_SIZE == 0)
        lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    else
        lv_cache_release(dsc->cache, dsc->cache_entry, NULL);
}

static lv_draw_buf_t * decode_jpeg_data(JPEGIMAGE * jpg, const void * jpeg_data, size_t jpeg_data_size, int iBPP)
{
    lv_draw_buf_t * decoded = NULL;
    uint8_t * pOut;
    int rc;

    /*Decode the image*/
    rc = JPEG_openRAM(jpg, (uint8_t *)jpeg_data, (int)jpeg_data_size, NULL);
    if(!rc) {
        LV_LOG_WARN("JPEG_openRAM failed\n");
        return NULL;
    }
    /*Allocate a full frame buffer as needed*/
    pOut = lv_malloc(jpg->iWidth * jpg->iHeight * iBPP);
    if(!pOut) {  // no memory
        return NULL;
    }
    jpg->pFramebuffer = pOut; /* Decode directly into a framebuffer */
    if(iBPP == 4) {
        jpg->ucPixelType = RGB8888;
    }
    else if(iBPP == 2) {
        jpg->ucPixelType = RGB565_LITTLE_ENDIAN;
    }
    else {
        jpg->ucPixelType = EIGHT_BIT_GRAYSCALE;
    }
    rc = JPEG_decode(jpg, 0, 0, (iBPP == 1) ? JPEG_LUMA_ONLY : 0);
    if(!rc) { /* Something went wrong */
        lv_free(pOut);
        return NULL;
    }
    decoded = (lv_draw_buf_t *)lv_malloc(sizeof(lv_draw_buf_t));
    if(!decoded) {
        lv_free(pOut); // ran out of memory
        return NULL;
    }

    decoded->header.stride = jpg->iWidth * iBPP;
    decoded->header.w = jpg->iWidth;
    decoded->header.h = jpg->iHeight;
    decoded->header.flags = LV_IMAGE_FLAGS_ALLOCATED;
    if(iBPP == 4) {
        decoded->header.cf = LV_COLOR_FORMAT_ARGB8888;
    }
    else if(iBPP == 2) {
        decoded->header.cf = LV_COLOR_FORMAT_RGB565;
    }
    else {
        decoded->header.cf = LV_COLOR_FORMAT_L8; // grayscale
    }
    decoded->header.magic = LV_IMAGE_HEADER_MAGIC;
    decoded->data_size = (jpg->iWidth * jpg->iHeight * iBPP);
    decoded->data = pOut;
    decoded->unaligned_data = pOut;
    return decoded;
}

#endif /*LV_USE_JPEGDEC*/
