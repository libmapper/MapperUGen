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

#include <boost/lockfree/queue.hpp>
#include <functional>
#include <queue>
#include <thread>

#include "SC_PlugIn.h"

static InterfaceTable* ft;
static mpr_dev dev;
static bool isReady;
static std::thread* libmapperThreadHandle;

typedef std::function<void()> Task;
static boost::lockfree::queue<Task*> taskQueue(128);

void libmapperThread() {
  // TODO(mb): Add option for specifying device name with Mapper.enable(name)
  dev = mpr_dev_new("SuperCollider", 0);
  while (!mpr_dev_get_is_ready(dev)) {
    mpr_dev_poll(dev, 10);
  }
  isReady = true;
  Print("Mapper: libmapper ready!\n");
  while (dev) {
    mpr_dev_poll(dev, 1);
    // Execute tasks
    Task* f;
    if (taskQueue.pop(f)) {
      (*f)();
      delete f;
    }
  }
}

struct MapperUnit : public Unit {
  int signalNameSize;
  char* signalName;
  mpr_sig sig;
  float sigMin = 0;
  float sigMax = 1;
  float val = 0;
};

struct MapIn : public MapperUnit {};
struct MapOut : public MapperUnit {};
struct MapperEnabler : public Unit {};
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

static void MapIn_signalUpdate(mpr_sig sig, mpr_sig_evt evt, mpr_id inst,
                               int length, mpr_type type, const void* value,
                               mpr_time time) {
  MapIn* m = (MapIn*)mpr_obj_get_prop_as_ptr(sig, MPR_PROP_DATA, 0);
  if (m) {
    m->val = *reinterpret_cast<const float*>(value);
  }
}

static void MapperUnit_bindToSignal(MapperUnit* unit, mpr_dir direction) {
  // Search for existing output signal with same name
  mpr_list sigs = mpr_dev_get_sigs(dev, direction);
  sigs = mpr_list_filter(sigs, MPR_PROP_NAME, 0, 1, MPR_STR, unit->signalName,
                         MPR_OP_EQ);

  mpr_sig_handler* handler = direction == MPR_DIR_IN ? MapIn_signalUpdate : 0;
  int flags = direction == MPR_DIR_IN ? MPR_SIG_UPDATE : 0;

  if (sigs) {
    // Signal exists, bind unit to signal
    unit->sig = *sigs;

    // Update pointer for signal update callback
    mpr_obj_set_prop(unit->sig, MPR_PROP_DATA, 0, 1, MPR_PTR, unit, 0);
    mpr_sig_set_cb(unit->sig, handler, flags);

    // Update signal metadata if signal range has changed
    mpr_obj_set_prop(unit->sig, MPR_PROP_MIN, 0, 1, MPR_FLT, &unit->sigMin, 0);
    mpr_obj_set_prop(unit->sig, MPR_PROP_MAX, 0, 1, MPR_FLT, &unit->sigMax, 0);
  } else {
    // Signal doesn't exist, create new
    Print("Creating signal '%s'\n", unit->signalName);
    unit->sig = mpr_sig_new(dev, direction, unit->signalName, 1, MPR_FLT, 0,
                            &unit->sigMin, &unit->sigMax, 0, handler, flags);
    mpr_obj_set_prop(unit->sig, MPR_PROP_DATA, 0, 1, MPR_PTR, unit, 0);
  }
}

// MapIn

void MapIn_Ctor(MapIn* unit) {
  unit->val = 0;
  SETCALC(MapIn_next);

  unit->sigMin = IN0(0);
  unit->sigMax = IN0(1);

  unit->signalNameSize = IN0(2);
  const int signalNameAllocSize = (unit->signalNameSize + 3) * sizeof(char);

  char* chunk =
      reinterpret_cast<char*>(RTAlloc(unit->mWorld, signalNameAllocSize));

  if (!chunk) {
    Print("MapIn: RT memory allocation failed\n");
    return;
  }

  unit->signalName = chunk;
  for (int i = 0; i < unit->signalNameSize; i++) {
    unit->signalName[i] = static_cast<char>(IN0(3 + i));
  }
  unit->signalName[unit->signalNameSize] = 0;

  if (dev) {
    taskQueue.push(
        new Task([=]() { MapperUnit_bindToSignal(unit, MPR_DIR_IN); }));
  } else {
    Print("MapIn: libmapper not enabled\n");
  }

  MapIn_next(unit, 1);
}

void MapIn_Dtor(MapIn* unit) { RTFree(unit->mWorld, unit->signalName); }

void MapIn_next(MapIn* unit, int inNumSamples) {
  float* out = OUT(0);
  *out = unit->val;
}

// MapOut

void MapOut_Ctor(MapOut* unit) {
  unit->sigMin = IN0(1);
  unit->sigMax = IN0(2);

  unit->signalNameSize = IN0(3);
  const int signalNameAllocSize = (unit->signalNameSize + 4) * sizeof(char);

  char* chunk =
      reinterpret_cast<char*>(RTAlloc(unit->mWorld, signalNameAllocSize));

  if (!chunk) {
    Print("MapOut: RT memory allocation failed\n");
    SETCALC(Unit_next_nop);
    return;
  }

  unit->signalName = chunk;
  for (int i = 0; i < unit->signalNameSize; i++) {
    unit->signalName[i] = static_cast<char>(IN0(4 + i));
  }
  unit->signalName[unit->signalNameSize] = 0;

  if (isReady) {
    taskQueue.push(
        new Task([=]() { MapperUnit_bindToSignal(unit, MPR_DIR_IN); }));
  } else {
    Print("MapOut: libmapper not enabled\n");
    SETCALC(Unit_next_nop);
  }

  SETCALC(MapOut_next);
  MapOut_next(unit, 1);
}

void MapOut_Dtor(MapOut* unit) { RTFree(unit->mWorld, unit->signalName); }

void MapOut_next(MapOut* unit, int inNumSamples) {
  float val = IN0(0);
  taskQueue.push(
      new Task([=]() { mpr_sig_set_value(unit->sig, 0, 1, MPR_FLT, &val); }));
}

// MapperEnabler

void MapperEnabler_Ctor(MapperEnabler* unit) {
  if (!dev) {
    // dev = mpr_dev_new("SuperCollider", 0);
    libmapperThreadHandle = new std::thread(libmapperThread);
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
