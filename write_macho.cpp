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
    std::vector<uint8_t> data;

    // Header
    mach_header_64 header;
    header.magic = MH_MAGIC_64;
    header.cputype = CPU_TYPE_ARM64;
    header.cpusubtype = 0;
    header.filetype = 2;
    header.ncmds = 17;
    header.sizeofcmds = 1056;
    header.flags = 2097285;
    header.reserved = 0;
    vec_append(data, (uint8_t*)&header, sizeof(header));

    // LC_SEGMENT_64
    segment_command_64 segment;
    segment.cmd = LC_SEGMENT_64;
    segment.cmdsize = 72;
    set_string(segment.segname, "__PAGEZERO");
    segment.vmaddr = 0;
    segment.vmsize = 0x100000000;
    segment.fileoff = 0;
    segment.filesize = 0;
    segment.maxprot = 0;
    segment.initprot = 0;
    segment.nsects = 0;
    segment.flags = 0;
    vec_append(data, (uint8_t*)&segment, sizeof(segment));

    // LC_SEGMENT_64
    segment.cmd = LC_SEGMENT_64;
    segment.cmdsize = 392;
    set_string(segment.segname, "__TEXT");
    segment.vmaddr = 0x100000000;
    segment.vmsize = 0x4000;
    segment.fileoff = 0;
    segment.filesize = 16384;
    segment.maxprot = 5;
    segment.initprot = 5;
    segment.nsects = 4;
    segment.flags = 0;
    vec_append(data, (uint8_t*)&segment, sizeof(segment));

    // LC_SEGMENT_64
    segment.cmd = LC_SEGMENT_64;
    segment.cmdsize = 152;
    set_string(segment.segname, "__DATA_CONST");
    segment.vmaddr = 0x100004000;
    segment.vmsize = 0x4000;
    segment.fileoff = 16384;
    segment.filesize = 16384;
    segment.maxprot = 3;
    segment.initprot = 3;
    segment.nsects = 1;
    segment.flags = 16;
    vec_append(data, (uint8_t*)&segment, sizeof(segment));

    // LC_SEGMENT_64
    segment.cmd = LC_SEGMENT_64;
    segment.cmdsize = 72;
    set_string(segment.segname, "__LINKEDIT");
    segment.vmaddr = 0x100008000;
    segment.vmsize = 0x4000;
    segment.fileoff = 32768;
    segment.filesize = 658;
    segment.maxprot = 1;
    segment.initprot = 1;
    segment.nsects = 0;
    segment.flags = 0;
    vec_append(data, (uint8_t*)&segment, sizeof(segment));

    write_file("test2.x", data);

    std::cout << "Done." << std::endl;
}
