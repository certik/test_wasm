#include <iostream>
#include <vector>

#include "macho_utils.h"

void vec_append(std::vector<uint8_t> &data, uint8_t *item, size_t item_size) {
    for (size_t i=0; i < item_size; i++) {
        data.push_back(item[i]);
    }
}

void set_string(char *data, const std::string &str) {
    for (size_t i=0; i < 16; i++) {
        if (i < str.size()) {
            data[i] = str[i];
        } else {
            data[i] = 0;
        }
    }
}

int main() {
    std::cout << "Constructing `data` in memory." << std::endl;

    std::vector<uint8_t> data;

    {
        // Header
        mach_header_64 header = {
            .magic = MH_MAGIC_64,
            .cputype = CPU_TYPE_ARM64,
            .cpusubtype = 0,
            .filetype = 2,
            .ncmds = 16,
            .sizeofcmds = 744,
            .flags = 2097285,
            .reserved = 0,
        };
        vec_append(data, (uint8_t*)&header, sizeof(header));
    }

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 72,
            .segname = "__PAGEZERO",
            .vmaddr = 0,
            .vmsize = 0x100000000,
            .fileoff = 0,
            .filesize = 0,
            .maxprot = 0,
            .initprot = 0,
            .nsects = 0,
            .flags = 0,
        };
        vec_append(data, (uint8_t*)&segment, sizeof(segment));
    }

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 232,
            .segname = "__TEXT",
            .vmaddr = 0x100000000,
            .vmsize = 0x4000,
            .fileoff = 0,
            .filesize = 16384,
            .maxprot = 5,
            .initprot = 5,
            .nsects = 2,
            .flags = 0,
        };
        vec_append(data, (uint8_t*)&segment, sizeof(segment));

        {
            // Section 0
            section_64 section = {
                .sectname = "__text",
                .segname = "__TEXT",
                .addr = 0x100003fa0,
                .size = 0x14,
                .offset = 16288,
                .align = 4,
                .reloff = 0,
                .nreloc = 0,
                .flags = 2147484672,
                .reserved1 = 0,
                .reserved2 = 0,
            };
            vec_append(data, (uint8_t*)&section, sizeof(section));
        }

        {
            // Section 1
            section_64 section = {
                .sectname = "__unwind_info",
                .segname = "__TEXT",
                .addr = 0x100003fb4,
                .size = 0x48,
                .offset = 16308,
                .align = 2,
                .reloff = 0,
                .nreloc = 0,
                .flags = 0,
                .reserved1 = 0,
                .reserved2 = 0,
            };
            vec_append(data, (uint8_t*)&section, sizeof(section));
        }
    }

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 72,
            .segname = "__LINKEDIT",
            .vmaddr = 0x100004000,
            .vmsize = 0x4000,
            .fileoff = 16384,
            .filesize = 451,
            .maxprot = 1,
            .initprot = 1,
            .nsects = 0,
            .flags = 0,
        };
        vec_append(data, (uint8_t*)&segment, sizeof(segment));
    }

    {
        // LC_DYLD_CHAINED_FIXUPS
        section_offset_len s = {
            .cmd = LC_DYLD_CHAINED_FIXUPS,
            .cmdsize = 16,
            .offset = 16384,
            .len = 56,
        };
        vec_append(data, (uint8_t*)&s, sizeof(s));
    }

    {
        // LC_DYLD_EXPORT_TRIE
        section_offset_len s = {
            .cmd = LC_DYLD_EXPORTS_TRIE,
            .cmdsize = 16,
            .offset = 16440,
            .len = 48,
        };
        vec_append(data, (uint8_t*)&s, sizeof(s));
    }

    {
        // LC_SYMTAB
        symtab_command s = {
            .cmd = LC_SYMTAB,
            .cmdsize = 24,
            .symoff = 16496,
            .nsyms = 2,
            .stroff = 16528,
            .strsize = 32,
        };
        vec_append(data, (uint8_t*)&s, sizeof(s));
    }

    {
        // LC_DYSYMTAB
        dysymtab_command s = {
            .cmd = LC_DYSYMTAB,
            .cmdsize = 80,
            .ilocalsym = 0,
            .nlocalsym = 0,
            .iextdefsym = 0,
            .nextdefsym = 0,
            .iundefsym = 0,
            .nundefsym = 0,
            .tocoff = 0,
            .ntoc = 0,
            .modtaboff = 0,
            .nmodtab = 0,
            .extrefsymoff = 0,
            .nextrefsyms = 0,
            .indirectsymoff = 0,
            .nindirectsyms = 0,
            .extreloff = 0,
            .nextrel = 0,
            .locreloff = 0,
            .nlocrel = 0,
        };
        vec_append(data, (uint8_t*)&s, sizeof(s));
    }

    {
        // LC_LOAD_DYLINKER
        dylinker_command s = {
            .cmd = LC_LOAD_DYLINKER,
            .cmdsize = 32,
            .name.offset = 12
        };
        vec_append(data, (uint8_t*)&s, sizeof(s));

        char text[20] = "/usr/lib/dyld";
        vec_append(data, (uint8_t*)&text, sizeof(text));
    }

    /*
    The following load commands still have to be saved:

Load command  8 LC_UUID
    cmdsize: 24
    expect : 24
    UUID: 844ACFC9-A60A-3347-8BFA-2F39FC580DAE
Load command  9 LC_BUILD_VERSION
    cmdsize: 32
    expect : 24
    platform: 1
    minos   : 12.0.0
    sdk   : 10.17.0
    ntools   : 1
Load command 10 LC_SOURCE_VERSION
    cmdsize: 16
    expect : 16
    version : 0
Load command 11 LC_MAIN
    cmdsize: 24
    expect : 24
    entryoff : 16288
    stacksize: 0
Load command 12 LC_LOAD_DYLIB
    cmdsize: 56
    expect : 24
    Dylib name: /usr/lib/libSystem.B.dylib
Load command 13 LC_FUNCTION_STARTS
    cmdsize: 16
    expect : 16
    dataoff : 16488
    datasize: 8
Load command 14 LC_DATA_IN_CODE
    cmdsize: 16
    expect : 16
    dataoff : 16496
    datasize: 0
Load command 15 LC_CODE_SIGNATURE
    cmdsize: 16
    expect : 16
    dataoff : 16560
    datasize: 275
    */

    std::cout << "Saving to `test2.x`." << std::endl;

    write_file("test2.x", data);

    std::cout << "Done." << std::endl;
}
