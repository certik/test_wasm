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

    std::cout << "Saving to `test2.x`." << std::endl;

    write_file("test2.x", data);

    std::cout << "Done." << std::endl;
}
