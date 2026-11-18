/*
 *   GamePad (DRC) firmware flashing, from DRXUtil (github.com/GaryOderNichts/DRXUtil, source/screens/FlashScreen.cpp
 *   and MainScreen.cpp). Copyright (C) 2024-2025 GaryOderNichts; changes for wup-homebrew.
 *
 *   This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
 *   Public License as published by the Free Software Foundation, either version 2 of the License, or (at your
 *   option) any later version. See LICENSE.
 */
#include "drc.hpp"

#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nsysccr/cdc.h>
#include <nsysccr/cfg.h>
#include <nn/ccr.h>
#include <mocha/mocha.h>

namespace {

int stage;                 // how far Init got (what Shutdown undoes)
volatile bool done;
volatile int32_t result;

void Completed(IOSError error, void*) { result = error; done = true; }

bool WaitForEeprom(uint32_t drcSlot)
{
    uint8_t val;
    OSTime startTime = OSGetSystemTime();
    while (CCRCFGGetCachedEeprom(drcSlot, 0, &val, sizeof(val)) == -1) {
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 2) return false;
        OSSleepTicks(OSMillisecondsToTicks(200));
    }
    return true;
}

bool ReattachDRC(CCRCDCDestination dest, CCRCDCDrcStateEnum targetState, BOOL unknown)
{
    CCRCDCDrcState state;
    if (CCRCDCSysGetDrcState(dest, &state) != 0) return false;
    if (state.state == CCR_CDC_DRC_STATE_STANDALONE) state.state = CCR_CDC_DRC_STATE_ACTIVE;
    if (state.state == targetState) return true;
    __CCRSysInitReattach(dest - CCR_CDC_DESTINATION_DRC0);
    state.state = targetState;
    if (CCRCDCSysSetDrcState(dest, &state) != 0) return false;
    if (__CCRSysWaitReattach(dest - CCR_CDC_DESTINATION_DRC0, unknown) != 0) return false;
    if (!WaitForEeprom(dest - CCR_CDC_DESTINATION_DRC0)) return false;
    if (CCRCDCSysGetDrcState(dest, &state) != 0) return false;
    return state.state == targetState;
}

bool AbortUpdate(CCRCDCDestination dest)
{
    OSTime startTime = OSGetSystemTime();
    while (CCRCDCSoftwareAbort(dest) != 0) {
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 3) return false;
        OSSleepTicks(OSMillisecondsToTicks(200));
    }
    return true;
}

bool CaffeineInvalidate()
{
    CCRCDCSoftwareVersion version;
    CCRCDCSoftwareGetVersion(CCR_CDC_DESTINATION_DRC0, &version);
    if (version.runningVersion >= 0x180a0000) return CCRSysCaffeineSetCaffeineSlot(0xff) == 0;   // (only newer versions have it)
    return true;
}

void Fail(std::string& err, const char* why)
{
    AbortUpdate(CCR_CDC_DESTINATION_DRC0);
    ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
    err = why;
}

}

namespace Drc {

bool Init(std::string& err)
{
    CCRSysInit();
    stage = 1;
    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) { err = "Needs Aroma or Tiramisu"; return false; }
    stage = 2;
    if (Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01") != MOCHA_RESULT_SUCCESS) { err = "The system memory could not be opened"; return false; }
    stage = 3;
    // IOS-PAD's firmware version check: off (DRXUtil does the same; the version this installs is the console's own)
    uint32_t op;
    if (Mocha_IOSUKernelRead32(0x11f53cf4, &op) != MOCHA_RESULT_SUCCESS) { err = "Unsupported system version"; return false; }
    if (op == 0xeb00bd82) {
        if (Mocha_IOSUKernelWrite32(0x11f53cf4, 0xe3a00001) != MOCHA_RESULT_SUCCESS) { err = "Unsupported system version"; return false; }
    } else if (op != 0xe3a00001) {
        err = "Unsupported system version";
        return false;
    }
    return true;
}

void Shutdown()
{
    if (stage >= 3) Mocha_UnmountFS("storage_mlc01");
    if (stage >= 2) Mocha_DeInitLibrary();
    if (stage >= 1) CCRSysExit();
    stage = 0;
}

bool FirmwarePath(std::string& path)
{
    int32_t handle = MCP_Open();
    if (handle < 0) return false;
    alignas(0x40) MCPTitleListType title;
    uint32_t titleCount = 0;
    MCPError error = MCP_TitleListByAppType(handle, MCP_APP_TYPE_DRC_FIRMWARE, &titleCount, &title, sizeof(title));
    MCP_Close(handle);
    if (error != 0 || titleCount != 1) return false;
    path = std::string(&title.path[0]) + "/content/drc_fw.bin";
    return true;
}

bool Start(const std::string& path, std::string& err)
{
    if (!CaffeineInvalidate()) { err = "The GamePad could not be prepared"; return false; }
    CCRCDCSoftwareAbort(CCR_CDC_DESTINATION_DRC0);                 // (anything pending)
    if (!ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_FWUPDATE, FALSE)) {
        ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE);
        err = "The GamePad is not connected";
        return false;
    }
    done = false;
    result = 0;
    if (CCRCDCSoftwareUpdate(CCR_CDC_DESTINATION_DRC0, path.c_str(), Completed, nullptr) != 0) {
        Fail(err, "The update could not start");
        return false;
    }
    return true;
}

bool Poll(int& progress)
{
    CCRCDCFWInfo fwInfo{};
    if (CCRCDCGetFWInfo(CCR_CDC_DESTINATION_DRC0, &fwInfo) == 0) progress = fwInfo.updateProgress;
    return !done;
}

bool Finish(std::string& err)
{
    if (result != IOS_ERROR_OK) { Fail(err, "The GamePad did not take the update"); return false; }
    // the new firmware's bank becomes the one the GamePad boots
    if (CCRCDCSoftwareActivate(CCR_CDC_DESTINATION_DRC0) != 0) { Fail(err, "The update could not be activated"); return false; }
    OSTime startTime = OSGetSystemTime();
    while (!ReattachDRC(CCR_CDC_DESTINATION_DRC0, CCR_CDC_DRC_STATE_ACTIVE, FALSE)) {
        if (OSTicksToSeconds(OSGetSystemTime() - startTime) > 10) break;   // (it is installed either way)
        OSSleepTicks(OSMillisecondsToTicks(1000));
    }
    return true;
}

}
