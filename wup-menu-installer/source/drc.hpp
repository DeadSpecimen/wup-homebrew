/*
 *   GamePad (DRC) firmware flashing, from DRXUtil (github.com/GaryOderNichts/DRXUtil, source/screens/FlashScreen.cpp
 *   and MainScreen.cpp). Copyright (C) 2024-2025 GaryOderNichts; changes for wup-homebrew.
 *
 *   This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
 *   Public License as published by the Free Software Foundation, either version 2 of the License, or (at your
 *   option) any later version. See LICENSE.
 */
#pragma once
#include <cstdint>
#include <string>

namespace Drc {

// CCR, Mocha, the MLC mounted at storage_mlc01, IOS-PAD's firmware version check disabled (as DRXUtil does)
bool Init(std::string& err);
void Shutdown();

// the console's own copy of the GamePad firmware: /vol/storage_mlc01/.../content/drc_fw.bin
bool FirmwarePath(std::string& path);

// the firmware at path (/vol/storage_mlc01/...) to the GamePad. Start returns at once; Poll gives the progress
// (0..100) until it returns false, then Result says whether it worked (and it is active).
bool Start(const std::string& path, std::string& err);
bool Poll(int& progress);
bool Finish(std::string& err);

}
