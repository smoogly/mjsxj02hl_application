#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>

#include "./alarm.h"
#include "./../localsdk.h"
#include "./../osd/osd.h"
#include "./../../logger/logger.h"
#include "./../../configs/configs.h"
#include "./../../yyjson/src/yyjson.h"
#include "./../../mqtt/mqtt.h"

static pthread_t timeout_thread;
static int alarm_time_motion, alarm_time_humanoid;

// MQTT send info
static bool alarm_state_mqtt(bool motion, bool humanoid) {
    LOGGER(LOGGER_LEVEL_DEBUG, "Function is called...");
    bool result = true;
    
    // Send alarm state info
    yyjson_mut_doc *json_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *json_root = yyjson_mut_obj(json_doc);
    yyjson_mut_doc_set_root(json_doc, json_root);
    
    // Motion state
    if(yyjson_mut_obj_add_bool(json_doc, json_root, "motion", motion)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "yyjson_mut_obj_add_bool(motion)");
    else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "yyjson_mut_obj_add_bool()");
    
    // Humanoid state
    if(yyjson_mut_obj_add_bool(json_doc, json_root, "humanoid", humanoid)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "yyjson_mut_obj_add_bool(humanoid)");
    else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "yyjson_mut_obj_add_bool()");
    
    // Send it
    const char *json = yyjson_mut_write(json_doc, 0, NULL);
    if(result &= !!json) {
        LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "yyjson_mut_write()");
        
        char *alarm_topic = mqtt_fulltopic(MQTT_ALARM_TOPIC);
        if(result &= mqtt_send(alarm_topic, (char *) json)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "mqtt_send(MQTT_ALARM_TOPIC)");
        else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "mqtt_send(MQTT_ALARM_TOPIC)");
        
        free(alarm_topic);
        free((void *) json);
    } else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "yyjson_mut_write()");
    
    // Free resources
    yyjson_mut_doc_free(json_doc);
    
    LOGGER(LOGGER_LEVEL_DEBUG, "Function completed (result = %s).", (result ? "true" : "false"));
    return result;
}

// State timeout (for pthread)
static void* alarm_state_timeout(void *args) {
    LOGGER(LOGGER_LEVEL_DEBUG, "Function is called...");
    
    bool alarm_state_motion, alarm_state_humanoid;
    bool alarm_change_motion, alarm_change_humanoid;
    
    alarm_time_motion = 0;
    alarm_time_humanoid = 0;
    alarm_state_motion = false;
    alarm_state_humanoid = false;
    
    do {
        alarm_change_motion = false;
        alarm_change_humanoid = false;
        
        // Motion
        if(alarm_time_motion > 0) {
            if(alarm_state_motion) {
                if((int) time(NULL) - alarm_time_motion > APP_CFG.alarm.motion_timeout) {
                    alarm_time_motion = 0;
                    alarm_state_motion = false;
                    alarm_change_motion = true;
                }
            } else {
                alarm_state_motion = true;
                alarm_change_motion = true;
            }
        }
        
        // Humanoid
        if(alarm_time_humanoid > 0) {
            if(alarm_state_humanoid) {
                if((int) time(NULL) - alarm_time_humanoid > APP_CFG.alarm.humanoid_timeout) {
                    alarm_time_humanoid = 0;
                    alarm_state_humanoid = false;
                    alarm_change_humanoid = true;
                }
            } else {
                alarm_state_humanoid = true;
                alarm_change_humanoid = true;
            }
        }
        
        // State changed
        if(alarm_change_motion || alarm_change_humanoid) {
        
            // Motion
            if(alarm_change_motion) {
                LOGGER(LOGGER_LEVEL_INFO, "Change %s status: %d", "motion", alarm_state_motion);
                // Execute the command
                if(alarm_state_motion && APP_CFG.alarm.motion_detect_exec && APP_CFG.alarm.motion_detect_exec[0]) {
                    // Detect
                    if(system(APP_CFG.alarm.motion_detect_exec) == 0) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "system(motion_detect_exec)");
                    else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "system(motion_detect_exec)");
                } else if(!alarm_state_motion && APP_CFG.alarm.motion_lost_exec && APP_CFG.alarm.motion_lost_exec[0]) {
                    // Lost
                    if(system(APP_CFG.alarm.motion_lost_exec) == 0) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "system(motion_lost_exec)");
                    else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "system(motion_lost_exec)");
                }
            }
            
            // Humanoid
            if(alarm_change_humanoid) {
                LOGGER(LOGGER_LEVEL_INFO, "Change %s status: %d", "humanoid", alarm_state_humanoid);
                // Execute the command
                if(alarm_state_motion && APP_CFG.alarm.humanoid_detect_exec && APP_CFG.alarm.humanoid_detect_exec[0]) {
                    // Detect
                    if(system(APP_CFG.alarm.humanoid_detect_exec) == 0) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "system(humanoid_detect_exec)");
                    else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "system(humanoid_detect_exec)");
                } else if(!alarm_state_humanoid && APP_CFG.alarm.humanoid_lost_exec && APP_CFG.alarm.humanoid_lost_exec[0]) {
                    // Lost
                    if(system(APP_CFG.alarm.humanoid_lost_exec) == 0) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "system(humanoid_lost_exec)");
                    else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "system(humanoid_lost_exec)");
                }
            }
            // MQTT
            if(mqtt_is_ready()) {
                if(alarm_state_mqtt(alarm_state_motion, alarm_state_humanoid)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "alarm_state_mqtt()");
                else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "alarm_state_mqtt()");
            }
        }
        
        sleep(1);
        pthread_testcancel();
    } while(true);
}

// Alarm callback
static int alarm_state_callback(LOCALSDK_ALARM_EVENT_INFO *eventInfo) {
    int result = LOCALSDK_OK;
    
    if(eventInfo) {
        // OSD rectangles callback
        result = osd_rectangles_callback(eventInfo);
        
        // Remember the timestamps of events
        if(eventInfo->state) {
            int current_timestamp = (int) time(NULL);
            switch(eventInfo->type) {
                case LOCALSDK_ALARM_TYPE_MOTION:
                    alarm_time_motion = current_timestamp;
                    break;
                case LOCALSDK_ALARM_TYPE_HUMANOID:
                    alarm_time_humanoid = current_timestamp;
                    break;
                default:
                    LOGGER(LOGGER_LEVEL_INFO, "Change %s status: %d", "unknown", eventInfo->state);
                    result = LOCALSDK_ERROR;
            }
        }
    } else result = LOCALSDK_ERROR;
    
    return result;
}

// Enable or disable alarm
bool alarm_switch(bool state) {
    LOGGER(LOGGER_LEVEL_DEBUG, "Function is called...");
    bool result = true;

    if (APP_CFG.alarm.enable == true) {
        LOGGER(LOGGER_LEVEL_INFO, "State: %s", (state ? "true" : "false"));

        // Switch alarm for motion
        if(result &= (local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_MOTION, state) == LOCALSDK_OK)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_MOTION)");
        else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_MOTION)");

        // Switch alarm for humanoid
        if(result &= (local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_HUMANOID, state)) == LOCALSDK_OK) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_HUMANOID)");
        else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "local_sdk_set_alarm_switch(LOCALSDK_ALARM_TYPE_HUMANOID)");
    } else {
        LOGGER(LOGGER_LEVEL_INFO, "Alarm switch ignored, because alarms are disabled");
    }
    
    LOGGER(LOGGER_LEVEL_DEBUG, "Function completed (result = %s).", (result ? "true" : "false"));
    return result;
}

// Init alarm
bool alarm_init() {
    LOGGER(LOGGER_LEVEL_DEBUG, "Function is called...");
    bool result = true;
    
    int changed_resolution_type;
    LOCALSDK_PICTURE_SIZE picture_size;
    if (APP_CFG.alarm.enable == true) {
        if(result &= (inner_change_resulu_type(LOCALSDK_VIDEO_RESOLUTION_640x360, &changed_resolution_type) == LOCALSDK_OK)) {
            LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "inner_change_resulu_type(LOCALSDK_VIDEO_RESOLUTION_640x360)");
            if(result &= (SAMPLE_COMM_SYS_GetPicSize(changed_resolution_type, &picture_size) == LOCALSDK_OK)) {
                LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "SAMPLE_COMM_SYS_GetPicSize(LOCALSDK_VIDEO_RESOLUTION_640x360)");
                if(result &= (local_sdk_alarm_init(picture_size.width, picture_size.height) == LOCALSDK_OK)) {
                    LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_alarm_init()");
                    if(result &= (local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_MOTION, APP_CFG.alarm.motion_sens) == LOCALSDK_OK)) {
                        LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_MOTION)");
                        if(result &= (local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_HUMANOID, APP_CFG.alarm.humanoid_sens) == LOCALSDK_OK)) {
                            LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_HUMANOID)");
                            if(result &= (local_sdk_alarm_state_set_callback(alarm_state_callback) == LOCALSDK_OK)) {
                                LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_alarm_state_set_callback(alarm_state_callback)");
                                if(result &= (pthread_create(&timeout_thread, NULL, alarm_state_timeout, NULL) == 0)) {
                                    LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "pthread_create(timeout_thread)");
                                } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "pthread_create(timeout_thread)");
                            } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "local_sdk_alarm_state_set_callback(alarm_state_callback)");
                        } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_HUMANOID)");
                    } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "local_sdk_set_alarm_sensitivity(LOCALSDK_ALARM_TYPE_MOTION)");
                } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "local_sdk_alarm_init()");
            } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "SAMPLE_COMM_SYS_GetPicSize(LOCALSDK_VIDEO_RESOLUTION_640x360)");
        } else LOGGER(LOGGER_LEVEL_ERROR, "%s error!", "inner_change_resulu_type(LOCALSDK_VIDEO_RESOLUTION_640x360)");
    } else LOGGER(LOGGER_LEVEL_INFO, "Alarm init skipped, because alarms are disabled");
    
    // Free alarm if error occurred
    if(!result) {
        if(alarm_free()) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "alarm_free()");
        else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "alarm_free()");
    }
    
    LOGGER(LOGGER_LEVEL_DEBUG, "Function completed (result = %s).", (result ? "true" : "false"));
    return result;
}

// Free alarm
bool alarm_free() {
    LOGGER(LOGGER_LEVEL_DEBUG, "Function is called...");
    bool result = true;
    
    // Disable alarm
    if(result &= alarm_switch(false)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "alarm_switch(false)");
    else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "alarm_switch(false)");
    
    // Stop timeout thread
    if(timeout_thread) {
        if(result &= (pthread_cancel(timeout_thread) == 0)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "pthread_cancel(timeout_thread)");
        else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "pthread_cancel(timeout_thread)");
    }
    
    // Clear alarm state callback
    if(result &= (local_sdk_alarm_state_clear_callback(alarm_state_callback) == LOCALSDK_OK)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_alarm_state_clear_callback()");
    else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "local_sdk_alarm_state_clear_callback()");
    
    // Alarm exit
    if(result &= (local_sdk_alarm_exit() == LOCALSDK_OK)) LOGGER(LOGGER_LEVEL_DEBUG, "%s success.", "local_sdk_alarm_exit()");
    else LOGGER(LOGGER_LEVEL_WARNING, "%s error!", "local_sdk_alarm_exit()");
    
    LOGGER(LOGGER_LEVEL_DEBUG, "Function completed (result = %s).", (result ? "true" : "false"));
    return result;
}
