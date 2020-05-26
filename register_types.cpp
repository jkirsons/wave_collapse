/* register_types.cpp */

#include "register_types.h"

#include "core/class_db.h"
#include "wave_collapse.h"

void register_wave_collapse_types() {
    ClassDB::register_class<WaveCollapse>();
}

void unregister_wave_collapse_types() {
   // Nothing to do here in this example.
}