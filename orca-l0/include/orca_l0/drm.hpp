#pragma once

// Compatibility names for the original Linux-only interface.
#include "dg2.hpp"

namespace orca::l0::drm {
using dg2::Allocation;
using dg2::Argument;
using dg2::Commands;
using dg2::Queue;
using dg2::Completion;
using dg2::HostGate;
using dg2::Runtime;
} // namespace orca::l0::drm
