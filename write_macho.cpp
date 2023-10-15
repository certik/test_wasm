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

    // Header
    mach_header_64 header = {
        .magic = MH_MAGIC_64,
        .cputype = CPU_TYPE_ARM64,
        .cpusubtype = 0,
        .filetype = 2,
        .ncmds = 17,
        .sizeofcmds = 1056,
        .flags = 2097285,
        .reserved = 0,
    };
    vec_append(data, (uint8_t*)&header, sizeof(header));

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

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 392,
            .segname = "__TEXT",
            .vmaddr = 0x100000000,
            .vmsize = 0x4000,
            .fileoff = 0,
            .filesize = 16384,
            .maxprot = 5,
            .initprot = 5,
            .nsects = 4,
            .flags = 0,
        };
        vec_append(data, (uint8_t*)&segment, sizeof(segment));
    }

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 152,
            .segname = "__DATA_CONST",
            .vmaddr = 0x100004000,
            .vmsize = 0x4000,
            .fileoff = 16384,
            .filesize = 16384,
            .maxprot = 3,
            .initprot = 3,
            .nsects = 1,
            .flags = 16,
        };
        vec_append(data, (uint8_t*)&segment, sizeof(segment));
    }

    {
        // LC_SEGMENT_64
        segment_command_64 segment = {
            .cmd = LC_SEGMENT_64,
            .cmdsize = 72,
            .segname = "__LINKEDIT",
            .vmaddr = 0x100008000,
            .vmsize = 0x4000,
            .fileoff = 32768,
            .filesize = 658,
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
