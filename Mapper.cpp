/*
 MapperUGen - UGen for using libmapper with SuperCollider server
 Copyright (C) 2020 Mathias Bredholt

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <thread>

#include <mapper/mapper.h>
#include "SC_PlugIn.h"

static InterfaceTable* ft;
static mpr_dev dev;
static std::thread* libmapperThreadHandle;

void libmapperThread() {
  // TODO(mb): Add option for specifying device name with Mapper.enable(name)
  dev = mpr_dev_new("SuperCollider", 0);
  while (!mpr_dev_get_is_ready(dev)) {
    mpr_dev_poll(dev, 10);
  }
  Print("Mapper: libmapper ready!\n");
  while (dev) {
    mpr_dev_poll(dev, 1);
  }
}

struct MapperUnit : public Unit {
  int m_signalNameSize;
  char* m_signalName;
  mpr_sig sig;
  float val = 0;
};

struct MapIn : public MapperUnit {};
struct MapOut : public MapperUnit {};
struct MapperEnabler : public Unit {};
struct MapperDisabler : public Unit {};

// Empty DSP function
static void Unit_next_nop(Unit* unit, int inNumSamples) {}

static void MapIn_Ctor(MapperUnit* unit);
static void MapIn_next(MapperUnit* unit, int inNumSamples);

static void MapOut_Ctor(MapperUnit* unit);
static void MapOut_next(MapperUnit* unit, int inNumSamples);

static void MapperEnabler_Ctor(MapperEnabler* unit);
static void MapperDisabler_Ctor(MapperDisabler* unit);

void signalHandler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int length,
                   mpr_type type, const void* value, mpr_time time) {
  MapperUnit* m = (MapperUnit*)mpr_obj_get_prop_as_ptr(sig, MPR_PROP_DATA, 0);
  if (m) {
    m->val = *reinterpret_cast<const float*>(value);
  }
}

void bindToSignal(MapperUnit* unit, mpr_dir direction, float sigMin,
                  float sigMax) {
  // Search for existing output signal with same name
  mpr_list sigs = mpr_dev_get_sigs(dev, direction);
  mpr_list_filter(sigs, MPR_PROP_NAME, 0, 1, MPR_STR, unit->m_signalName,
                  MPR_OP_EQ);

  if (sigs) {
    // Signal exists, bind unit to signal
    unit->sig = *sigs;
    mpr_obj_set_prop(unit->sig, MPR_PROP_DATA, 0, 1, MPR_PTR, unit, 0);
  } else {
    // Signal doesn't exist, create new
    unit->sig = mpr_sig_new(dev, direction, unit->m_signalName, 1, MPR_FLT, 0,
                            &sigMin, &sigMax, 0, signalHandler, MPR_SIG_UPDATE);
    mpr_obj_set_prop(unit->sig, MPR_PROP_DATA, 0, 1, MPR_PTR, unit, 0);
  }
}

// MapIn

void MapIn_Ctor(MapperUnit* unit) {
  float sigMin = IN0(0);
  float sigMax = IN0(1);

  unit->m_signalNameSize = IN0(2);
  const int signalNameAllocSize = (unit->m_signalNameSize + 3) * sizeof(char);

  char* chunk =
      reinterpret_cast<char*>(RTAlloc(unit->mWorld, signalNameAllocSize));

  if (!chunk) {
    Print("MapIn: RT memory allocation failed\n");
    SETCALC(Unit_next_nop);
    return;
  }

  unit->m_signalName = chunk;
  for (int i = 0; i < unit->m_signalNameSize; i++) {
    unit->m_signalName[i] = static_cast<char>(IN0(3 + i));
  }
  unit->m_signalName[unit->m_signalNameSize] = 0;

  if (dev) {
    bindToSignal(unit, MPR_DIR_IN, sigMin, sigMax);
  } else {
    Print("MapIn: libmapper not enabled\n");
    SETCALC(Unit_next_nop);
    return;
  }

  RTFree(unit->mWorld, unit->m_signalName);

  SETCALC(MapIn_next);
  MapIn_next(unit, 1);
}

void MapIn_next(MapperUnit* unit, int inNumSamples) {
  float* out = OUT(0);
  *out = unit->val;
}

// MapOut

void MapOut_Ctor(MapperUnit* unit) {
  float sigMin = IN0(1);
  float sigMax = IN0(2);

  unit->m_signalNameSize = IN0(3);
  const int signalNameAllocSize = (unit->m_signalNameSize + 4) * sizeof(char);

  char* chunk =
      reinterpret_cast<char*>(RTAlloc(unit->mWorld, signalNameAllocSize));

  if (!chunk) {
    Print("MapOut: RT memory allocation failed\n");
    SETCALC(Unit_next_nop);
    return;
  }

  unit->m_signalName = chunk;
  for (int i = 0; i < unit->m_signalNameSize; i++) {
    unit->m_signalName[i] = static_cast<char>(IN0(4 + i));
  }
  unit->m_signalName[unit->m_signalNameSize] = 0;

  if (dev) {
    bindToSignal(unit, MPR_DIR_OUT, sigMin, sigMax);
  } else {
    Print("MapOut: libmapper not enabled\n");
    SETCALC(Unit_next_nop);
    return;
  }

  RTFree(unit->mWorld, unit->m_signalName);

  SETCALC(MapOut_next);
  MapOut_next(unit, 1);
}

void MapOut_next(MapperUnit* unit, int inNumSamples) {
  float val = IN0(0);
  mpr_sig_set_value(unit->sig, 0, 1, MPR_FLT, &val);
}

// MapperEnabler

void MapperEnabler_Ctor(MapperEnabler* unit) {
  if (!dev) {
    libmapperThreadHandle = new std::thread(libmapperThread);
  } else {
    Print("Mapper: libmapper already enabled.\n");
  }
  SETCALC(Unit_next_nop);
}

// MapperDisabler

void MapperDisabler_Ctor(MapperDisabler* unit) {
  if (dev) {
    mpr_dev_free(dev);
    dev = nullptr;
    libmapperThreadHandle->join();
  }
  SETCALC(Unit_next_nop);
}

PluginLoad(MapperUGens) {
  ft = inTable;
  DefineSimpleUnit(MapIn);
  DefineSimpleUnit(MapOut);
  DefineSimpleUnit(MapperEnabler);
  DefineSimpleUnit(MapperDisabler);
}
