/* / mcu / sound.h
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/firmware.h>
#include <eez/system.h>

#include <eez/modules/psu/psu.h>

#include <stdio.h>

#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/ntp.h>
#include <eez/mqtt.h>
#endif
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/profile.h>
#include <eez/sound.h>
#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#endif
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/ontime.h>

#include <eez/modules/mcu/battery.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_systemCapabilityQ(scpi_t *context) {
    char text[sizeof(STR_SYST_CAP)];
    strcpy(text, STR_SYST_CAP);
    SCPI_ResultText(context, text);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemErrorNextQ(scpi_t *context) {
    return SCPI_SystemErrorNextQ(context);
}

scpi_result_t scpi_cmd_systemErrorCountQ(scpi_t *context) {
    return SCPI_SystemErrorCountQ(context);
}

scpi_result_t scpi_cmd_systemVersionQ(scpi_t *context) {
    return SCPI_SystemVersionQ(context);
}

scpi_result_t scpi_cmd_systemPower(scpi_t *context) {
    bool up;
    if (!SCPI_ParamBool(context, &up, TRUE)) {
        return SCPI_RES_ERR;
    }

#if OPTION_AUX_TEMP_SENSOR
    if (temperature::sensors[temp_sensor::AUX].isTripped()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_EXECUTE_BEFORE_CLEARING_PROTECTION);
        return SCPI_RES_ERR;
    }
#endif

    changePowerState(up);

    osDelay(1000);

    if (up != isPowerUp()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerQ(scpi_t *context) {
    SCPI_ResultBool(context, isPowerUp());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDate(scpi_t *context) {
    int32_t year;
    if (!SCPI_ParamInt(context, &year, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t month;
    if (!SCPI_ParamInt(context, &month, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t day;
    if (!SCPI_ParamInt(context, &day, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (year < 2000 || year > 2099) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }
    year = year - 2000;

    if (!datetime::isValidDate((uint8_t)year, (uint8_t)month, (uint8_t)day)) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    if (!datetime::setDate((uint8_t)year, (uint8_t)month, (uint8_t)day, 2)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDateClear(scpi_t *context) {
    persist_conf::setDateValid(0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDateQ(scpi_t *context) {
    uint8_t year, month, day;
    if (!datetime::getDate(year, month, day)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[16] = { 0 };
    sprintf(buffer, "%d, %d, %d", (int)(year + 2000), (int)month, (int)day);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTime(scpi_t *context) {
    int32_t hour;
    if (!SCPI_ParamInt(context, &hour, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t minute;
    if (!SCPI_ParamInt(context, &minute, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t second;
    if (!SCPI_ParamInt(context, &second, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (!datetime::isValidTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second)) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    if (!datetime::setTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second, 2)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeClear(scpi_t *context) {
    persist_conf::setTimeValid(0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeQ(scpi_t *context) {
    uint8_t hour, minute, second;
    if (!datetime::getTime(hour, minute, second)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[16] = { 0 };
    sprintf(buffer, "%d, %d, %d", (int)hour, (int)minute, (int)second);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

// NONE|ODD|EVEN
static scpi_choice_def_t dstChoice[] = { { "OFF", datetime::DST_RULE_OFF },
                                         { "EU", datetime::DST_RULE_EUROPE },
                                         { "USA", datetime::DST_RULE_USA },
                                         { "AUS", datetime::DST_RULE_AUSTRALIA },
                                         SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemTimeDst(scpi_t *context) {
    int32_t dstRule;
    if (!SCPI_ParamChoice(context, dstChoice, &dstRule, true)) {
        return SCPI_RES_ERR;
    }

    persist_conf::setDstRule((uint8_t)dstRule);

#if OPTION_ETHERNET
    ntp::reset();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeDstQ(scpi_t *context) {
    resultChoiceName(context, dstChoice, persist_conf::devConf.dstRule);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeZone(scpi_t *context) {
    const char *timeZoneStr;
    size_t timeZoneStrLength;

    if (!SCPI_ParamCharacters(context, &timeZoneStr, &timeZoneStrLength, true)) {
        return SCPI_RES_ERR;
    }

    int16_t timeZone;
    if (!parseTimeZone(timeZoneStr, timeZoneStrLength, timeZone)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setTimeZone(timeZone);

#if OPTION_ETHERNET
    ntp::reset();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeZoneQ(scpi_t *context) {
    char timeZoneStr[32];
    formatTimeZone(persist_conf::devConf.timeZone, timeZoneStr, 32);
    SCPI_ResultText(context, timeZoneStr);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperImmediate(scpi_t *context) {
    sound::playBeep(true);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperState(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (enable != persist_conf::isSoundEnabled()) {
        persist_conf::enableSound(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperStateQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isSoundEnabled());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperKeyState(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (enable != persist_conf::isClickSoundEnabled()) {
        persist_conf::enableClickSound(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperKeyStateQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isClickSoundEnabled());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighClear(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::clearOtpProtection(sensor);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighLevel(scpi_t *context) {
    float level;
    if (!get_temperature_param(context, level, OTP_AUX_MIN_LEVEL, OTP_AUX_MAX_LEVEL,
                               OTP_AUX_DEFAULT_LEVEL)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpLevel(sensor, level);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighLevelQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    return result_float(context, 0, temperature::sensors[sensor].prot_conf.level, UNIT_CELSIUS);
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighState(scpi_t *context) {
    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpState(sensor, state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighStateQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, temperature::sensors[sensor].prot_conf.state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighDelayTime(scpi_t *context) {
    float delay;
    if (!get_duration_param(context, delay, OTP_AUX_MIN_DELAY, OTP_AUX_MAX_DELAY,
                            OTP_AUX_DEFAULT_DELAY)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpDelay(sensor, delay);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighDelayTimeQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, temperature::sensors[sensor].prot_conf.delay);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighTrippedQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, temperature::sensors[sensor].isTripped());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelCountQ(scpi_t *context) {
    SCPI_ResultInt(context, CH_NUM);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationCurrentQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel_dispatcher::getIMax(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationPowerQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel->params.PTOT);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationProgramQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint16_t features = channel->params.features;

    char strFeatures[64] = { 0 };

    if (features & CH_FEATURE_VOLT) {
        strcat(strFeatures, "Volt");
    }

    if (features & CH_FEATURE_CURRENT) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "Current");
    }

    if (features & CH_FEATURE_POWER) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "Power");
    }

    if (features & CH_FEATURE_OE) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "OE");
    }

    if (features & CH_FEATURE_DPROG) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "DProg");
    }

    if (features & CH_FEATURE_RPROG) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "Rprog");
    }

    if (features & CH_FEATURE_RPOL) {
        if (strFeatures[0]) {
            strcat(strFeatures, ", ");
        }
        strcat(strFeatures, "RPol");
    }

    SCPI_ResultText(context, strFeatures);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationVoltageQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel_dispatcher::getUMax(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationOntimeTotalQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    outputOnTime(context, ontime::g_moduleCounters[channel->slotIndex].getTotalTime());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationOntimeLastQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    outputOnTime(context, ontime::g_moduleCounters[channel->slotIndex].getLastTime());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelModelQ(scpi_t *context) {
    Channel *channel = param_channel(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->isInstalled()) {
        auto &slot = g_slots[channel->slotIndex];
        char text[100];
        sprintf(text, "%s_R%dB%d", slot.moduleInfo->moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
        SCPI_ResultText(context, text);
    } else {
        SCPI_ResultText(context, "None");
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuInformationTypeQ(scpi_t *context) {
    SCPI_ResultText(context, getCpuType());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuInformationOntimeTotalQ(scpi_t *context) {
    outputOnTime(context, ontime::g_mcuCounter.getTotalTime());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuInformationOntimeLastQ(scpi_t *context) {
    outputOnTime(context, ontime::g_mcuCounter.getLastTime());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuModelQ(scpi_t *context) {
    SCPI_ResultText(context, getCpuModel());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuOptionQ(scpi_t *context) {
    char strFeatures[128] = { 0 };

#if OPTION_BP
    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "BPost");
#endif

#if OPTION_EXT_EEPROM
    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "EEPROM");
#endif

#if OPTION_EXT_RTC
    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "RTC");
#endif

    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "SDcard");

#if OPTION_ETHERNET
    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "Ethernet");
#endif

#if OPTION_DISPLAY
    if (strFeatures[0]) {
        strcat(strFeatures, ", ");
    }
    strcat(strFeatures, "Display");
#endif

    SCPI_ResultText(context, strFeatures);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSerialQ(scpi_t *context) {
    SCPI_ResultText(context, getSerialNumber());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerProtectionTrip(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableShutdownWhenProtectionTripped(enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerProtectionTripQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isShutdownWhenProtectionTrippedEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPonOutputDisable(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableForceDisablingAllOutputsOnPowerUp(enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPonOutputDisableQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordNew(scpi_t *context) {
    if (!checkPassword(context, persist_conf::devConf.systemPassword)) {
        return SCPI_RES_ERR;
    }

    const char *new_password;
    size_t new_password_len;

    if (!SCPI_ParamCharacters(context, &new_password, &new_password_len, true)) {
        return SCPI_RES_ERR;
    }

    int16_t err;
    if (!persist_conf::isSystemPasswordValid(new_password, new_password_len, err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    persist_conf::changeSystemPassword(new_password, new_password_len);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordFpanelReset(scpi_t *context) {
    persist_conf::changeSystemPassword("", 0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordCalibrationReset(scpi_t *context) {
    persist_conf::changeCalibrationPassword(CALIBRATION_PASSWORD_DEFAULT, strlen(CALIBRATION_PASSWORD_DEFAULT));
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemKlock(scpi_t *context) {
    persist_conf::lockFrontPanel(true);

#if OPTION_DISPLAY
    refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_choice_def_t rlStateChoice[] = {
    { "LOCal", RL_STATE_LOCAL },
    { "REMote", RL_STATE_REMOTE },
    { "RWLock", RL_STATE_RW_LOCK },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_systemCommunicateRlstate(scpi_t *context) {
    int32_t rlState;
    if (!SCPI_ParamChoice(context, rlStateChoice, &rlState, true)) {
        return SCPI_RES_ERR;
    }

    g_rlState = (RLState)rlState;

#if OPTION_DISPLAY
    refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateRlstateQ(scpi_t *context) {
    resultChoiceName(context, rlStateChoice, g_rlState);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemLocal(scpi_t *context) {
    g_rlState = RL_STATE_LOCAL;

#if OPTION_DISPLAY
    refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRemote(scpi_t *context) {
    g_rlState = RL_STATE_REMOTE;

#if OPTION_DISPLAY
    refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRwlock(scpi_t *context) {
    g_rlState = RL_STATE_RW_LOCK;

#if OPTION_DISPLAY
    refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateSerialBaud(scpi_t *context) {
    int32_t baud;
    if (!SCPI_ParamInt(context, &baud, TRUE)) {
        return SCPI_RES_ERR;
    }

    int baudIndex = persist_conf::getIndexFromBaud(baud);
    if (baudIndex == 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    if (!persist_conf::setSerialBaudIndex(baudIndex)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateSerialBaudQ(scpi_t *context) {
    SCPI_ResultInt(context, persist_conf::getBaudFromIndex(persist_conf::getSerialBaudIndex()));
    return SCPI_RES_OK;
}

// NONE|ODD|EVEN
static scpi_choice_def_t parityChoice[] = {
    { "NONE", serial::PARITY_NONE },   { "EVEN", serial::PARITY_EVEN },
    { "ODD", serial::PARITY_ODD },     { "MARK", serial::PARITY_MARK },
    { "SPACE", serial::PARITY_SPACE }, SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateSerialParity(scpi_t *context) {
    int32_t parity;
    if (!SCPI_ParamChoice(context, parityChoice, &parity, true)) {
        return SCPI_RES_ERR;
    }

    if (!persist_conf::setSerialParity(parity)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateSerialParityQ(scpi_t *context) {
    resultChoiceName(context, parityChoice, persist_conf::getSerialParity());
    return SCPI_RES_OK;
}

// NONE|ODD|EVEN
static scpi_choice_def_t commInterfaceChoice[] = {
    { "SERial", 1 }, 
    { "ETHernet", 2 }, 
    { "NTP", 3 }, 
    { "MQTT", 4 }, 
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateEnable(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t commInterface;
    if (!SCPI_ParamChoice(context, commInterfaceChoice, &commInterface, true)) {
        return SCPI_RES_ERR;
    }

    if (commInterface == 1) {
        persist_conf::enableSerial(enable);
    } else if (commInterface == 2) {
#if OPTION_ETHERNET
        persist_conf::enableEthernet(enable);
#else
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
#endif
    } else if (commInterface == 3) {
        persist_conf::enableNtp(enable);
    } else if (commInterface == 4) {
        persist_conf::enableMqtt(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateEnableQ(scpi_t *context) {
    int32_t commInterface;
    if (!SCPI_ParamChoice(context, commInterfaceChoice, &commInterface, true)) {
        return SCPI_RES_ERR;
    }

    if (commInterface == 1) {
        SCPI_ResultBool(context, persist_conf::isSerialEnabled());
    } else if (commInterface == 2) {
#if OPTION_ETHERNET
        SCPI_ResultBool(context, persist_conf::isEthernetEnabled());
#else
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
#endif
    } else if (commInterface == 3) {
        SCPI_ResultBool(context, persist_conf::isNtpEnabled());
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDhcp(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableEthernetDhcp(enable);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDhcpQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, persist_conf::isEthernetDhcpEnabled());
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetAddress(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetIpAddress(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetAddressQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char ipAddressStr[16];
    if (persist_conf::devConf.ethernetDhcpEnabled) {
        ipAddressToString(ethernet::getIpAddress(), ipAddressStr);
    } else {
        ipAddressToString(persist_conf::devConf.ethernetIpAddress, ipAddressStr);
    }
    SCPI_ResultText(context, ipAddressStr);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDns(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetDns(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDnsQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetDns, ipAddressStr);
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetGateway(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetGateway(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetGatewayQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetGateway, ipAddressStr);
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetHostname(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *hostNameStr;
    size_t hostNameStrLength;

    if (!SCPI_ParamCharacters(context, &hostNameStr, &hostNameStrLength, true)) {
        return SCPI_RES_ERR;
    }

    if (hostNameStrLength > 32) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
        return SCPI_RES_ERR;
    }

    char hostName[32 + 1];
    strncpy(hostName, hostNameStr, hostNameStrLength);
    hostName[hostNameStrLength] = 1;

    persist_conf::setEthernetHostName(hostName);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetHostnameQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    SCPI_ResultText(context, persist_conf::devConf.ethernetHostName);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetSmask(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetSubnetMask(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetSmaskQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetSubnetMask, ipAddressStr);
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetPort(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    int32_t port;
    if (!SCPI_ParamInt(context, &port, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (port < 0 && port > 65535) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetScpiPort((uint16_t)port);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetPortQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    SCPI_ResultInt(context, persist_conf::devConf.ethernetScpiPort);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetMac(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *macAddressStr;
    size_t macAddressStrLength;

    if (!SCPI_ParamCharacters(context, &macAddressStr, &macAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint8_t macAddress[6];
    if (!parseMacAddress(macAddressStr, macAddressStrLength, macAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetMacAddress(macAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetMacQ(scpi_t *context) {
#if OPTION_ETHERNET
    char macAddressStr[18];
    macAddressToString(persist_conf::devConf.ethernetMacAddress, macAddressStr);
    SCPI_ResultText(context, macAddressStr);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtp(scpi_t *context) {
#if OPTION_ETHERNET
    const char *ntpServer;
    size_t ntpServerLength;

    if (!SCPI_ParamCharacters(context, &ntpServer, &ntpServerLength, true)) {
        return SCPI_RES_ERR;
    }

    if (ntpServerLength > 32) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
        return SCPI_RES_ERR;
    }

    persist_conf::setNtpServer(ntpServer, ntpServerLength);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtpQ(scpi_t *context) {
#if OPTION_ETHERNET
    SCPI_ResultText(context, persist_conf::devConf.ntpServer);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemInhibitQ(scpi_t *context) {
    SCPI_ResultBool(context, io_pins::isInhibited());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalInputDataQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 1 && pin != 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (persist_conf::devConf.ioPins[pin].function != io_pins::FUNCTION_INPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultInt(context, io_pins::getPinState(pin) ? 1 : 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputData(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 3 && pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (persist_conf::devConf.ioPins[pin].function != io_pins::FUNCTION_OUTPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    io_pins::setPinState(pin, state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputDataQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 3 && pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (persist_conf::devConf.ioPins[pin].function != io_pins::FUNCTION_OUTPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, io_pins::getPinState(pin));

    return SCPI_RES_OK;
}

static scpi_choice_def_t functionChoice[] = { { "NONE", io_pins::FUNCTION_NONE },
                                              { "DINPut", io_pins::FUNCTION_INPUT },
                                              { "DOUTput", io_pins::FUNCTION_OUTPUT },
                                              { "FAULt", io_pins::FUNCTION_FAULT },
                                              { "INHibit", io_pins::FUNCTION_INHIBIT },
                                              { "ONCouple", io_pins::FUNCTION_ON_COUPLE },
                                              { "TINPut", io_pins::FUNCTION_TINPUT },
                                              { "TOUTput", io_pins::FUNCTION_TOUTPUT },
                                              SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemDigitalPinFunction(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int32_t function;
    if (!SCPI_ParamChoice(context, functionChoice, &function, true)) {
        return SCPI_RES_ERR;
    }

    pin--;

    if (pin < 2) {
        if (function != io_pins::FUNCTION_NONE && function != io_pins::FUNCTION_INPUT &&
            function != io_pins::FUNCTION_INHIBIT && function != io_pins::FUNCTION_TINPUT) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (function != io_pins::FUNCTION_NONE && function != io_pins::FUNCTION_OUTPUT &&
            function != io_pins::FUNCTION_FAULT && function != io_pins::FUNCTION_ON_COUPLE &&
            function != io_pins::FUNCTION_TOUTPUT) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }

    persist_conf::setIoPinFunction(pin, function);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalPinFunctionQ(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    resultChoiceName(context, functionChoice, persist_conf::devConf.ioPins[pin].function);

    return SCPI_RES_OK;
}

static scpi_choice_def_t polarityChoice[] = { { "POSitive", io_pins::POLARITY_POSITIVE },
                                              { "NEGative", io_pins::POLARITY_NEGATIVE },
                                              SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemDigitalPinPolarity(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    int32_t polarity;
    if (!SCPI_ParamChoice(context, polarityChoice, &polarity, true)) {
        return SCPI_RES_ERR;
    }

    persist_conf::setIoPinPolarity(pin, polarity);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalPinPolarityQ(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    resultChoiceName(context, polarityChoice, persist_conf::devConf.ioPins[pin].polarity);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemReset(scpi_t *context) {
#if defined(EEZ_PLATFORM_STM32)
	restart();
	return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

static scpi_choice_def_t systemMeasureVoltageChoice[] = { 
    { "VBAT", 1 },
    SCPI_CHOICE_LIST_END 
};

scpi_result_t scpi_cmd_systemMeasureScalarVoltageDcQ(scpi_t *context) {
#if defined(EEZ_PLATFORM_STM32)
    int32_t choice;
    if (!SCPI_ParamChoice(context, systemMeasureVoltageChoice, &choice, true)) {
        return SCPI_RES_ERR;
    }

    // only choice for now is VBAT
    return result_float(context, 0, mcu::battery::g_battery, UNIT_VOLT);
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateMqttSettings(scpi_t *context) {
#if OPTION_ETHERNET
    const char *addr;
    size_t addrLen;
    if (!SCPI_ParamCharacters(context, &addr, &addrLen, true)) {
        return SCPI_RES_ERR;
    }

    int32_t port;
    if (!SCPI_ParamInt(context, &port, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (port < 0 && port > 65535) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    const char *user;
    size_t userLen;
    if (!SCPI_ParamCharacters(context, &user, &userLen, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        user = nullptr;
        userLen = 0;
    }

    const char *pass;
    size_t passLen;
    if (!SCPI_ParamCharacters(context, &pass, &passLen, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        pass = nullptr;
        passLen = 0;
    }

    float period;
    scpi_number_t param;
    if (SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        if (param.special) {
            if (param.content.tag == SCPI_NUM_MIN) {
                period = mqtt::PERIOD_MIN;
            } else if (param.content.tag == SCPI_NUM_MAX) {
                period = mqtt::PERIOD_MAX;
            } else if (param.content.tag == SCPI_NUM_DEF) {
                period = mqtt::PERIOD_DEFAULT;
            } else {
                SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
                return SCPI_RES_ERR;
            }
        } else {
            if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
                SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
                return SCPI_RES_ERR;
            }

            period = (float)param.content.value;
        }
    } else {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        period = 1.0f;
    }

    char savedPastAddrChar = *(addr + addrLen);
    *((char *)addr + addrLen) = 0;

    char savedPastUserChar = 0;
    if (user && userLen) {
        savedPastUserChar = *(user + userLen);
        *((char *)user + userLen) = 0;
    }

    char savedPastPassChar = 0;
    if (pass && passLen) {
        savedPastPassChar = *(pass + passLen);
        *((char *)pass + passLen) = 0;
    }

    persist_conf::setMqttSettings(persist_conf::devConf.mqttEnabled, addr, port, user, pass, period);

    *((char *)addr + addrLen) = savedPastAddrChar;
    if (savedPastUserChar) {
        *((char *)user + userLen) = savedPastUserChar;
    }
    if (savedPastPassChar) {
        *((char *)pass + passLen) = savedPastPassChar;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateMqttStateQ(scpi_t *context) {
#if OPTION_ETHERNET
    SCPI_ResultInt(context, mqtt::g_connectionState);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez
