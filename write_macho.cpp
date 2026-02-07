#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

#include <CommonCrypto/CommonDigest.h>

#include "macho_utils.h"

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "REQUIRE failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

static const std::string kMessage =
    "hello from libSystem Write(), now with a much longer message from write_macho.cpp!\nSecond line.\n";

static void vec_append(std::vector<uint8_t> &data, const void *item, size_t size) {
    const uint8_t *p = (const uint8_t *)item;
    data.insert(data.end(), p, p + size);
}

static void append_u8(std::vector<uint8_t> &out, uint8_t v) {
    out.push_back(v);
}

static void append_le16(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
}

static void append_le32(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
    out.push_back((uint8_t)((v >> 16) & 0xff));
    out.push_back((uint8_t)((v >> 24) & 0xff));
}

static void append_le64(std::vector<uint8_t> &out, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        out.push_back((uint8_t)((v >> (8 * i)) & 0xff));
    }
}

static void append_be32(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back((uint8_t)((v >> 24) & 0xff));
    out.push_back((uint8_t)((v >> 16) & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
    out.push_back((uint8_t)(v & 0xff));
}

static void append_uleb128(std::vector<uint8_t> &out, uint64_t value) {
    do {
        uint8_t byte = (uint8_t)(value & 0x7f);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

static void append_cstr(std::vector<uint8_t> &out, const std::string &s) {
    out.insert(out.end(), s.begin(), s.end());
    out.push_back('\0');
}

static void append_padding_to(std::vector<uint8_t> &out, size_t target_size) {
    REQUIRE(out.size() <= target_size);
    out.insert(out.end(), target_size - out.size(), 0);
}

static constexpr uint64_t kTextAddr = 0x100000410ULL;
static constexpr uint64_t kStubsAddr = 0x10000042cULL;
static constexpr uint64_t kCStringAddr = 0x100000444ULL;
static constexpr uint64_t kGotAddr = 0x100004000ULL;

static uint32_t arm64_movz_64(uint8_t rd, uint16_t imm16, uint8_t shift) {
    REQUIRE(rd <= 31);
    REQUIRE((shift % 16) == 0);
    const uint8_t hw = (uint8_t)(shift / 16);
    REQUIRE(hw <= 3);
    return 0xd2800000U | ((uint32_t)hw << 21) | ((uint32_t)imm16 << 5) | (uint32_t)rd;
}

static uint32_t arm64_adrp(uint8_t rd, int64_t page_delta) {
    REQUIRE(rd <= 31);
    REQUIRE(page_delta >= -(1 << 20));
    REQUIRE(page_delta <= ((1 << 20) - 1));

    const int64_t imm = page_delta;
    const uint32_t immlo = (uint32_t)(imm & 0x3);
    const uint32_t immhi = (uint32_t)((imm >> 2) & 0x7ffff);
    return 0x90000000U | (immlo << 29) | (immhi << 5) | (uint32_t)rd;
}

static uint32_t arm64_add_imm_64(uint8_t rd, uint8_t rn, uint16_t imm12, uint8_t shift) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    REQUIRE(shift == 0 || shift == 12);
    const uint32_t sh = shift == 12 ? 1U : 0U;
    return 0x91000000U | (sh << 22) | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_bl(int32_t imm26) {
    REQUIRE(imm26 >= -(1 << 25));
    REQUIRE(imm26 <= ((1 << 25) - 1));
    return 0x94000000U | ((uint32_t)imm26 & 0x03ffffffU);
}

static uint32_t arm64_ldr_imm_u64(uint8_t rt, uint8_t rn, uint16_t byte_offset) {
    REQUIRE(rt <= 31);
    REQUIRE(rn <= 31);
    REQUIRE((byte_offset % 8) == 0);
    const uint16_t imm12 = (uint16_t)(byte_offset / 8);
    REQUIRE(imm12 <= 0x0fff);
    return 0xf9400000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

static uint32_t arm64_br(uint8_t rn) {
    REQUIRE(rn <= 31);
    return 0xd61f0000U | ((uint32_t)rn << 5);
}

static void emit_arm64(std::vector<uint8_t> &out, uint32_t inst) {
    append_le32(out, inst);
}

static int64_t arm64_adrp_page_delta(uint64_t from_insn_addr, uint64_t to_addr) {
    const uint64_t from_page = from_insn_addr & ~0xfffULL;
    const uint64_t to_page = to_addr & ~0xfffULL;
    REQUIRE((to_page % 4096) == 0);
    REQUIRE((from_page % 4096) == 0);
    return (int64_t)((to_page - from_page) / 4096);
}

static int32_t arm64_bl_imm26_from_addrs(uint64_t from_insn_addr, uint64_t to_addr) {
    const int64_t delta = (int64_t)to_addr - (int64_t)from_insn_addr;
    REQUIRE((delta % 4) == 0);
    return (int32_t)(delta / 4);
}

static std::vector<uint8_t> build_text_bytes(uint16_t msg_len) {
    // _main in exit.asm:
    //   mov x0,#1 ; adrp/add for msg ; mov x2,#msg_len ; bl _write ; mov x0,#42 ; bl _exit
    REQUIRE(msg_len <= 0xffff);
    const uint64_t kMainAdrpMsgAddr = kTextAddr + 0x4;
    const uint64_t kMainBlWriteAddr = kTextAddr + 0x10;
    const uint64_t kMainBlExitAddr = kTextAddr + 0x18;
    const uint64_t kExitStubAddr = kStubsAddr + 0x0;
    const uint64_t kWriteStubAddr = kStubsAddr + 0xc;

    std::vector<uint8_t> out;
    emit_arm64(out, arm64_movz_64(0, 1, 0));                      // mov x0, #1
    emit_arm64(out, arm64_adrp(1, arm64_adrp_page_delta(kMainAdrpMsgAddr, kCStringAddr)));
    emit_arm64(out, arm64_add_imm_64(1, 1, (uint16_t)(kCStringAddr & 0xfffU), 0));
    emit_arm64(out, arm64_movz_64(2, msg_len, 0));                // mov x2, #msg_len
    emit_arm64(out, arm64_bl(arm64_bl_imm26_from_addrs(kMainBlWriteAddr, kWriteStubAddr)));
    emit_arm64(out, arm64_movz_64(0, 42, 0));                     // mov x0, #42
    emit_arm64(out, arm64_bl(arm64_bl_imm26_from_addrs(kMainBlExitAddr, kExitStubAddr)));
    REQUIRE(out.size() == 28);
    return out;
}

static std::vector<uint8_t> build_stub_bytes() {
    // 2 symbol stubs, 12 bytes each
    const uint64_t kStub0AdrpAddr = kStubsAddr + 0x0;
    const uint64_t kStub1AdrpAddr = kStubsAddr + 0xc;

    std::vector<uint8_t> out;
    emit_arm64(out, arm64_adrp(16, arm64_adrp_page_delta(kStub0AdrpAddr, kGotAddr)));
    emit_arm64(out, arm64_ldr_imm_u64(16, 16, 0));                // ldr x16, [x16, #0]
    emit_arm64(out, arm64_br(16));                                // br x16

    emit_arm64(out, arm64_adrp(16, arm64_adrp_page_delta(kStub1AdrpAddr, kGotAddr)));
    emit_arm64(out, arm64_ldr_imm_u64(16, 16, 8));                // ldr x16, [x16, #8]
    emit_arm64(out, arm64_br(16));                                // br x16
    REQUIRE(out.size() == 24);
    return out;
}

static std::vector<uint8_t> build_cstring_bytes() {
    std::vector<uint8_t> out;
    append_cstr(out, kMessage);
    return out;
}

static std::vector<uint8_t> build_got_bytes() {
    // Entries as emitted by ld/dyld chained fixups.
    std::vector<uint8_t> out;
    append_le64(out, 0x8010000000000000ULL);
    append_le64(out, 0x8000000000000001ULL);
    REQUIRE(out.size() == 16);
    return out;
}

static std::vector<uint8_t> build_chained_fixups_blob() {
    std::vector<uint8_t> out;

    // dyld_chained_fixups_header
    append_le32(out, 0);      // fixups_version
    append_le32(out, 0x20);   // starts_offset
    append_le32(out, 0x50);   // imports_offset
    append_le32(out, 0x58);   // symbols_offset
    append_le32(out, 2);      // imports_count
    append_le32(out, 1);      // imports_format (DYLD_CHAINED_IMPORT)
    append_le32(out, 0);      // symbols_format

    append_le32(out, 0);      // pad to starts_offset

    // dyld_chained_starts_in_image
    append_le32(out, 4);      // seg_count
    append_le32(out, 0);      // __PAGEZERO
    append_le32(out, 0);      // __TEXT
    append_le32(out, 0x18);   // __DATA_CONST starts info offset from starts_in_image
    append_le32(out, 0);      // __LINKEDIT
    append_le32(out, 0);      // alignment padding before starts_in_segment

    // dyld_chained_starts_in_segment for __DATA_CONST
    append_le32(out, 0x18);   // size
    append_le16(out, 0x4000); // page_size
    append_le16(out, 6);      // pointer_format
    append_le64(out, 0x4000); // segment_offset
    append_le32(out, 0);      // max_valid_pointer
    append_le16(out, 1);      // page_count
    append_le16(out, 0);      // page_start[0]

    // 2 imports (lib ordinal 1, name offsets 2 and 14 in symbols table)
    append_le32(out, 0x00000201);
    append_le32(out, 0x00000e01);

    // symbols table (imports strings)
    append_u8(out, 0x00);
    append_cstr(out, "_exit");
    append_cstr(out, "_write");
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    REQUIRE(out.size() == 104);
    return out;
}

static std::vector<uint8_t> build_exports_trie_blob() {
    std::vector<uint8_t> out;

    // This is the dyld exports trie for two exports:
    //   __mh_execute_header -> 0x0
    //   _main               -> 0x410
    // Encoded in compact trie form (matching ld output).
    append_u8(out, 0x00);
    append_u8(out, 0x01);
    append_cstr(out, "_");
    append_uleb128(out, 0x12);
    append_u8(out, 0x00);
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    append_u8(out, 0x00);
    append_u8(out, 0x02);

    append_u8(out, 0x00);
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    append_u8(out, 0x03);
    append_u8(out, 0x00);
    append_uleb128(out, 0x410);
    append_u8(out, 0x00);

    append_u8(out, 0x00);
    append_u8(out, 0x02);
    append_cstr(out, "_mh_execute_header");
    append_uleb128(out, 0x09);
    append_cstr(out, "main");
    append_uleb128(out, 0x0d);
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    REQUIRE(out.size() == 48);
    return out;
}

struct SymbolDef {
    std::string name;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

static void build_symbol_and_string_tables(std::vector<uint8_t> &symtab,
        std::vector<uint8_t> &indirect_syms,
        std::vector<uint8_t> &strtab,
        uint64_t msg_len) {
    const std::vector<std::string> name_pool = {
        "__mh_execute_header", "_main", "_exit", "_write", "msg", "msg_len"
    };

    strtab.clear();
    strtab.push_back(0x20); // preserve original leading bytes
    strtab.push_back(0x00);

    std::unordered_map<std::string, uint32_t> strx;
    for (const auto &name : name_pool) {
        strx[name] = (uint32_t)strtab.size();
        append_cstr(strtab, name);
    }
    append_padding_to(strtab, 56);

    const std::vector<SymbolDef> symbols = {
        {"msg",                 0x0e, 3, 0x0000, 0x100000444ULL},
        {"msg_len",             0x02, 0, 0x0000, msg_len},
        {"__mh_execute_header", 0x0f, 1, 0x0010, 0x100000000ULL},
        {"_main",               0x0f, 1, 0x0000, 0x100000410ULL},
        {"_exit",               0x01, 0, 0x0100, 0x0ULL},
        {"_write",              0x01, 0, 0x0100, 0x0ULL},
    };

    symtab.clear();
    for (const auto &s : symbols) {
        append_le32(symtab, strx[s.name]);
        append_u8(symtab, s.n_type);
        append_u8(symtab, s.n_sect);
        append_le16(symtab, s.n_desc);
        append_le64(symtab, s.n_value);
    }
    REQUIRE(symtab.size() == 96);

    indirect_syms.clear();
    append_le32(indirect_syms, 4);
    append_le32(indirect_syms, 5);
    append_le32(indirect_syms, 4);
    append_le32(indirect_syms, 5);
    REQUIRE(indirect_syms.size() == 16);
}

static std::vector<uint8_t> build_function_starts_blob() {
    std::vector<uint8_t> out;
    append_uleb128(out, 0x410); // first function at file offset 1040
    append_u8(out, 0x00);       // terminator
    while (out.size() < 8) append_u8(out, 0x00);
    REQUIRE(out.size() == 8);
    return out;
}

static std::vector<uint8_t> build_code_signature_blob(
        const std::vector<uint8_t> &image, uint32_t code_limit) {
    REQUIRE(code_limit <= image.size());
    const uint32_t page_size = 4096;
    const uint32_t page_shift = 12;
    const std::string ident = "test.x";
    const uint32_t n_code_slots = (code_limit + page_size - 1) / page_size;
    const uint32_t ident_offset = 88;
    const uint32_t hash_offset = ident_offset + (uint32_t)ident.size() + 1;
    const uint32_t cd_len = hash_offset + n_code_slots * CC_SHA256_DIGEST_LENGTH;

    std::vector<std::array<uint8_t, CC_SHA256_DIGEST_LENGTH>> page_hashes;
    page_hashes.reserve(n_code_slots);
    for (uint32_t i = 0; i < n_code_slots; i++) {
        const uint32_t start = i * page_size;
        const uint32_t remain = code_limit - start;
        const uint32_t len = remain < page_size ? remain : page_size;
        std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> digest = {};
        CC_SHA256(&image[start], len, digest.data());
        page_hashes.push_back(digest);
    }

    std::vector<uint8_t> cd;
    append_be32(cd, 0xfade0c02); // CSMAGIC_CODEDIRECTORY
    append_be32(cd, cd_len);     // length
    append_be32(cd, 0x00020400); // version
    append_be32(cd, 0x00020002); // flags
    append_be32(cd, hash_offset); // hashOffset
    append_be32(cd, ident_offset); // identOffset
    append_be32(cd, 0);          // nSpecialSlots
    append_be32(cd, n_code_slots); // nCodeSlots
    append_be32(cd, code_limit); // codeLimit
    append_u8(cd, 32);           // hashSize
    append_u8(cd, 2);            // hashType (SHA-256)
    append_u8(cd, 0);            // platform
    append_u8(cd, page_shift);   // pageSize (2^12)
    append_be32(cd, 0);          // spare2
    append_be32(cd, 0);          // scatterOffset
    append_be32(cd, 0);          // teamOffset

    append_padding_to(cd, 76);
    append_be32(cd, 0x1c);
    append_be32(cd, 0x0);
    append_be32(cd, 0x1);
    REQUIRE(cd.size() == ident_offset);

    append_cstr(cd, ident);
    REQUIRE(cd.size() == hash_offset);

    for (const auto &h : page_hashes) {
        cd.insert(cd.end(), h.begin(), h.end());
    }
    REQUIRE(cd.size() == cd_len);

    std::vector<uint8_t> superblob;
    append_be32(superblob, 0xfade0cc0); // CSMAGIC_EMBEDDED_SIGNATURE
    append_be32(superblob, 20 + cd_len); // length
    append_be32(superblob, 1);          // count
    append_be32(superblob, 0);          // CSSLOT_CODEDIRECTORY
    append_be32(superblob, 20);         // offset of CodeDirectory
    superblob.insert(superblob.end(), cd.begin(), cd.end());
    REQUIRE(superblob.size() == 20 + cd_len);

    append_padding_to(superblob, 408); // LC_CODE_SIGNATURE datasize
    REQUIRE(superblob.size() == 408);
    return superblob;
}

int main() {
    std::cout << "Constructing `data` in memory." << std::endl;

    std::vector<uint8_t> data;
    data.reserve(33512);
    const std::vector<uint8_t> cstr = build_cstring_bytes();
    const uint16_t msg_len = (uint16_t)kMessage.size();

    {
        mach_header_64 header = {
            .magic = MH_MAGIC_64,
            .cputype = CPU_TYPE_ARM64,
            .cpusubtype = 0,
            .filetype = 2,
            .ncmds = 17,
            .sizeofcmds = 976,
            .flags = 2097285,
            .reserved = 0,
        };
        vec_append(data, &header, sizeof(header));
    }

    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 72,
            .segname = "__PAGEZERO",
            .vmaddr = 0,
            .vmsize = 0x100000000ULL,
            .fileoff = 0,
            .filesize = 0,
            .maxprot = 0,
            .initprot = 0,
            .nsects = 0,
            .flags = 0,
        };
        vec_append(data, &s, sizeof(s));
    }

    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 312,
            .segname = "__TEXT",
            .vmaddr = 0x100000000ULL,
            .vmsize = 0x4000,
            .fileoff = 0,
            .filesize = 16384,
            .maxprot = 5,
            .initprot = 5,
            .nsects = 3,
            .flags = 0,
        };
        vec_append(data, &s, sizeof(s));

        section_64 text = {
            .sectname = "__text",
            .segname = "__TEXT",
            .addr = 0x100000410ULL,
            .size = 0x1c,
            .offset = 1040,
            .align = 4,
            .reloff = 0,
            .nreloc = 0,
            .flags = 2147484672,
            .reserved1 = 0,
            .reserved2 = 0,
        };
        vec_append(data, &text, sizeof(text));

        section_64 stubs = {
            .sectname = "__stubs",
            .segname = "__TEXT",
            .addr = 0x10000042cULL,
            .size = 0x18,
            .offset = 1068,
            .align = 2,
            .reloff = 0,
            .nreloc = 0,
            .flags = 2147484680,
            .reserved1 = 0,
            .reserved2 = 12,
        };
        vec_append(data, &stubs, sizeof(stubs));

        section_64 cstring = {
            .sectname = "__cstring",
            .segname = "__TEXT",
            .addr = 0x100000444ULL,
            .size = (uint64_t)cstr.size(),
            .offset = 1092,
            .align = 0,
            .reloff = 0,
            .nreloc = 0,
            .flags = 2,
            .reserved1 = 0,
            .reserved2 = 0,
        };
        vec_append(data, &cstring, sizeof(cstring));
    }

    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 152,
            .segname = "__DATA_CONST",
            .vmaddr = 0x100004000ULL,
            .vmsize = 0x4000,
            .fileoff = 16384,
            .filesize = 16384,
            .maxprot = 3,
            .initprot = 3,
            .nsects = 1,
            .flags = 16,
        };
        vec_append(data, &s, sizeof(s));

        section_64 got = {
            .sectname = "__got",
            .segname = "__DATA_CONST",
            .addr = 0x100004000ULL,
            .size = 0x10,
            .offset = 16384,
            .align = 3,
            .reloff = 0,
            .nreloc = 0,
            .flags = 6,
            .reserved1 = 2,
            .reserved2 = 0,
        };
        vec_append(data, &got, sizeof(got));
    }

    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 72,
            .segname = "__LINKEDIT",
            .vmaddr = 0x100008000ULL,
            .vmsize = 0x4000,
            .fileoff = 32768,
            .filesize = 744,
            .maxprot = 1,
            .initprot = 1,
            .nsects = 0,
            .flags = 0,
        };
        vec_append(data, &s, sizeof(s));
    }

    {
        section_offset_len s = {.cmd = LC_DYLD_CHAINED_FIXUPS, .cmdsize = 16, .offset = 32768, .len = 104};
        vec_append(data, &s, sizeof(s));
    }
    {
        section_offset_len s = {.cmd = LC_DYLD_EXPORTS_TRIE, .cmdsize = 16, .offset = 32872, .len = 48};
        vec_append(data, &s, sizeof(s));
    }
    {
        symtab_command s = {.cmd = LC_SYMTAB, .cmdsize = 24, .symoff = 32928, .nsyms = 6, .stroff = 33040, .strsize = 56};
        vec_append(data, &s, sizeof(s));
    }
    {
        dysymtab_command s = {
            .cmd = LC_DYSYMTAB,
            .cmdsize = 80,
            .ilocalsym = 0,
            .nlocalsym = 2,
            .iextdefsym = 2,
            .nextdefsym = 2,
            .iundefsym = 4,
            .nundefsym = 2,
            .tocoff = 0,
            .ntoc = 0,
            .modtaboff = 0,
            .nmodtab = 0,
            .extrefsymoff = 0,
            .nextrefsyms = 0,
            .indirectsymoff = 33024,
            .nindirectsyms = 4,
            .extreloff = 0,
            .nextrel = 0,
            .locreloff = 0,
            .nlocrel = 0,
        };
        vec_append(data, &s, sizeof(s));
    }
    {
        dylinker_command s = {.cmd = LC_LOAD_DYLINKER, .cmdsize = 32, .name = {.offset = 12}};
        vec_append(data, &s, sizeof(s));
        const char dyld_name[20] = "/usr/lib/dyld";
        vec_append(data, dyld_name, sizeof(dyld_name));
    }
    {
        uuid_command s = {
            .cmd = LC_UUID,
            .cmdsize = 24,
            .uuid = {0x27, 0x07, 0xdd, 0x62, 0x09, 0x67, 0x3c, 0xc0,
                     0xb2, 0xac, 0xef, 0xc3, 0x2b, 0x1c, 0xf6, 0x3a},
        };
        vec_append(data, &s, sizeof(s));
    }
    {
        build_version_command s = {
            .cmd = LC_BUILD_VERSION,
            .cmdsize = 32,
            .platform = 1,
            .minos = 0x000f0700,
            .sdk = 0,
            .ntools = 1,
        };
        vec_append(data, &s, sizeof(s));
        append_le32(data, 3);          // TOOL_LD
        append_le32(data, 0x04ce0100); // tool version
    }
    {
        source_version_command s = {.cmd = LC_SOURCE_VERSION, .cmdsize = 16, .version = 0};
        vec_append(data, &s, sizeof(s));
    }
    {
        entry_point_command s = {.cmd = LC_MAIN, .cmdsize = 24, .entryoff = 1040, .stacksize = 0};
        vec_append(data, &s, sizeof(s));
    }
    {
        dylib_command s = {
            .cmd = LC_LOAD_DYLIB,
            .cmdsize = 56,
            .dylib = {
                .name = {.offset = 24},
                .timestamp = 2,
                .current_version = 0x054c0000,
                .compatibility_version = 0x00010000,
            },
        };
        vec_append(data, &s, sizeof(s));
        const char libsystem_name[32] = "/usr/lib/libSystem.B.dylib";
        vec_append(data, libsystem_name, sizeof(libsystem_name));
    }
    {
        section_offset_len s = {.cmd = LC_FUNCTION_STARTS, .cmdsize = 16, .offset = 32920, .len = 8};
        vec_append(data, &s, sizeof(s));
    }
    {
        section_offset_len s = {.cmd = LC_DATA_IN_CODE, .cmdsize = 16, .offset = 32928, .len = 0};
        vec_append(data, &s, sizeof(s));
    }
    {
        section_offset_len s = {.cmd = LC_CODE_SIGNATURE, .cmdsize = 16, .offset = 33104, .len = 408};
        vec_append(data, &s, sizeof(s));
    }

    REQUIRE(data.size() == 1008);

    append_padding_to(data, 1040);
    std::vector<uint8_t> text = build_text_bytes(msg_len);
    data.insert(data.end(), text.begin(), text.end());

    REQUIRE(data.size() == 1068);
    std::vector<uint8_t> stubs = build_stub_bytes();
    data.insert(data.end(), stubs.begin(), stubs.end());

    REQUIRE(data.size() == 1092);
    data.insert(data.end(), cstr.begin(), cstr.end());

    append_padding_to(data, 16384);
    std::vector<uint8_t> got = build_got_bytes();
    data.insert(data.end(), got.begin(), got.end());

    append_padding_to(data, 32768);

    std::vector<uint8_t> chained_fixups = build_chained_fixups_blob();
    std::vector<uint8_t> exports_trie = build_exports_trie_blob();
    std::vector<uint8_t> function_starts = build_function_starts_blob();
    std::vector<uint8_t> symtab;
    std::vector<uint8_t> indirect_syms;
    std::vector<uint8_t> strtab;
    build_symbol_and_string_tables(symtab, indirect_syms, strtab, msg_len);

    data.insert(data.end(), chained_fixups.begin(), chained_fixups.end());
    data.insert(data.end(), exports_trie.begin(), exports_trie.end());
    data.insert(data.end(), function_starts.begin(), function_starts.end());
    data.insert(data.end(), symtab.begin(), symtab.end());
    data.insert(data.end(), indirect_syms.begin(), indirect_syms.end());
    data.insert(data.end(), strtab.begin(), strtab.end());
    append_padding_to(data, 33104);

    std::vector<uint8_t> codesig = build_code_signature_blob(data, 33104);
    data.insert(data.end(), codesig.begin(), codesig.end());

    REQUIRE(data.size() == 33512);

    std::cout << "Saving to `test2.x`." << std::endl;
    write_file("test2.x", data);
    std::cout << "Done." << std::endl;
}
