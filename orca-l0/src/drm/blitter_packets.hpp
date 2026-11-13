// Copyright (C) 2021-2026 Intel Corporation; SPDX-License-Identifier: MIT
// Unmodified packet bodies from pinned compute-runtime ee495cce; local glue only.
#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#define BITFIELD_RANGE(a,b) ((b)-(a)+1)
#define STATIC_ASSERT(v) static_assert(v)
#define UNRECOVERABLE_IF(v) do { if(v) throw std::invalid_argument("invalid BLT packet field"); } while(false)
#define DEBUG_BREAK_IF(v) UNRECOVERABLE_IF(v)
#pragma pack(push,1)
namespace blitter_packet { struct Packets {
typedef struct tagXY_FAST_COLOR_BLT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeOfOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTargetOpcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t DestinationAuxiliarySurfaceMode : BITFIELD_RANGE(18, 20);
            uint32_t DestinationMocsValue : BITFIELD_RANGE(21, 27);
            uint32_t DestinationControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t DestinationCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);
            // DWORD 2
            uint32_t DestinationX1CoordinateLeft : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1CoordinateTop : BITFIELD_RANGE(16, 31);
            // DWORD 3
            uint32_t DestinationX2CoordinateRight : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2CoordinateBottom : BITFIELD_RANGE(16, 31);
            // DWORD 4-5
            uint64_t DestinationBaseAddress;
            // DWORD 6
            uint32_t DestinationXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t FillColor[4];
            // DWORD 11-12
            uint64_t DestinationCompressionFormat : BITFIELD_RANGE(0, 4);
            uint64_t DestinationClearValueEnable : BITFIELD_RANGE(5, 5);
            uint64_t DestinationClearAddress : BITFIELD_RANGE(6, 47);
            uint64_t Reserved_400 : BITFIELD_RANGE(48, 63);
            // DWORD 13
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_444 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 14
            uint32_t DestinationLod : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_467 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 15
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_482 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_485 : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_492 : BITFIELD_RANGE(12, 17);
            uint32_t DestinationDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_499 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);
        } Common;
        uint32_t RawData[16];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0xe,
    } DWORD_LENGTH;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagSPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0x0,
        SPECIAL_MODE_OF_OPERATION_FAST_CLEAR_1 = 0x1,
        SPECIAL_MODE_OF_OPERATION_FAST_CLEAR_0 = 0x2,
    } SPECIAL_MODE_OF_OPERATION;
    typedef enum tagCOLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0x0,
        COLOR_DEPTH_16_BIT_COLOR = 0x1,
        COLOR_DEPTH_32_BIT_COLOR = 0x2,
        COLOR_DEPTH_64_BIT_COLOR = 0x3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_SUPPORTED_FOR_LINEAR_CASE = 0x4,
        COLOR_DEPTH_128_BIT_COLOR = 0x5,
    } COLOR_DEPTH;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTION_TARGETOPCODE_XY_FAST_COLOR_BLT = 0x44,
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagDESTINATION_AUXILIARY_SURFACE_MODE {
        DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE = 0x0,
        DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E = 0x5,
    } DESTINATION_AUXILIARY_SURFACE_MODE;
    typedef enum tagDESTINATION_CONTROL_SURFACE_TYPE {
        DESTINATION_CONTROL_SURFACE_TYPE_3D_CONTROL_SURFACE = 0x0,
        DESTINATION_CONTROL_SURFACE_TYPE_MEDIA_CONTROL_SURFACE = 0x1,
    } DESTINATION_CONTROL_SURFACE_TYPE;
    typedef enum tagDESTINATION_TILING {
        DESTINATION_TILING_LINEAR = 0x0,
        DESTINATION_TILING_XMAJOR = 0x1,
        DESTINATION_TILING_TILE4 = 0x2,
        DESTINATION_TILING_TILE64 = 0x3,
    } DESTINATION_TILING;
    typedef enum tagDESTINATION_TARGET_MEMORY {
        DESTINATION_TARGET_MEMORY_LOCAL_MEM = 0x0,
        DESTINATION_TARGET_MEMORY_SYSTEM_MEM = 0x1,
    } DESTINATION_TARGET_MEMORY;
    typedef enum tagDESTINATIONCLEARADDRESS {
        DESTINATIONCLEARADDRESS_BIT_SHIFT = 0x6,
        DESTINATIONCLEARADDRESS_ALIGN_SIZE = 0x40,
    } DESTINATIONCLEARADDRESS;
    typedef enum tagDESTINATION_SURFACE_TYPE {
        DESTINATION_SURFACE_TYPE_SURFTYPE_1D = 0x0,
        DESTINATION_SURFACE_TYPE_SURFTYPE_2D = 0x1,
        DESTINATION_SURFACE_TYPE_SURFTYPE_3D = 0x2,
        DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } DESTINATION_SURFACE_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.InstructionTargetOpcode = INSTRUCTION_TARGETOPCODE_XY_FAST_COLOR_BLT;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
    }
    static tagXY_FAST_COLOR_BLT sInit() {
        XY_FAST_COLOR_BLT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 16);
        return TheStructure.RawData[index];
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setSpecialModeOfOperation(const SPECIAL_MODE_OF_OPERATION value) {
        UNRECOVERABLE_IF(true);
        TheStructure.Common.SpecialModeOfOperation = value;
    }
    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeOfOperation);
    }
    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }
    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTargetOpcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTargetOpcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x40000u);
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setDestinationAuxiliarysurfacemode(const DESTINATION_AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.DestinationAuxiliarySurfaceMode = value;
    }
    inline DESTINATION_AUXILIARY_SURFACE_MODE getDestinationAuxiliarysurfacemode() const {
        return static_cast<DESTINATION_AUXILIARY_SURFACE_MODE>(TheStructure.Common.DestinationAuxiliarySurfaceMode);
    }
    inline void setDestinationMOCS(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fu);
        TheStructure.Common.DestinationMocsValue = value;
    }
    inline uint32_t getDestinationMOCS() const {
        return TheStructure.Common.DestinationMocsValue;
    }
    inline void setDestinationControlSurfaceType(const DESTINATION_CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.DestinationControlSurfaceType = value;
    }
    inline DESTINATION_CONTROL_SURFACE_TYPE getDestinationControlSurfaceType() const {
        return static_cast<DESTINATION_CONTROL_SURFACE_TYPE>(TheStructure.Common.DestinationControlSurfaceType);
    }
    inline void setDestinationCompressionEnable(const bool value) {
        TheStructure.Common.DestinationCompressionEnable = value;
    }
    inline bool getDestinationCompressionEnable() const {
        return TheStructure.Common.DestinationCompressionEnable;
    }
    inline void setDestinationTiling(const DESTINATION_TILING value) {
        TheStructure.Common.DestinationTiling = value;
    }
    inline DESTINATION_TILING getDestinationTiling() const {
        return static_cast<DESTINATION_TILING>(TheStructure.Common.DestinationTiling);
    }
    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationX1CoordinateLeft = value;
    }
    inline uint32_t getDestinationX1CoordinateLeft() const {
        return TheStructure.Common.DestinationX1CoordinateLeft;
    }
    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationY1CoordinateTop = value;
    }
    inline uint32_t getDestinationY1CoordinateTop() const {
        return TheStructure.Common.DestinationY1CoordinateTop;
    }
    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationX2CoordinateRight = value;
    }
    inline uint32_t getDestinationX2CoordinateRight() const {
        return TheStructure.Common.DestinationX2CoordinateRight;
    }
    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationY2CoordinateBottom = value;
    }
    inline uint32_t getDestinationY2CoordinateBottom() const {
        return TheStructure.Common.DestinationY2CoordinateBottom;
    }
    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }
    inline uint64_t getDestinationBaseAddress() const {
        return TheStructure.Common.DestinationBaseAddress;
    }
    inline void setDestinationXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.DestinationXOffset = value;
    }
    inline uint32_t getDestinationXOffset() const {
        return TheStructure.Common.DestinationXOffset;
    }
    inline void setDestinationYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.DestinationYOffset = value;
    }
    inline uint32_t getDestinationYOffset() const {
        return TheStructure.Common.DestinationYOffset;
    }
    inline void setDestinationTargetMemory(const DESTINATION_TARGET_MEMORY value) {
        TheStructure.Common.DestinationTargetMemory = value;
    }
    inline DESTINATION_TARGET_MEMORY getDestinationTargetMemory() const {
        return static_cast<DESTINATION_TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }
    inline void setFillColor(const uint32_t *value) {
        TheStructure.Common.FillColor[0] = value[0];
        TheStructure.Common.FillColor[1] = value[1];
        TheStructure.Common.FillColor[2] = value[2];
        TheStructure.Common.FillColor[3] = value[3];
    }
    inline void setDestinationCompressionFormat(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fu);
        TheStructure.Common.DestinationCompressionFormat = value;
    }
    inline uint32_t getDestinationCompressionFormat() const {
        return TheStructure.Common.DestinationCompressionFormat;
    }
    inline void setDestinationClearValueEnable(const bool value) {
        TheStructure.Common.DestinationClearValueEnable = value;
    }
    inline bool getDestinationClearValueEnable() const {
        return TheStructure.Common.DestinationClearValueEnable;
    }
    inline void setDestinationClearAddress(const uint64_t value) {
        TheStructure.Common.DestinationClearAddress = value >> DESTINATIONCLEARADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDestinationClearAddress() const {
        return TheStructure.Common.DestinationClearAddress << DESTINATIONCLEARADDRESS_BIT_SHIFT;
    }
    inline void setDestinationSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }
    inline uint32_t getDestinationSurfaceHeight() const {
        return TheStructure.Common.DestinationSurfaceHeight + 1;
    }
    inline void setDestinationSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }
    inline uint32_t getDestinationSurfaceWidth() const {
        return TheStructure.Common.DestinationSurfaceWidth + 1;
    }
    inline void setDestinationSurfaceType(const DESTINATION_SURFACE_TYPE value) {
        TheStructure.Common.DestinationSurfaceType = value;
    }
    inline DESTINATION_SURFACE_TYPE getDestinationSurfaceType() const {
        return static_cast<DESTINATION_SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }
    inline void setDestinationLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.DestinationLod = value;
    }
    inline uint32_t getDestinationLod() const {
        return TheStructure.Common.DestinationLod;
    }
    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fffu);
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }
    inline uint32_t getDestinationSurfaceQpitch() const {
        return TheStructure.Common.DestinationSurfaceQpitch;
    }
    inline void setDestinationSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.DestinationSurfaceDepth = value - 1;
    }
    inline uint32_t getDestinationSurfaceDepth() const {
        return TheStructure.Common.DestinationSurfaceDepth + 1;
    }
    inline void setDestinationHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.DestinationHorizontalAlign = value;
    }
    inline uint32_t getDestinationHorizontalAlign() const {
        return TheStructure.Common.DestinationHorizontalAlign;
    }
    inline void setDestinationVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.DestinationVerticalAlign = value;
    }
    inline uint32_t getDestinationVerticalAlign() const {
        return TheStructure.Common.DestinationVerticalAlign;
    }
    inline void setDestinationMipTailStartLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.DestinationMipTailStartLod = value;
    }
    inline uint32_t getDestinationMipTailStartLOD() const {
        return TheStructure.Common.DestinationMipTailStartLod;
    }
    inline void setDestinationDepthStencilResource(const bool value) {
        TheStructure.Common.DestinationDepthStencilResource = value;
    }
    inline bool getDestinationDepthStencilResource() const {
        return TheStructure.Common.DestinationDepthStencilResource;
    }
    inline void setDestinationArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }
    inline uint32_t getDestinationArrayIndex() const {
        return TheStructure.Common.DestinationArrayIndex + 1;
    }
} XY_FAST_COLOR_BLT;
STATIC_ASSERT(64 == sizeof(XY_FAST_COLOR_BLT));

typedef struct tagXY_BLOCK_COPY_BLT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeOfOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTargetOpcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t DestinationAuxiliarySurfaceMode : BITFIELD_RANGE(18, 20);
            uint32_t DestinationMocsValue : BITFIELD_RANGE(21, 27);
            uint32_t DestinationControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t DestinationCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);
            // DWORD 2
            uint32_t DestinationX1CoordinateLeft : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1CoordinateTop : BITFIELD_RANGE(16, 31);
            // DWORD 3
            uint32_t DestinationX2CoordinateRight : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2CoordinateBottom : BITFIELD_RANGE(16, 31);
            // DWORD 4-5
            uint64_t DestinationBaseAddress;
            // DWORD 6
            uint32_t DestinationXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t SourceX1CoordinateLeft : BITFIELD_RANGE(0, 15);
            uint32_t SourceY1CoordinateTop : BITFIELD_RANGE(16, 31);
            // DWORD 8
            uint32_t SourcePitch : BITFIELD_RANGE(0, 17);
            uint32_t SourceAuxiliarySurfaceMode : BITFIELD_RANGE(18, 20);
            uint32_t SourceMocs : BITFIELD_RANGE(21, 27);
            uint32_t SourceControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t SourceCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t SourceTiling : BITFIELD_RANGE(30, 31);
            // DWORD 9-10
            uint64_t SourceBaseAddress;
            // DWORD 11
            uint32_t SourceXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_366 : BITFIELD_RANGE(14, 15);
            uint32_t SourceYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_382 : BITFIELD_RANGE(30, 30);
            uint32_t SourceTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 12-13
            uint64_t SourceCompressionFormat : BITFIELD_RANGE(0, 4);
            uint64_t SourceClearValueEnable : BITFIELD_RANGE(5, 5);
            uint64_t SourceClearAddress : BITFIELD_RANGE(6, 47);
            uint64_t Reserved_432 : BITFIELD_RANGE(48, 63);
            // DWORD 14-15
            uint64_t DestinationCompressionFormat : BITFIELD_RANGE(0, 4);
            uint64_t DestinationClearValueEnable : BITFIELD_RANGE(5, 5);
            uint64_t DestinationClearAddress : BITFIELD_RANGE(6, 47);
            uint64_t Reserved_496 : BITFIELD_RANGE(48, 63);
            // DWORD 16
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_540 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 17
            uint32_t DestinationLod : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_563 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 18
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_578 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_581 : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_588 : BITFIELD_RANGE(12, 17);
            uint32_t DestinationDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_595 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);
            // DWORD 19
            uint32_t SourceSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t SourceSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_636 : BITFIELD_RANGE(28, 28);
            uint32_t SourceSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 20
            uint32_t SourceLod : BITFIELD_RANGE(0, 3);
            uint32_t SourceSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_659 : BITFIELD_RANGE(19, 20);
            uint32_t SourceSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 21
            uint32_t SourceHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_674 : BITFIELD_RANGE(2, 2);
            uint32_t SourceVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_677 : BITFIELD_RANGE(5, 7);
            uint32_t SourceMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_684 : BITFIELD_RANGE(12, 17);
            uint32_t SourceDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_691 : BITFIELD_RANGE(19, 20);
            uint32_t SourceArrayIndex : BITFIELD_RANGE(21, 31);
        } Common;
        uint32_t RawData[22];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x14,
    } DWORD_LENGTH;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagSPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0x0,
        SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE = 0x1,
        SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE = 0x2,
    } SPECIAL_MODE_OF_OPERATION;
    typedef enum tagCOLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0x0,
        COLOR_DEPTH_16_BIT_COLOR = 0x1,
        COLOR_DEPTH_32_BIT_COLOR = 0x2,
        COLOR_DEPTH_64_BIT_COLOR = 0x3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED = 0x4,
        COLOR_DEPTH_128_BIT_COLOR = 0x5,
    } COLOR_DEPTH;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTION_TARGETOPCODE_INSTRUCTION_TARGET_XY_BLOCK_COPY_BLT = 0x41,
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagAUXILIARY_SURFACE_MODE {
        AUXILIARY_SURFACE_MODE_AUX_NONE = 0x0,
        AUXILIARY_SURFACE_MODE_AUX_CCS_E = 0x5,
    } AUXILIARY_SURFACE_MODE;
    typedef enum tagCONTROL_SURFACE_TYPE {
        CONTROL_SURFACE_TYPE_3D_CONTROL_SURFACE = 0x0,
        CONTROL_SURFACE_TYPE_MEDIA_CONTROL_SURFACE = 0x1,
    } CONTROL_SURFACE_TYPE;
    typedef enum tagTILING {
        TILING_LINEAR = 0x0,
        TILING_XMAJOR = 0x1,
        TILING_TILE4 = 0x2,
        TILING_TILE64 = 0x3,
    } TILING;
    typedef enum tagTARGET_MEMORY {
        TARGET_MEMORY_LOCAL_MEM = 0x0,
        TARGET_MEMORY_SYSTEM_MEM = 0x1,
    } TARGET_MEMORY;
    typedef enum tagSOURCECLEARADDRESS {
        SOURCECLEARADDRESS_BIT_SHIFT = 0x6,
        SOURCECLEARADDRESS_ALIGN_SIZE = 0x40,
    } SOURCECLEARADDRESS;
    typedef enum tagDESTINATIONCLEARADDRESS {
        DESTINATIONCLEARADDRESS_BIT_SHIFT = 0x6,
        DESTINATIONCLEARADDRESS_ALIGN_SIZE = 0x40,
    } DESTINATIONCLEARADDRESS;
    typedef enum tagSURFACE_TYPE {
        SURFACE_TYPE_SURFTYPE_1D = 0x0,
        SURFACE_TYPE_SURFTYPE_2D = 0x1,
        SURFACE_TYPE_SURFTYPE_3D = 0x2,
        SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } SURFACE_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.InstructionTargetOpcode = INSTRUCTION_TARGETOPCODE_INSTRUCTION_TARGET_XY_BLOCK_COPY_BLT;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
    }
    static tagXY_BLOCK_COPY_BLT sInit() {
        XY_BLOCK_COPY_BLT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 22);
        return TheStructure.RawData[index];
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setSpecialModeOfOperation(const SPECIAL_MODE_OF_OPERATION value) {
        TheStructure.Common.SpecialModeOfOperation = value;
    }
    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeOfOperation);
    }
    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }
    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTargetOpcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTargetOpcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x40000u);
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setDestinationAuxiliarysurfacemode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.DestinationAuxiliarySurfaceMode = value;
    }
    inline AUXILIARY_SURFACE_MODE getDestinationAuxiliarysurfacemode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure.Common.DestinationAuxiliarySurfaceMode);
    }
    inline void setDestinationMOCS(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fu);
        TheStructure.Common.DestinationMocsValue = value;
    }
    inline uint32_t getDestinationMOCS() const {
        return TheStructure.Common.DestinationMocsValue;
    }
    inline void setDestinationControlSurfaceType(const CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.DestinationControlSurfaceType = value;
    }
    inline CONTROL_SURFACE_TYPE getDestinationControlSurfaceType() const {
        return static_cast<CONTROL_SURFACE_TYPE>(TheStructure.Common.DestinationControlSurfaceType);
    }
    inline void setDestinationCompressionEnable(const bool value) {
        TheStructure.Common.DestinationCompressionEnable = value;
    }
    inline bool getDestinationCompressionEnable() const {
        return TheStructure.Common.DestinationCompressionEnable;
    }
    inline void setDestinationTiling(const TILING value) {
        TheStructure.Common.DestinationTiling = value;
    }
    inline TILING getDestinationTiling() const {
        return static_cast<TILING>(TheStructure.Common.DestinationTiling);
    }
    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationX1CoordinateLeft = value;
    }
    inline uint32_t getDestinationX1CoordinateLeft() const {
        return TheStructure.Common.DestinationX1CoordinateLeft;
    }
    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationY1CoordinateTop = value;
    }
    inline uint32_t getDestinationY1CoordinateTop() const {
        return TheStructure.Common.DestinationY1CoordinateTop;
    }
    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationX2CoordinateRight = value;
    }
    inline uint32_t getDestinationX2CoordinateRight() const {
        return TheStructure.Common.DestinationX2CoordinateRight;
    }
    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.DestinationY2CoordinateBottom = value;
    }
    inline uint32_t getDestinationY2CoordinateBottom() const {
        return TheStructure.Common.DestinationY2CoordinateBottom;
    }
    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }
    inline uint64_t getDestinationBaseAddress() const {
        return TheStructure.Common.DestinationBaseAddress;
    }
    inline void setDestinationXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.DestinationXOffset = value;
    }
    inline uint32_t getDestinationXOffset() const {
        return TheStructure.Common.DestinationXOffset;
    }
    inline void setDestinationYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.DestinationYOffset = value;
    }
    inline uint32_t getDestinationYOffset() const {
        return TheStructure.Common.DestinationYOffset;
    }
    inline void setDestinationTargetMemory(const TARGET_MEMORY value) {
        TheStructure.Common.DestinationTargetMemory = value;
    }
    inline TARGET_MEMORY getDestinationTargetMemory() const {
        return static_cast<TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }
    inline void setSourceX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.SourceX1CoordinateLeft = value;
    }
    inline uint32_t getSourceX1CoordinateLeft() const {
        return TheStructure.Common.SourceX1CoordinateLeft;
    }
    inline void setSourceY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffu);
        TheStructure.Common.SourceY1CoordinateTop = value;
    }
    inline uint32_t getSourceY1CoordinateTop() const {
        return TheStructure.Common.SourceY1CoordinateTop;
    }
    inline void setSourcePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x40000u);
        TheStructure.Common.SourcePitch = value - 1;
    }
    inline uint32_t getSourcePitch() const {
        return TheStructure.Common.SourcePitch + 1;
    }
    inline void setSourceAuxiliarysurfacemode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.SourceAuxiliarySurfaceMode = value;
    }
    inline AUXILIARY_SURFACE_MODE getSourceAuxiliarysurfacemode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure.Common.SourceAuxiliarySurfaceMode);
    }
    inline void setSourceMOCS(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fu);
        TheStructure.Common.SourceMocs = value;
    }
    inline uint32_t getSourceMOCS() const {
        return TheStructure.Common.SourceMocs;
    }
    inline void setSourceControlSurfaceType(const CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.SourceControlSurfaceType = value;
    }
    inline CONTROL_SURFACE_TYPE getSourceControlSurfaceType() const {
        return static_cast<CONTROL_SURFACE_TYPE>(TheStructure.Common.SourceControlSurfaceType);
    }
    inline void setSourceCompressionEnable(const bool value) {
        TheStructure.Common.SourceCompressionEnable = value;
    }
    inline bool getSourceCompressionEnable() const {
        return TheStructure.Common.SourceCompressionEnable;
    }
    inline void setSourceTiling(const TILING value) {
        TheStructure.Common.SourceTiling = value;
    }
    inline TILING getSourceTiling() const {
        return static_cast<TILING>(TheStructure.Common.SourceTiling);
    }
    inline void setSourceBaseAddress(const uint64_t value) {
        TheStructure.Common.SourceBaseAddress = value;
    }
    inline uint64_t getSourceBaseAddress() const {
        return TheStructure.Common.SourceBaseAddress;
    }
    inline void setSourceXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.SourceXOffset = value;
    }
    inline uint32_t getSourceXOffset() const {
        return TheStructure.Common.SourceXOffset;
    }
    inline void setSourceYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffu);
        TheStructure.Common.SourceYOffset = value;
    }
    inline uint32_t getSourceYOffset() const {
        return TheStructure.Common.SourceYOffset;
    }
    inline void setSourceTargetMemory(const TARGET_MEMORY value) {
        TheStructure.Common.SourceTargetMemory = value;
    }
    inline TARGET_MEMORY getSourceTargetMemory() const {
        return static_cast<TARGET_MEMORY>(TheStructure.Common.SourceTargetMemory);
    }
    inline void setSourceCompressionFormat(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fu);
        TheStructure.Common.SourceCompressionFormat = value;
    }
    inline uint32_t getSourceCompressionFormat() const {
        return TheStructure.Common.SourceCompressionFormat;
    }
    inline void setSourceClearValueEnable(const bool value) {
        TheStructure.Common.SourceClearValueEnable = value;
    }
    inline bool getSourceClearValueEnable() const {
        return TheStructure.Common.SourceClearValueEnable;
    }
    inline void setSourceClearAddress(const uint64_t value) {
        TheStructure.Common.SourceClearAddress = value >> SOURCECLEARADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSourceClearAddress() const {
        return TheStructure.Common.SourceClearAddress << SOURCECLEARADDRESS_BIT_SHIFT;
    }
    inline void setDestinationCompressionFormat(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fu);
        TheStructure.Common.DestinationCompressionFormat = value;
    }
    inline uint32_t getDestinationCompressionFormat() const {
        return TheStructure.Common.DestinationCompressionFormat;
    }
    inline void setDestinationClearValueEnable(const bool value) {
        TheStructure.Common.DestinationClearValueEnable = value;
    }
    inline bool getDestinationClearValueEnable() const {
        return TheStructure.Common.DestinationClearValueEnable;
    }
    inline void setDestinationClearAddress(const uint64_t value) {
        TheStructure.Common.DestinationClearAddress = value >> DESTINATIONCLEARADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDestinationClearAddress() const {
        return TheStructure.Common.DestinationClearAddress << DESTINATIONCLEARADDRESS_BIT_SHIFT;
    }
    inline void setDestinationSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }
    inline uint32_t getDestinationSurfaceHeight() const {
        return TheStructure.Common.DestinationSurfaceHeight + 1;
    }
    inline void setDestinationSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }
    inline uint32_t getDestinationSurfaceWidth() const {
        return TheStructure.Common.DestinationSurfaceWidth + 1;
    }
    inline void setDestinationSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.DestinationSurfaceType = value;
    }
    inline SURFACE_TYPE getDestinationSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }
    inline void setDestinationLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.DestinationLod = value;
    }
    inline uint32_t getDestinationLod() const {
        return TheStructure.Common.DestinationLod;
    }
    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fffu);
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }
    inline uint32_t getDestinationSurfaceQpitch() const {
        return TheStructure.Common.DestinationSurfaceQpitch;
    }
    inline void setDestinationSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.DestinationSurfaceDepth = value - 1;
    }
    inline uint32_t getDestinationSurfaceDepth() const {
        return TheStructure.Common.DestinationSurfaceDepth + 1;
    }
    inline void setDestinationHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.DestinationHorizontalAlign = value;
    }
    inline uint32_t getDestinationHorizontalAlign() const {
        return TheStructure.Common.DestinationHorizontalAlign;
    }
    inline void setDestinationVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.DestinationVerticalAlign = value;
    }
    inline uint32_t getDestinationVerticalAlign() const {
        return TheStructure.Common.DestinationVerticalAlign;
    }
    inline void setDestinationMipTailStartLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.DestinationMipTailStartLod = value;
    }
    inline uint32_t getDestinationMipTailStartLOD() const {
        return TheStructure.Common.DestinationMipTailStartLod;
    }
    inline void setDestinationDepthStencilResource(const bool value) {
        TheStructure.Common.DestinationDepthStencilResource = value;
    }
    inline bool getDestinationDepthStencilResource() const {
        return TheStructure.Common.DestinationDepthStencilResource;
    }
    inline void setDestinationArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }
    inline uint32_t getDestinationArrayIndex() const {
        return TheStructure.Common.DestinationArrayIndex + 1;
    }
    inline void setSourceSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.SourceSurfaceHeight = value - 1;
    }
    inline uint32_t getSourceSurfaceHeight() const {
        return TheStructure.Common.SourceSurfaceHeight + 1;
    }
    inline void setSourceSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x4000u);
        TheStructure.Common.SourceSurfaceWidth = value - 1;
    }
    inline uint32_t getSourceSurfaceWidth() const {
        return TheStructure.Common.SourceSurfaceWidth + 1;
    }
    inline void setSourceSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.SourceSurfaceType = value;
    }
    inline SURFACE_TYPE getSourceSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SourceSurfaceType);
    }
    inline void setSourceLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.SourceLod = value;
    }
    inline uint32_t getSourceLod() const {
        return TheStructure.Common.SourceLod;
    }
    inline void setSourceSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fffu);
        TheStructure.Common.SourceSurfaceQpitch = value;
    }
    inline uint32_t getSourceSurfaceQpitch() const {
        return TheStructure.Common.SourceSurfaceQpitch;
    }
    inline void setSourceSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.SourceSurfaceDepth = value - 1;
    }
    inline uint32_t getSourceSurfaceDepth() const {
        return TheStructure.Common.SourceSurfaceDepth + 1;
    }
    inline void setSourceHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.SourceHorizontalAlign = value;
    }
    inline uint32_t getSourceHorizontalAlign() const {
        return TheStructure.Common.SourceHorizontalAlign;
    }
    inline void setSourceVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3u);
        TheStructure.Common.SourceVerticalAlign = value;
    }
    inline uint32_t getSourceVerticalAlign() const {
        return TheStructure.Common.SourceVerticalAlign;
    }
    inline void setSourceMipTailStartLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfu);
        TheStructure.Common.SourceMipTailStartLod = value;
    }
    inline uint32_t getSourceMipTailStartLOD() const {
        return TheStructure.Common.SourceMipTailStartLod;
    }
    inline void setSourceDepthStencilResource(const bool value) {
        TheStructure.Common.SourceDepthStencilResource = value;
    }
    inline bool getSourceDepthStencilResource() const {
        return TheStructure.Common.SourceDepthStencilResource;
    }
    inline void setSourceArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value < 1 || value > 0x800u);
        TheStructure.Common.SourceArrayIndex = value - 1;
    }
    inline uint32_t getSourceArrayIndex() const {
        return TheStructure.Common.SourceArrayIndex + 1;
    }
} XY_BLOCK_COPY_BLT;
STATIC_ASSERT(88 == sizeof(XY_BLOCK_COPY_BLT));
struct MI_FLUSH_DW {
    union tagData {
        struct tagCommon {
            /// DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t FlushLlc : BITFIELD_RANGE(9, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t FlushCcs : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);

            /// DWORD 1..2
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t DestinationAddress : BITFIELD_RANGE(2, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);

            /// DWORD 3..4
            uint64_t ImmediateData;

        } Common;
        uint32_t RawData[5];
    } TheStructure;

    enum DWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 3,
    };

    enum POST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD = 1,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER = 3,
    };

    enum MI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_FLUSH_DW = 38,
    };

    enum COMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0,
    };

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH::DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE::MI_COMMAND_OPCODE_MI_FLUSH_DW;
    }

    static MI_FLUSH_DW sInit() {
        MI_FLUSH_DW state;
        state.init();
        return state;
    }

    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index < 5);
        return TheStructure.RawData[index];
    }

    inline void setNotifyEnable(const uint32_t value) {
        TheStructure.Common.NotifyEnable = value;
    }

    inline uint32_t getNotifyEnable() const {
        return (TheStructure.Common.NotifyEnable);
    }

    inline void setFlushLlc(const uint32_t value) {
        TheStructure.Common.FlushLlc = value;
    }

    inline uint32_t getFlushLlc() const {
        return (TheStructure.Common.FlushLlc);
    }

    inline void setFlushCcs(const uint32_t value) {
        TheStructure.Common.FlushCcs = value;
    }

    inline uint32_t getFlushCcs() const {
        return (TheStructure.Common.FlushCcs);
    }

    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }

    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }

    inline void setTlbInvalidate(const uint32_t value) {
        TheStructure.Common.TlbInvalidate = value;
    }

    inline uint32_t getTlbInvalidate() const {
        return (TheStructure.Common.TlbInvalidate);
    }

    inline void setStoreDataIndex(const uint32_t value) {
        TheStructure.Common.StoreDataIndex = value;
    }

    inline uint32_t getStoreDataIndex() const {
        return (TheStructure.Common.StoreDataIndex);
    }

    typedef enum tagDESTINATIONADDRESS {
        DESTINATIONADDRESS_BIT_SHIFT = 2,
        DESTINATIONADDRESS_ALIGN_SIZE = 4,
    } DESTINATIONADDRESS;

    inline void setDestinationAddress(const uint64_t value) {
        TheStructure.Common.DestinationAddress = value >> DESTINATIONADDRESS_BIT_SHIFT;
    }

    inline uint64_t getDestinationAddress() const {
        return (TheStructure.Common.DestinationAddress << DESTINATIONADDRESS_BIT_SHIFT);
    }

    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }

    inline uint64_t getImmediateData() const {
        return (TheStructure.Common.ImmediateData);
    }
};
STATIC_ASSERT(20 == sizeof(MI_FLUSH_DW));
typedef struct tagMI_ARB_CHECK {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t PreParserDisable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_1 : BITFIELD_RANGE(1, 7);
            uint32_t MaskBits : BITFIELD_RANGE(8, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_ARB_CHECK = 0x5,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ARB_CHECK;
    }
    static tagMI_ARB_CHECK sInit() {
        MI_ARB_CHECK state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPreParserDisable(const bool value) {
        TheStructure.Common.PreParserDisable = value;
        TheStructure.Common.MaskBits |= 0x1; // Manual generator override
    }
    inline bool getPreParserDisable() const {
        return TheStructure.Common.PreParserDisable;
    }
    inline void setMaskBits(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffu);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
    inline void setMiCommandOpcode(const MI_COMMAND_OPCODE value) {
        TheStructure.Common.MiCommandOpcode = value;
    }
    inline MI_COMMAND_OPCODE getMiCommandOpcode() const {
        return static_cast<MI_COMMAND_OPCODE>(TheStructure.Common.MiCommandOpcode);
    }
    inline void setCommandType(const COMMAND_TYPE value) {
        TheStructure.Common.CommandType = value;
    }
    inline COMMAND_TYPE getCommandType() const {
        return static_cast<COMMAND_TYPE>(TheStructure.Common.CommandType);
    }
} MI_ARB_CHECK;
STATIC_ASSERT(4 == sizeof(MI_ARB_CHECK));
};}
#pragma pack(pop)
#undef BITFIELD_RANGE
#undef STATIC_ASSERT
#undef UNRECOVERABLE_IF
#undef DEBUG_BREAK_IF
