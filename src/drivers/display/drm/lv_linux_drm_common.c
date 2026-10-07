/**
 * @file lv_linux_drm_common.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_linux_drm.h"

#if LV_USE_LINUX_DRM

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "lv_linux_drm.h"
#include "../../../stdlib/lv_sprintf.h"
#include "../../../stdlib/lv_string.h"
#include "../../../stdlib/lv_mem.h"
#include "../../../misc/lv_log.h"

/*********************
 *      DEFINES
 *********************/

#define LV_DRM_CARD_PATH "/dev/dri/card"
#define LV_DRM_MAX_CARDS 8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int rank_card(const char * path);
static char * find_best_card(void);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

char * lv_linux_drm_find_device_path(void)
{
    return find_best_card();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Rank a DRM card node by its suitability for scanout
 *
 * On boards with several DRM devices (e.g. Pi 5: v3d render only, vc4 HDMI,
 * rp1 DSI/DPI) the /dev/dri/cardN order changes between boots, so the right
 * device must be detected instead of taking the first one.
 *
 * @return 0 unusable, 1 KMS with dumb buffers, 2 + connected connector,
 *         3 + the connector is a DSI/DPI panel
 */
static int rank_card(const char * path)
{
    int fd;
    int i;
    int rank;
    uint64_t has_dumb;
    drmModeRes * res;
    drmModeConnector * conn;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if(fd < 0) {
        return 0;
    }

    has_dumb = 0;
    if(drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) != 0 || !has_dumb) {
        close(fd);
        return 0;
    }

    res = drmModeGetResources(fd);
    if(res == NULL || res->count_connectors <= 0) {
        if(res != NULL) {
            drmModeFreeResources(res);
        }
        close(fd);
        return 0;
    }

    rank = 1;
    for(i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if(conn == NULL) {
            continue;
        }

        if(conn->connection == DRM_MODE_CONNECTED) {
            if(conn->connector_type == DRM_MODE_CONNECTOR_DSI ||
               conn->connector_type == DRM_MODE_CONNECTOR_DPI) {
                rank = 3;
            }
            else if(rank < 2) {
                rank = 2;
            }
        }

        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    close(fd);

    return rank;
}

static char * find_best_card(void)
{
    char path[32];
    char best_path[32];
    int card;
    int rank;
    int best_rank = 0;

    for(card = 0; card < LV_DRM_MAX_CARDS; card++) {
        lv_snprintf(path, sizeof(path), LV_DRM_CARD_PATH "%d", card);

        rank = rank_card(path);
        if(rank > best_rank) {
            best_rank = rank;
            lv_strcpy(best_path, path);
        }
    }

    if(best_rank == 0) {
        LV_LOG_WARN("No usable DRM/KMS device found");
        return NULL;
    }

    LV_LOG_USER("Using DRM card %s (rank %d)", best_path, best_rank);
    return lv_strdup(best_path);
}

int32_t lv_linux_drm_mode_get_horizontal_resolution(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->hdisplay;
}

int32_t lv_linux_drm_mode_get_vertical_resolution(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->vdisplay;
}

int32_t lv_linux_drm_mode_get_refresh_rate(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return 0;
    }
    return mode->vrefresh;
}

bool lv_linux_drm_mode_is_preferred(const lv_linux_drm_mode_t * mode)
{
    if(!mode) {
        return false;
    }
    return (mode->type & DRM_MODE_TYPE_PREFERRED) != 0;
}

#endif /*LV_USE_LINUX_DRM*/
