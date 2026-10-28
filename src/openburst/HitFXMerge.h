#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace hitfx {
struct Bytes { const std::uint8_t* data; std::size_t size; };
struct MergeResult {
    std::uint32_t previousCount = 0, textureCount = 0;
    std::uint32_t tmcSize = 0, tmclSize = 0, ttdmOffset = 0;
    std::array<std::uint32_t, 3> addedIndices{};
};

struct MergeDiagnostics {
    std::uint32_t rawBase = 0, rawPayload = 0;
    std::array<std::uint32_t, 3> rawHeader{}, mirror{};
    bool available = false, mismatch = false;
};
namespace detail {
inline std::uint32_t u32(const std::uint8_t* p) {
    std::uint32_t x; std::memcpy(&x, p, sizeof(x)); return x;
}
inline void put(std::uint8_t* p, std::uint32_t x) { std::memcpy(p, &x, sizeof(x)); }
inline bool range(std::size_t at, std::size_t size, std::size_t limit) {
    return at <= limit && size <= limit - at;
}
inline std::size_t align16(std::size_t x) { return (x + 15u) & ~std::size_t(15u); }
inline bool magic(const std::uint8_t* p, const char* tag, std::size_t n) {
    return std::memcmp(p, tag, n) == 0;
}
inline bool disjoint(std::size_t a, std::size_t an, std::size_t b, std::size_t bn) {
    return a + an <= b || b + bn <= a;
}
inline bool dds(Bytes b) {

    return b.data && b.size == 16512 && magic(b.data, "DDS ", 4) &&
        u32(b.data+4) == 124 && u32(b.data+12) == 128 &&
        u32(b.data+16) == 128 && u32(b.data+24) == 0 &&
        u32(b.data+28) == 1 && u32(b.data+76) == 32 &&
        (u32(b.data+80) & 4) != 0 && magic(b.data+84, "DXT5", 4) &&
        u32(b.data+112) == 0;
}
}

inline bool AppendImpactTextures(std::uint8_t* tmc, std::size_t tmcCapacity,
                                 std::uint8_t* tmcl, std::size_t tmclCapacity,
                                 const std::array<Bytes,3>& textures,
                                 MergeResult* result = nullptr,
                                 const char** error = nullptr,
                                 std::uint32_t expectedCount = 0,
                                 MergeDiagnostics* diagnostics = nullptr) noexcept {
    using namespace detail;
    auto fail = [&](const char* why) { if (error) *error = why; return false; };
    if (error) *error = nullptr;
    if (diagnostics) *diagnostics = MergeDiagnostics{};
    if (!tmc || !tmcl || tmcCapacity < 0x40 || tmclCapacity < 12 ||
        tmcCapacity > 0x800000 || tmclCapacity > 0x800000)
        return fail("invalid pool or capacity");
    const auto ta = reinterpret_cast<std::uintptr_t>(tmc);
    const auto la = reinterpret_cast<std::uintptr_t>(tmcl);
    if (ta > std::numeric_limits<std::uintptr_t>::max()-tmcCapacity ||
        la > std::numeric_limits<std::uintptr_t>::max()-tmclCapacity ||
        !(ta+tmcCapacity <= la || la+tmclCapacity <= ta))
        return fail("overlapping pools");
    const std::size_t oldTmcSize=u32(tmc+0x10), oldRawSize=u32(tmcl+4);
    if (!magic(tmc,"TMC\0",4) || u32(tmc+0xc)!=0x30 || u32(tmc+0x38)!=4 ||
        oldTmcSize<0x40 || oldTmcSize>tmcCapacity ||
        oldRawSize<12 || oldRawSize>tmclCapacity)
        return fail("invalid common headers");
    const std::size_t childCount=u32(tmc+0x18), childTable=u32(tmc+0x20);
    if (!childCount || childCount>64 || u32(tmc+0x14)!=childCount ||
        childTable<0x40 || !range(childTable,childCount*4,oldTmcSize) ||
        u32(tmc+0x24)!=0)
        return fail("unsupported child table");
    std::size_t ttOff=0, lhOff=0, ttIndex=0;
    for (std::size_t i=0;i<childCount;++i) {
        const std::size_t off=u32(tmc+childTable+i*4);
        if (off<childTable+childCount*4 || !range(off,8,oldTmcSize))
            return fail("child offset out of bounds");
        if (magic(tmc+off,"TTDM\0\0\0\0",8)) {
            if (ttOff) return fail("duplicate TTDM");
            ttOff=off; ttIndex=i;
        }
        if (magic(tmc+off,"LHeader\0",8)) {
            if (lhOff) return fail("duplicate LHeader");
            lhOff=off;
        }
    }
    if (!ttOff || !lhOff || !range(ttOff,0x30,oldTmcSize) ||
        !range(lhOff,0x50,oldTmcSize)) return fail("missing texture headers");
    const auto tt=tmc+ttOff; const auto lh=tmc+lhOff;
    const std::size_t ttSize=u32(tt+0x10), lhSize=u32(lh+0x10);
    if (u32(tt+0xc)!=0x30 || ttSize<0x30 || !range(ttOff,ttSize,oldTmcSize) ||
        u32(lh+0xc)!=0x50 || lhSize<0x50 || !range(lhOff,lhSize,oldTmcSize) ||
        !disjoint(ttOff,ttSize,lhOff,lhSize))
        return fail("invalid texture block bounds");
    const std::size_t lhOffsets=u32(lh+0x20), lhSizes=u32(lh+0x24);
    if (u32(lh+0x14)!=1 || u32(lh+0x18)!=1 || lhOffsets<0x50 || lhSizes<0x50 ||
        !range(lhOffsets,4,lhSize) || !range(lhSizes,4,lhSize) ||
        !disjoint(lhOffsets,4,lhSizes,4)) return fail("unsupported LHeader tables");
    const std::size_t rawBase=u32(lh+lhOffsets), rawPayload=u32(lh+lhSizes);
    if (diagnostics) {
        diagnostics->rawBase=static_cast<std::uint32_t>(rawBase);
        diagnostics->rawPayload=static_cast<std::uint32_t>(rawPayload);
        for (unsigned i=0;i<3;++i) {
            diagnostics->rawHeader[i]=u32(tmcl+i*4);
            diagnostics->mirror[i]=u32(lh+0x40+i*4);
        }
        diagnostics->available=true;
        diagnostics->mismatch=diagnostics->rawHeader!=diagnostics->mirror;
    }
    if (rawBase<12 || rawBase>oldRawSize)
        return fail("raw-data base out of bounds");
    if (rawPayload>oldRawSize-rawBase)
        return fail("raw-data payload exceeds buffer extent");
    if (u32(lh+0x44)!=oldRawSize || u32(lh+0x40)!=u32(tmcl) ||
        u32(lh+0x48)!=u32(tmcl+8))
        return fail("inconsistent mirrored raw-data header");

    const std::size_t hOff=0x30,lOff=u32(tt+0x28);
    if (!range(hOff,0x40,ttSize) || !range(lOff,0x50,ttSize) || lOff<hOff+0x40)
        return fail("invalid TTDM children");
    const auto h=tt+hOff; const auto l=tt+lOff;
    const std::size_t hSize=u32(h+0x10),lSize=u32(l+0x10);
    if (!magic(h,"TTDH\0\0\0\0",8) || !magic(l,"TTDL\0\0\0\0",8) ||
        u32(h+0xc)!=0x30 || u32(l+0xc)!=0x50 || hSize<0x40 || lSize<0x50 ||
        !range(hOff,hSize,ttSize) || !range(lOff,lSize,ttSize) ||
        !disjoint(hOff,hSize,lOff,lSize)) return fail("invalid TT(DH/L) blocks");
    const std::size_t n=u32(h+0x18),hTable=u32(h+0x20);
    const std::size_t lOffsets=u32(l+0x20),lSizes=u32(l+0x24);
    const bool supportedCount=expectedCount ? n==expectedCount : (n==214 || n==225);
    if (!n || n>4093 || !supportedCount || u32(h+0x14)!=n ||
        u32(l+0x14)!=n || u32(l+0x18)!=n || u32(l+0x40)!=n ||
        u32(l+0x44)!=rawPayload || hTable<0x40 || lOffsets<0x50 || lSizes<0x50 ||
        !range(hTable,n*4,hSize) || !range(lOffsets,n*4,lSize) ||
        !range(lSizes,n*4,lSize) || !disjoint(lOffsets,n*4,lSizes,n*4))
        return fail("invalid texture count or tables");
    for (std::size_t i=0;i<n;++i) {
        const std::size_t rec=u32(h+hTable+i*4);
        const std::size_t pos=u32(l+lOffsets+i*4),size=u32(l+lSizes+i*4);
        if (rec<0x30 || !range(rec,16,hSize) || !disjoint(rec,16,hTable,n*4) ||
            u32(h+rec)!=1 || u32(h+rec+4)>=n ||
            size<128 || !range(pos,size,rawPayload) ||
            !magic(tmcl+rawBase+pos,"DDS ",4) || u32(tmcl+rawBase+pos+4)!=124)
            return fail("invalid existing texture record");
    }
    for (const auto& texture:textures)
        if (!dds(texture)) return fail("invalid appended DXT5 texture");
    const std::size_t nn=n+3, records=align16(0x40+nn*4);
    const std::size_t newHSize=records+nn*16, newLSizes=align16(0x50+nn*4);
    const std::size_t newLSize=align16(newLSizes+nn*4),newTTSize=0x30+newHSize+newLSize;
    const std::size_t newTTOff=align16(oldTmcSize),newTmcSize=newTTOff+newTTSize;
    std::array<std::size_t,3> rawPos{};
    std::size_t newRawSize=oldRawSize;
    for (std::size_t i=0;i<3;++i) { rawPos[i]=align16(newRawSize); newRawSize=rawPos[i]+textures[i].size; }
    if (newTmcSize>tmcCapacity || newRawSize>tmclCapacity)
        return fail("insufficient native pool capacity");
    try {

        std::vector<std::uint8_t> newTT(newTTSize,0),rawTail(newRawSize-oldRawSize,0);
        std::memcpy(newTT.data(),tt,0x30);
        auto nh=newTT.data()+0x30, nl=nh+newHSize;
        std::memcpy(nh,h,0x40); std::memcpy(nl,l,0x50);
        put(newTT.data()+0x10,static_cast<std::uint32_t>(newTTSize));
        put(newTT.data()+0x28,static_cast<std::uint32_t>(0x30+newHSize));
        put(nh+0x10,static_cast<std::uint32_t>(newHSize));
        put(nh+0x14,static_cast<std::uint32_t>(nn)); put(nh+0x18,static_cast<std::uint32_t>(nn)); put(nh+0x20,0x40);
        put(nl+0x10,static_cast<std::uint32_t>(newLSize));
        put(nl+0x14,static_cast<std::uint32_t>(nn)); put(nl+0x18,static_cast<std::uint32_t>(nn));
        put(nl+0x20,0x50); put(nl+0x24,static_cast<std::uint32_t>(newLSizes));
        put(nl+0x40,static_cast<std::uint32_t>(nn)); put(nl+0x44,static_cast<std::uint32_t>(newRawSize-rawBase));
        for (std::size_t i=0;i<nn;++i) {
            const auto rec=records+i*16;
            put(nh+0x40+i*4,static_cast<std::uint32_t>(rec));
            if (i<n) {
                std::memcpy(nh+rec,h+u32(h+hTable+i*4),16);
                put(nl+0x50+i*4,u32(l+lOffsets+i*4));
                put(nl+newLSizes+i*4,u32(l+lSizes+i*4));
            } else {
                const auto j=i-n;
                put(nh+rec,1); put(nh+rec+4,static_cast<std::uint32_t>(i));
                put(nl+0x50+i*4,static_cast<std::uint32_t>(rawPos[j]-rawBase));
                put(nl+newLSizes+i*4,static_cast<std::uint32_t>(textures[j].size));
                std::memcpy(rawTail.data()+rawPos[j]-oldRawSize,textures[j].data,textures[j].size);
            }
        }
        std::memcpy(tmcl+oldRawSize,rawTail.data(),rawTail.size());
        std::memset(tmc+oldTmcSize,0,newTTOff-oldTmcSize);
        std::memcpy(tmc+newTTOff,newTT.data(),newTT.size());
        put(tmcl+4,static_cast<std::uint32_t>(newRawSize));
        put(tmc+lhOff+0x44,static_cast<std::uint32_t>(newRawSize));
        put(tmc+lhOff+lhSizes,static_cast<std::uint32_t>(newRawSize-rawBase));
        put(tmc+childTable+ttIndex*4,static_cast<std::uint32_t>(newTTOff));
        put(tmc+0x10,static_cast<std::uint32_t>(newTmcSize));
        if (result) {
            result->previousCount=static_cast<std::uint32_t>(n); result->textureCount=static_cast<std::uint32_t>(nn);
            result->tmcSize=static_cast<std::uint32_t>(newTmcSize); result->tmclSize=static_cast<std::uint32_t>(newRawSize);
            result->ttdmOffset=static_cast<std::uint32_t>(newTTOff);
            for (std::size_t i=0;i<3;++i) result->addedIndices[i]=static_cast<std::uint32_t>(n+i);
        }
        return true;
    } catch (...) { return fail("merge staging allocation failed"); }
}
}
