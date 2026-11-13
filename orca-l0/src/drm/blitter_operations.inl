// Shared State methods; keep the calibrated Windows packet bodies identical.
    void record_color(uint64_t destination,const void* pattern,size_t pixel,size_t pitch,uint32_t width,uint32_t height) {
        using P=blitter_packet::Packets;using C=P::XY_FAST_COLOR_BLT;
        auto blt=C::sInit();
        const auto depth=pixel==1?C::COLOR_DEPTH_8_BIT_COLOR:pixel==2?C::COLOR_DEPTH_16_BIT_COLOR:
            pixel==4?C::COLOR_DEPTH_32_BIT_COLOR:pixel==8?C::COLOR_DEPTH_64_BIT_COLOR:C::COLOR_DEPTH_128_BIT_COLOR;
        std::array<uint32_t,4> color{};std::memcpy(color.data(),pattern,pixel);
        blt.setColorDepth(depth);blt.setFillColor(color.data());blt.setDestinationMOCS(2);
        blt.setDestinationBaseAddress(destination);blt.setDestinationPitch(uint32_t(pitch));
        blt.setDestinationX2CoordinateRight(width);blt.setDestinationY2CoordinateBottom(height);
        blt.setDestinationSurfaceWidth(width);blt.setDestinationSurfaceHeight(height);
        blt.setDestinationSurfaceType(height>1?C::DESTINATION_SURFACE_TYPE_SURFTYPE_2D:C::DESTINATION_SURFACE_TYPE_SURFTYPE_1D);
        detail::emit(recording.commands,blt);
        if(recording.encoder_state.in_order)detail::emit(recording.commands,blitter_flush());
        detail::emit(recording.commands,P::MI_ARB_CHECK::sInit());recording.encoder_state.valid=true;
    }
    void record_color_fill(uint64_t destination,const void* value,size_t pattern_bytes,size_t bytes) {
        const auto pattern=static_cast<const uint8_t*>(value);
        const size_t repeats=bytes/pattern_bytes,tail=bytes%pattern_bytes;
        if(pattern_bytes<=16 && destination%pattern_bytes==0) {
            for(size_t done=0;done<repeats;) {
                const auto width=uint32_t(std::min<size_t>(16384,repeats-done));
                const auto height=uint32_t(std::min<size_t>(16384,(repeats-done)/width));
                record_color(destination+done*pattern_bytes,pattern,pattern_bytes,size_t(width)*pattern_bytes,width,height);
                done+=size_t(width)*height;
            }
        } else {
            // ponytail: strided color stripes preserve wide/unaligned patterns;
            // unaligned fills use byte stripes (at most128 per16K rows).
            const size_t pixel=destination%16==0?16:1;
            for(size_t column=0;column<pattern_bytes;column+=pixel)
                for(size_t row=0;row<repeats;){
                    const auto height=uint32_t(std::min<size_t>(16384,repeats-row));
                    record_color(destination+row*pattern_bytes+column,pattern+column,pixel,pattern_bytes,1,height);row+=height;
                }
        }
        for(size_t i=0;i<tail;++i)record_color(destination+repeats*pattern_bytes+i,pattern+i,1,1,1,1);
    }
    void record_blit(uint64_t destination,uint64_t source,size_t bytes) {
        using P=blitter_packet::Packets;
        // MOCS2 is calibrated by the installed driver capture and 16-way matrix.
        // ponytail: linear buffer copies only; split into legal 16K rectangles.
        for(size_t done=0;done<bytes;) {
            const auto width=uint32_t(std::min<size_t>(16384,bytes-done));
            const auto height=uint32_t(std::min<size_t>(16384,(bytes-done)/width));
            auto blt=P::XY_BLOCK_COPY_BLT::sInit();
            blt.setColorDepth(P::XY_BLOCK_COPY_BLT::COLOR_DEPTH_8_BIT_COLOR);
            blt.setSourceBaseAddress(source+done);blt.setDestinationBaseAddress(destination+done);
            blt.setSourcePitch(width);blt.setDestinationPitch(width);
            blt.setDestinationX2CoordinateRight(width);blt.setDestinationY2CoordinateBottom(height);
            blt.setSourceSurfaceWidth(width);blt.setDestinationSurfaceWidth(width);
            blt.setSourceSurfaceHeight(height);blt.setDestinationSurfaceHeight(height);
            const auto type=height>1?P::XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_2D:P::XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_1D;
            blt.setSourceSurfaceType(type);blt.setDestinationSurfaceType(type);
            blt.setSourceMOCS(2);blt.setDestinationMOCS(2);
            detail::emit(recording.commands,blt);
            if(recording.encoder_state.in_order)detail::emit(recording.commands,blitter_flush());
            detail::emit(recording.commands,P::MI_ARB_CHECK::sInit());
            done+=size_t(width)*height;
        }
        recording.encoder_state.valid=true;
    }
