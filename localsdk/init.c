#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "./init.h"
#include "./localsdk.h"
#include "./osd/osd.h"
#include "./video/video.h"
#include "./audio/audio.h"
#include "./speaker/speaker.h"
#include "./alarm/alarm.h"
#include "./night/night.h"
#include "./../logger/logger.h"
#include "./../configs/configs.h"

// Log printf function
static int logprintf(const char *format, ...) {
    int result = 0;
    char *message;
    va_list params;
    va_start(params, format);
    if(vasprintf(&message, format, params) > 0) {
        result = logger("localsdk", "logprintf", LOGGER_LEVEL_INFO, message);
        free(message);
    }
    va_end(params);
    return result;
}

// Get firmware version
char *firmware_version() {
    char *firmware_version = "Unknown";
    FILE *version_file;
    if(version_file = fopen("/usr/app/share/.version", "r")) {
        fseek(version_file, 0, SEEK_END);
        long version_size = ftell(version_file) - 1;
        fseek(version_file, 0, SEEK_SET);
        firmware_version = malloc(version_size);
        fread(firmware_version, 1, version_size, version_file);
        fclose(version_file);
    }
    return firmware_version;
}

// Init all
bool all_init() {
    logger("localsdk-init", "all_init", LOGGER_LEVEL_DEBUG, "Function is called...");
    if(localsdk_set_logprintf_func(logprintf) == LOCALSDK_OK) {
        logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "localsdk_set_logprintf_func()");
        //if(localsdk_set_shellcall_func(shellcall_func) == LOCALSDK_OK) { // FIXME
            //logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "localsdk_set_shellcall_func()");
            if(localsdk_init() == LOCALSDK_OK) {
                logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "localsdk_init()");
                if(localsdk_get_version() == LOCALSDK_CURRENT_VERSION) {
                    logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "video_init()");
                    if(osd_init()) {
                        logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "localsdk_get_version()");
                        if(video_init()) {
                            logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "osd_init()");
                            if(audio_init()) {
                                logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "audio_init()");
                                if(speaker_init()) {
                                    logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "speaker_init()");
                                    if(alarm_init()) {
                                        logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "alarm_init()");
                                        if(night_init()) {
                                            logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "night_init()");
                                            
                                            logger("localsdk-init", "all_init", LOGGER_LEVEL_DEBUG, "Function completed.");
                                            return true;
                                        } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "night_init()");
                                    } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "alarm_init()");
                                } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "speaker_init()");
                            } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "audio_init()");
                        } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "video_init()");
                    } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "osd_init()");
                } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "localsdk_get_version()");
            } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "localsdk_init()");
        //} else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "localsdk_set_shellcall_func()");
    } else logger("localsdk-init", "all_init", LOGGER_LEVEL_ERROR, "%s error!", "localsdk_set_logprintf_func()");
    if(all_free()) {
        logger("localsdk-init", "all_init", LOGGER_LEVEL_INFO, "%s success.", "all_free()");
    } else logger("localsdk-init", "all_init", LOGGER_LEVEL_WARNING, "%s error!", "all_free()");
    logger("localsdk-init", "all_init", LOGGER_LEVEL_DEBUG, "Function completed.");
    return false;
}

// Free all
bool all_free() {
    bool result = true;
    logger("localsdk-init", "all_free", LOGGER_LEVEL_DEBUG, "Function is called...");
    
    // Free night mode
    if(night_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "night_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "night_free()");
        result = false;
    }
    
    // Free alarm
    if(alarm_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "alarm_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "alarm_free()");
        result = false;
    }
    
    // Free speaker
    if(speaker_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "speaker_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "speaker_free()");
        result = false;
    }
    
    // Free audio
    if(audio_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "audio_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "audio_free()");
        result = false;
    }
    
    // Free OSD
    if(osd_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "osd_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "osd_free()");
        result = false;
    }
    
    // Free video
    if(video_free()) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "video_free()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "video_free()");
        result = false;
    }
    
    // Free localsdk
    if(localsdk_destory() == LOCALSDK_OK) {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_INFO, "%s success.", "localsdk_destory()");
    } else {
        logger("localsdk-init", "all_free", LOGGER_LEVEL_WARNING, "%s error!", "localsdk_destory()");
        result = false;
    }
    
    logger("localsdk-init", "all_free", LOGGER_LEVEL_DEBUG, "Function completed.");
    return result;
}
