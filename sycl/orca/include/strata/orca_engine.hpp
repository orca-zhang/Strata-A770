#pragma once
#include <dpct/dpct.hpp>
namespace strata::l0 {
// The executable installs a lazy native bootstrap before its first device query.
void install_engine(const std::filesystem::path& images = {});
Queue* create_copy_queue();
int import_host(void* pointer, size_t bytes);
void release_host(void* pointer);
}
namespace dpct {
inline void sync_barrier(event_ptr event) { sync_barrier(event, &get_in_order_queue()); }
}
