/*
 MapperUGen - UGen for using libmapper with SuperCollider server
 Copyright (C) 2020 Mathias Bredholt

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA

*/

#include <mapper/mapper.h>

#include <atomic>
#include <functional>
#include <queue>
#include <thread>

#include "SC_PlugIn.h"

static InterfaceTable* ft;
static mpr_dev dev;
static std::atomic<bool> isReady;
static std::thread* libmapperThreadHandle;

void libmapperThread(char* deviceName) {
  dev = mpr_dev_new(deviceName, 0);
  while (!mpr_dev_get_is_ready(dev)) {
    mpr_dev_poll(dev, 10);
  }
  isReady = true;
  Print("Mapper: libmapper ready!\n");
  while (dev) {
    mpr_dev_poll(dev, 1);
  }
}

struct MapperSignalUnit : public Unit {
  int signalNameSize;
  char* signalName;
  mpr_sig sig = nullptr;
  float sigMin = 0;
  float sigMax = 1;
  float val = 0;
};

struct MapperDeviceUnit : public Unit {
  int deviceNameSize;
  char* deviceName;
};

struct MapIn : public MapperSignalUnit {};
struct MapOut : public MapperSignalUnit {};
struct MapperEnabler : public MapperDeviceUnit {};
struct MapperDisabler : public Unit {};

// Empty DSP function
static void Unit_next_nop(Unit* unit, int inNumSamples) {}

static void MapIn_Ctor(MapIn* unit);
static void MapIn_Dtor(MapIn* unit);
static void MapIn_next(MapIn* unit, int inNumSamples);

static void MapOut_Ctor(MapOut* unit);
static void MapOut_Dtor(MapOut* unit);
static void MapOut_next(MapOut* unit, int inNumSamples);

static void MapperEnabler_Ctor(MapperEnabler* unit);
static void MapperDisabler_Ctor(MapperDisabler* unit);

static void MapperSignalUnit_bindToSignal(MapperSignalUnit* unit, mpr_dir direction) {
  // Search for existing output signal with same name
  mpr_list sigs = mpr_dev_get_sigs(dev, direction);
  sigs = mpr_list_filter(sigs, MPR_PROP_NAME, 0, 1, MPR_STR, unit->signalName,
                         MPR_OP_EQ);

  if (sigs) {
    // Signal exists, bind unit to signal
    unit->sig = *sigs;

    // Update signal metadata if signal range has changed
    mpr_obj_set_prop(unit->sig, MPR_PROP_MIN, 0, 1, MPR_FLT, &unit->sigMin, 1);
    mpr_obj_set_prop(unit->sig, MPR_PROP_MAX, 0, 1, MPR_FLT, &unit->sigMax, 1);
    mpr_obj_push(unit->sig);

    // Update maps containing signal
    // mpr_list maps = mpr_sig_get_maps(unit->sig, MPR_DIR_ANY);
    // while (maps) {
    // // char expr[128];
    // if (direction == MPR_DIR_IN) {
    //   mpr_obj_set_prop(*maps, MPR_PROP_EXTRA, "var@dMin", 1, MPR_FLT,
    //                    &unit->sigMin, 1);
    //   mpr_obj_set_prop(*maps, MPR_PROP_EXTRA, "var@dMax", 1, MPR_FLT,
    //                    &unit->sigMax, 1);
    // } else {
    //   mpr_obj_set_prop(*maps, MPR_PROP_EXTRA, "var@sMin", 1, MPR_FLT,
    //                    &unit->sigMin, 1);
    //   mpr_obj_set_prop(*maps, MPR_PROP_EXTRA, "var@sMax", 1, MPR_FLT,
    //                    &unit->sigMax, 1);
    // }
    // mpr_obj_push(*maps);
    // maps = mpr_list_get_next(maps);
    // }
  } else {
    // Signal doesn't exist, create new
    Print("Mapper: Creating signal '%s'\n", unit->signalName);
    unit->sig = mpr_sig_new(dev, direction, unit->signalName, 1, MPR_FLT, 0,
                            &unit->sigMin, &unit->sigMax, 0, 0, 0);
  }
}

// MapIn

void MapIn_Ctor(MapIn* unit) {
  unit->val = 0;
  SETCALC(MapIn_next);

  unit->sigMin = IN0(0);
  unit->sigMax = IN0(1);

  // Set signal name
  unit->signalNameSize = IN0(2);
  const int signalNameAllocSize = (unit->signalNameSize + 1) * sizeof(char);

  void* chunk = RTAlloc(unit->mWorld, signalNameAllocSize);

  if (!chunk) {
    Print("MapIn: RT memory allocation failed\n");
    return;
  }

  unit->signalName = reinterpret_cast<char*>(chunk);
  for (int i = 0; i < unit->signalNameSize; i++) {
    unit->signalName[i] = static_cast<char>(IN0(3 + i));
  }
  unit->signalName[unit->signalNameSize] = 0;

  // Bind to signal
  if (dev) {
    MapperSignalUnit_bindToSignal(unit, MPR_DIR_IN);
  } else {
    Print("MapIn: libmapper not enabled\n");
  }

  MapIn_next(unit, 1);
}

void MapIn_Dtor(MapIn* unit) { RTFree(unit->mWorld, unit->signalName); }

void MapIn_next(MapIn* unit, int inNumSamples) {
  float* out = OUT(0);

  // Signal is not created yet
  if (!unit->sig) {
    *out = 0.f;
  }
  // Get signal value pointer
  const float* val =
      static_cast<const float*>(mpr_sig_get_value(unit->sig, 0, 0));

  // Signal doesn't have a value yet
  if (!val) {
    *out = 0.f;
    return;
  }

  // Set out value to signal value
  *out = *val;
}

// MapOut

void MapOut_Ctor(MapOut* unit) {
  unit->sigMin = IN0(1);
  unit->sigMax = IN0(2);

  // Set signal name
  unit->signalNameSize = IN0(3);
  const int signalNameAllocSize = (unit->signalNameSize + 1) * sizeof(char);

  void* chunk = RTAlloc(unit->mWorld, signalNameAllocSize);

  if (!chunk) {
    Print("MapOut: RT memory allocation failed\n");
    SETCALC(Unit_next_nop);
    return;
  }

  unit->signalName = reinterpret_cast<char*>(chunk);
  for (int i = 0; i < unit->signalNameSize; i++) {
    unit->signalName[i] = static_cast<char>(IN0(4 + i));
  }
  unit->signalName[unit->signalNameSize] = 0;

  if (isReady) {
    MapperSignalUnit_bindToSignal(unit, MPR_DIR_OUT);
  } else {
    Print("MapOut: libmapper not enabled\n");
    SETCALC(Unit_next_nop);
  }

  SETCALC(MapOut_next);
  MapOut_next(unit, 1);
}

void MapOut_Dtor(MapOut* unit) { RTFree(unit->mWorld, unit->signalName); }

void MapOut_next(MapOut* unit, int inNumSamples) {
  // Signal is not ready yet
  if (!unit->sig) {
    return;
  }

  // Set output signal value
  float val = IN0(0);
  mpr_sig_set_value(unit->sig, 0, 1, MPR_FLT, &val);
}

// MapperEnabler

void MapperEnabler_Ctor(MapperEnabler* unit) {
  if (!dev) {
    // Set device name
    unit->deviceNameSize = IN0(0);
    const int deviceNameAllocSize = (unit->deviceNameSize + 1) * sizeof(char);

    void* chunk = RTAlloc(unit->mWorld, deviceNameAllocSize);
    if (!chunk) {
      Print("MapOut: RT memory allocation failed\n");
      SETCALC(Unit_next_nop);
      return;
    }

    unit->deviceName = reinterpret_cast<char*>(chunk);
    for (int i = 0; i < unit->deviceNameSize; i++) {
      unit->deviceName[i] = static_cast<char>(IN0(1 + i));
    }
    unit->deviceName[unit->deviceNameSize] = 0;
    libmapperThreadHandle = new std::thread(libmapperThread, unit->deviceName);
  } else {
    Print("Mapper: libmapper already enabled.\n");
  }
  SETCALC(Unit_next_nop);
}

// MapperDisabler

void MapperDisabler_Ctor(MapperDisabler* unit) {
  if (isReady) {
    mpr_dev_free(dev);
    dev = nullptr;
    libmapperThreadHandle->join();
  }
  SETCALC(Unit_next_nop);
}

PluginLoad(MapperUGens) {
  ft = inTable;
  DefineDtorUnit(MapIn);
  DefineDtorUnit(MapOut);
  DefineSimpleUnit(MapperEnabler);
  DefineSimpleUnit(MapperDisabler);
}
