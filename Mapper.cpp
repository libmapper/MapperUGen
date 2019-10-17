#include <cstdio>
#include <thread>
#include <mapper/mapper_cpp.h>
#include "SC_PlugIn.h"

static mapper::Device* dev = nullptr;
static std::thread* poll_thread = nullptr;

// InterfaceTable contains pointers to functions in the host (server).
static InterfaceTable *ft;

void poll_thread_handler() {
    dev = new mapper::Device("SuperCollider");
    while (dev) {
        dev->poll(10);
    }
}

// Mapper input

struct MapperInput : public Unit {
    int m_signalNameSize;
    char* m_signalName;
    mapper::Signal sig;
    float val = 0;
};

void signal_handler(mapper_signal sig, mapper_id instance, const void *value,
                    int count, mapper_timetag_t *timetag) {
    MapperInput* m = (MapperInput*) mapper_signal_user_data(sig);
    m->val = *((float*) value);
}

static void MapperInput_next(MapperInput* unit, int inNumSamples);
static void MapperInput_Ctor(MapperInput* unit);
static void MapperInput_Dtor(MapperInput* unit);

static void Unit_next_nop(MapperInput* unit, int inNumSamples) {}

void MapperInput_Ctor(MapperInput* unit) {
    float sigMin = IN0(0);
    float sigMax = IN0(1);

    unit->m_signalNameSize = IN0(2);
    const int signalNameAllocSize = (unit->m_signalNameSize + 3) * sizeof(char);

    char* chunk = (char*) RTAlloc(unit->mWorld, signalNameAllocSize);

    if (!chunk) {
        Print("MapperInput: RT memory allocation failed\n");
        SETCALC(Unit_next_nop);
        return;
    }

    unit->m_signalName = chunk;
    for (int i = 0; i < unit->m_signalNameSize; i++) {
        unit->m_signalName[i] = (char) IN0(3 + i);
    }
    unit->m_signalName[unit->m_signalNameSize] = 0;

    if (dev) {
        unit->sig = dev->add_input_signal(unit->m_signalName, 1, 'f', 0, &sigMin, &sigMax, signal_handler, unit);
    } else {
        Print("MapperInput: libmapper not enabled\n");
        SETCALC(Unit_next_nop);
        return;
    }
    SETCALC(MapperInput_next);
    MapperInput_next(unit, 1);
}

void MapperInput_Dtor(MapperInput* unit) { 
    RTFree(unit->mWorld, unit->m_signalName);
    dev->remove_signal(unit->sig);
}

void MapperInput_next(MapperInput* unit, int inNumSamples) {
    float *out = OUT(0);
    *out = unit->val;
}

// Mapper output

struct MapperOutput : public Unit {
    int m_signalNameSize;
    char* m_signalName;
    mapper::Signal sig;
    float val = 0;
};

static void MapperOutput_next(MapperOutput* unit, int inNumSamples);
static void MapperOutput_Ctor(MapperOutput* unit);
static void MapperOutput_Dtor(MapperOutput* unit);

void MapperOutput_Ctor(MapperOutput* unit) {
    float sigMin = IN0(1);
    float sigMax = IN0(2);

    unit->m_signalNameSize = IN0(3);
    const int signalNameAllocSize = (unit->m_signalNameSize + 4) * sizeof(char);

    char* chunk = (char*) RTAlloc(unit->mWorld, signalNameAllocSize);

    if (!chunk) {
        Print("MapperOutput: RT memory allocation failed\n");
        SETCALC(Unit_next_nop);
        return;
    }

    unit->m_signalName = chunk;
    for (int i = 0; i < unit->m_signalNameSize; i++) {
        unit->m_signalName[i] = (char) IN0(4 + i);
    }
    unit->m_signalName[unit->m_signalNameSize] = 0;

    if (dev) {
        unit->sig = dev->add_output_signal(unit->m_signalName, 1, 'f', 0, &sigMin, &sigMax);
    } else {
        Print("MapperOutput: libmapper not enabled\n");
        SETCALC(Unit_next_nop);
        return;
    }
    SETCALC(MapperOutput_next);
    MapperOutput_next(unit, 1);
}

void MapperOutput_Dtor(MapperOutput* unit) {
    RTFree(unit->mWorld, unit->m_signalName);
    dev->remove_signal(unit->sig);
}

void MapperOutput_next(MapperOutput* unit, int inNumSamples) {
    float val = IN0(0);
    unit->sig.update(val);
}

// Mapper handler

struct Mapper : public Unit {};

static void Mapper_next(Mapper* unit, int inNumSamples) {}
static void Mapper_Ctor(Mapper* unit) {}

// Mapper enabler

struct MapperEnabler : public Unit {};

static void MapperEnabler_next(MapperEnabler* unit, int inNumSamples);
static void MapperEnabler_Ctor(MapperEnabler* unit);
static void MapperEnabler_Dtor(MapperEnabler* unit);

void MapperEnabler_Ctor(MapperEnabler* unit) {
    if (!dev) {
        poll_thread = new std::thread(poll_thread_handler);
    } else {
        Print("MapperEnabler: libmapper already enabled.");
    }
    SETCALC(MapperEnabler_next);
}

void MapperEnabler_next(MapperEnabler* unit, int inNumSamples) {}

// Mapper disabler
// For deleting the mapper device

struct MapperDisabler : public Unit {};

static void MapperDisabler_next(MapperDisabler* unit, int inNumSamples);
static void MapperDisabler_Ctor(MapperDisabler* unit);
static void MapperDisabler_Dtor(MapperDisabler* unit);

void MapperDisabler_Ctor(MapperDisabler* unit) {
    if (dev) {
        delete dev;
        dev = nullptr;
        poll_thread->join();
    }
    SETCALC(MapperDisabler_next);
}

void MapperDisabler_next(MapperDisabler* unit, int inNumSamples) {}

PluginLoad(MapperUGens) {
    ft = inTable;
    DefineSimpleUnit(Mapper);
    DefineSimpleUnit(MapperEnabler);
    DefineSimpleUnit(MapperDisabler);
    DefineDtorUnit(MapperInput);
    DefineDtorUnit(MapperOutput);
}
