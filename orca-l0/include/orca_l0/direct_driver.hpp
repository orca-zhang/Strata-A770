#pragma once

namespace orca::l0 {
// Immutable diagnostics after zeInit; no handle translation or lookup on submission.
const char* direct_driver_path() noexcept;
const char* direct_driver_error() noexcept;
}
