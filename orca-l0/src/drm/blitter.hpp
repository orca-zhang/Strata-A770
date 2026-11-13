#pragma once
#include "blitter_packets.hpp"
namespace orca::l0::dg2 {
inline auto blitter_flush(uint64_t address=0,uint64_t value=0) {
    auto flush=blitter_packet::Packets::MI_FLUSH_DW::sInit();
    flush.setFlushLlc(1);flush.setFlushCcs(1);
    if(address){flush.setPostSyncOperation(blitter_packet::Packets::MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);flush.setDestinationAddress(address);flush.setImmediateData(value);}
    return flush;
}
}
