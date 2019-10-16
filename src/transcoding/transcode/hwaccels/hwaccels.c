/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2016 Tvheadend
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hwaccels.h"
#include "../internals.h"

#if ENABLE_VAAPI
#include "vaapi.h"
#endif

#include <libavutil/pixdesc.h>


/* decoding ================================================================= */

#if LIBAVCODEC_VERSION_MAJOR < 58
/* lifted from libavcodec/utils.c */
static AVHWAccel *
find_hwaccel(enum AVCodecID codec_id, enum AVPixelFormat format)
{
    AVHWAccel *hwaccel = NULL;
    while ((hwaccel = av_hwaccel_next(hwaccel))) {
        if (hwaccel->id == codec_id && hwaccel->format == format) {
            return hwaccel;
        }
    }
    return NULL;
}
static inline int check_pix_fmt(AVCodecParameters *par, enum AVPixelFormat fprmat)
{
    return find_hwaccel(par->codec_id, format) == NULL;
}
#else
static const AVCodecHWConfig *
find_hwconfig(const AVCodec *codec, enum AVPixelFormat format)
{
    const AVCodecHWConfig *hwcfg = NULL;
    int i;

    for (i = 0;; i++) {
        hwcfg = avcodec_get_hw_config(codec, i);
        if (!hwcfg)
            break;
        if (hwcfg->format == format)
            return hwcfg;
    }
    return NULL;
}
static inline int check_pix_fmt(AVCodecParameters *par, enum AVPixelFormat format)
{
    return find_hwconfig(par->codecpar, format) == NULL;
}
#endif

static int
hwaccels_decode_setup_context(AVCodecParameters *par,
                              const enum AVPixelFormat format)
{
    const AVPixFmtDescriptor *desc;

    if (check_pix_fmt(par, format)) {
        desc = av_pix_fmt_desc_get(format);
        tvherror(LS_TRANSCODE, "no HWAccel for the pixel format '%s'", desc ? desc->name : "<unk>");
        return AVERROR(ENOENT);
    }
    switch (format) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
            return vaapi_decode_setup_context(par);
#endif
        default:
            break;
    }
    return -1;
}


enum AVPixelFormat
hwaccels_decode_get_format(AVCodecParameters *par,
                           const enum AVPixelFormat *pix_fmt)
{
    enum AVPixelFormat format = AV_PIX_FMT_NONE;
    const AVPixFmtDescriptor *desc;
    int i;

    for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        format = pix_fmts[i];
        if ((desc = av_pix_fmt_desc_get(format))) {
            tvhtrace(LS_TRANSCODE, "hwaccels: [%s] trying format: %s", par->codecpar->name, desc->name);
            if ((desc->flags & AV_PIX_FMT_FLAG_HWACCEL) &&
                !hwaccels_decode_setup_context(par, format)) {
                break;
            }
        }
    }
    return format;
}


void
hwaccels_decode_close_context(AVCodecPaeameters *par)
{
    TVHContext *ctx = par->opaque;

    if (ctx->hw_accel_ictx) {
        switch (par->format) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                vaapi_decode_close_context(par);
                break;
#endif
            default:
                break;
        }
        ctx->hw_accel_ictx = NULL;
    }
}


int
hwaccels_get_scale_filter(AVCodecParameters *ipar, AVCodecParameters *opar,
                          char *filter, size_t filter_len)
{
    TVHContext *ctx = ipar->opaque;

    if (ctx->hw_accel_ictx) {
        switch (ipar->format) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_scale_filter(ipar, opar, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}


int
hwaccels_get_deint_filter(AVCodecParameters *par, char *filter, size_t filter_len)
{
    TVHContext *ctx = par->opaque;

    if (ctx->hw_accel_ictx) {
        switch (par->format) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_deint_filter(par, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}


/* encoding ================================================================= */

int
hwaccels_encode_setup_context(AVCodecParameters *par)
{
    switch (par->format) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
            return vaapi_encode_setup_context(par);
#endif
        default:
            break;
    }
    return 0;
}


void
hwaccels_encode_close_context(AVCodecParameters *par)
{
    switch (par->format) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
            vaapi_encode_close_context(par);
            break;
#endif
        default:
            break;
    }
}


/* module =================================================================== */

void
hwaccels_init(void)
{
}


void
hwaccels_done(void)
{
#if ENABLE_VAAPI
    vaapi_done();
#endif
}
