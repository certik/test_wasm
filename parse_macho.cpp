#include <iostream>
#include <vector>

#include "macho_utils.h"

/*
 * The Mach-O format is documented at:
 *
 * https://web.archive.org/web/20090901205800/http://developer.apple.com/mac/library/documentation/DeveloperTools/Conceptual/MachORuntime/Reference/reference.html#//apple_ref/c/tag/segment_command_64
 *
 * Another nice introduction is at:
 *
 * https://h3adsh0tzz.com/2020/01/macho-file-format/
 *
 * This file provides a straightforward self-contained reader (no
 * dependencies).
 *
 * One can print the sections of a Mach-O file using:
 *
 * otool -lV test.x
 *
 * Print machine code in the text section and disassemble:
 *
 * otool -t test.x
 * otool -tv test.x
 *
 * ARM instructions encoding reference manual:
 *
 * https://developer.arm.com/documentation/ddi0406/cb/Application-Level-Architecture/ARM-Instruction-Set-Encoding/ARM-instruction-set-encoding
 *
 */


#define ASSERT(cond)                                                  \
    {                                                                          \
        if (!(cond)) {                                                         \
            std::cerr << "ASSERT failed: " << __FILE__                \
                      << "\nfunction " << __func__ << "(), line number "       \
                      << __LINE__ << " at \n"                                  \
                      << #cond << "\n";                                   \
            abort();                                                           \
        }                                                                      \
    }

void print_bytes(uint8_t *data, size_t size) {
    std::cout << "DATA (" << size << "):";
    for (size_t i=0; i < size; i++) {
        std::cout << " " << std::hex << (uint64_t)data[i] << std::dec;
    }
    std::cout << std::endl;
}

/*
uint32_t static inline string_to_uint32(const char *s) {
    // The cast from signed char to unsigned char is important,
    // otherwise the signed char shifts return wrong value for negative numbers
    const uint8_t *p = (const unsigned char*)s;
    return (((uint32_t)p[0]) << 24) |
           (((uint32_t)p[1]) << 16) |
           (((uint32_t)p[2]) <<  8) |
                       p[3];
}
*/

/*
Section C5.6 A64 Base Instruction Descriptions, Alphabetical list
In ARM Architecture Reference Manual: ARMv8, for ARMv8-A architecture profile
*/
std::string decode_instruction(uint32_t inst) {
    if (inst >> 12 == 0xd65f0) {
        // C5.6.148 RET
        return "ret";
    }
    return "?";
}

void decode_instructions(uint32_t *data, size_t n, uint64_t addr) {
    std::cout << "        Instructions in binary (address + code), equivalent to `otool -t test.x`: " << std::endl;
    for (size_t i=0; i < n; i+=4) {
        std::cout << "            " << std::hex
            << addr + i*4 << " "
            << data[i] << " "
            << data[i+1] << " "
            << data[i+2] << " "
            << data[i+3]
            << std::dec << std::endl;
    }
    std::cout << "        Instructions in asm, equivalent to `otool -tv text.x`: " << std::endl;
    for (size_t i=0; i < n; i++) {
        uint32_t inst = data[i];
        std::cout << "            " << std::setw(2) << i << std::setw(0)
            << " " << std::hex << addr+i*4 << "    "
            << inst
            << std::dec << " " << decode_instruction(inst) << std::endl;
    }
}

int main() {
    std::vector<uint8_t> data;
    read_file("test.x", data);
    std::cout << "File size: " << data.size() << std::endl;
    mach_header_64 *pheader = (mach_header_64*)(&data[0]);
    ASSERT(pheader->magic == MH_MAGIC_64)
    ASSERT(pheader->cputype == CPU_TYPE_ARM64)
    std::cout << "Mach-O Header" << std::endl;
    std::cout << "    magic: " << pheader->magic << std::endl;
    std::cout << "    cputype: " << pheader->cputype << std::endl;
    std::cout << "    cpusubtype: " << pheader->cpusubtype << std::endl;
    std::cout << "    filetype: " << pheader->filetype << std::endl;
    std::cout << "    ncmds: " << pheader->ncmds << std::endl;
    std::cout << "    sizeofcmds: " << pheader->sizeofcmds << std::endl;
    std::cout << "    flags: " << pheader->flags << std::endl;
    std::cout << "    reserved: " << pheader->reserved << std::endl;
    size_t idx = sizeof(mach_header_64);
    for (size_t ncmd=0; ncmd < pheader->ncmds; ncmd++) {
        std::cout << "Load command " << std::setfill(' ')
            << std::setw(2) << ncmd << " ";
        load_command *pcmd = (load_command*)(&data[idx]);
        if (pcmd->cmd == LC_UUID) {
            std::cout << "LC_UUID" << std::endl;
            uuid_command *p = (uuid_command*)(&data[idx]);
            std::cout << "    UUID: " << uuid_to_str(p->uuid) << std::endl;
        } else if (pcmd->cmd == LC_SEGMENT_64) {
            std::cout << "LC_SEGMENT_64" << std::endl;
            segment_command_64 *p = (segment_command_64*)(&data[idx]);
            std::cout << "    cmdsize: " << p->cmdsize << std::endl;
            std::cout << "    segname: " << p->segname << std::endl;
            std::cout << "    vmaddr: 0x" << std::hex << p->vmaddr << std::dec << std::endl;
            std::cout << "    vmsize: 0x" << std::hex << p->vmsize << std::dec << std::endl;
            std::cout << "    fileoff: " << p->fileoff << std::endl;
            std::cout << "    filesize: " << p->filesize << std::endl;
            std::cout << "    maxprot: " << perm2str(p->maxprot) << " (" << p->maxprot << ")" << std::endl;
            std::cout << "    initprot: " << perm2str(p->initprot) << " (" << p->initprot << ")" << std::endl;
            std::cout << "    nsects: " << p->nsects << std::endl;
            std::cout << "    flags: " << p->flags << std::endl;
            for (size_t nsection=0; nsection < p->nsects; nsection++) {
                size_t section_idx = idx + sizeof(segment_command_64) + nsection*sizeof(section_64);
                section_64 *s = (section_64*)(&data[section_idx]);
                std::cout << "    Section " << nsection << std::endl;
                std::cout << "        sectname: " << s->sectname << std::endl;
                std::cout << "        segname: " << s->segname << std::endl;
                std::cout << "        addr: 0x" << std::hex << s->addr << std::dec << std::endl;
                std::cout << "        size: 0x" << std::hex << s->size << std::dec << std::endl;
                std::cout << "        offset: " << s->offset << std::endl;
                std::cout << "        align: " << s->align << std::endl;
                std::cout << "        reloff: " << s->reloff << std::endl;
                std::cout << "        nreloc: " << s->nreloc << std::endl;
                std::cout << "        flags: " << s->flags << std::endl;
                std::cout << "        reserved1: " << s->reserved1 << std::endl;
                std::cout << "        reserved2: " << s->reserved2 << std::endl;

                std::cout << "        ";
                print_bytes(&data[s->offset], s->size);

                if (std::string(s->sectname) == "__cstring") {
                    std::cout << "        C string: \""
                        << std::string((char*)&data[s->offset], s->size)
                        << "\"" << std::endl;
                } else if (std::string(s->sectname) == "__text") {
                    uint64_t n = s->size/4;
                    ASSERT(n*4 == s->size)
                    decode_instructions((uint32_t*)&data[s->offset], n,
                        s->addr);
                }
            }
        } else if (pcmd->cmd == LC_SYMTAB) {
            std::cout << "LC_SYMTAB" << std::endl;
            symtab_command *p = (symtab_command*)(&data[idx]);
            std::cout << "    Number of symbols: " << p->nsyms <<std::endl;
            std::cout << "    symoff: " << p->symoff <<std::endl;
            std::cout << "    stroff: " << p->stroff <<std::endl;
            std::cout << "    strsize: " << p->strsize <<std::endl;
            std::cout << "    stroff/strsize as string: "
                << std::string((char*)&data[p->stroff], p->strsize) << std::endl;;
        } else if (pcmd->cmd == LC_DYSYMTAB) {
            std::cout << "LC_DYSYMTAB" << std::endl;
            dysymtab_command *p = (dysymtab_command*)(&data[idx]);
            std::cout << "    Number of local symbols: " << p->nlocalsym <<std::endl;
        } else if (pcmd->cmd == LC_LOAD_DYLIB) {
            std::cout << "LC_LOAD_DYLIB" << std::endl;
            dylib_command *p = (dylib_command*)(&data[idx]);
            size_t str_idx = idx+p->dylib.name.offset;
            std::string str = (char *)(&data[str_idx]);
            std::cout << "    Dylib name: " << str <<std::endl;
        } else if (pcmd->cmd == LC_LOAD_DYLINKER) {
            std::cout << "LC_LOAD_DYLINKER" << std::endl;
        } else if (pcmd->cmd == LC_CODE_SIGNATURE) {
            std::cout << "LC_CODE_SIGNATURE" << std::endl;
        } else if (pcmd->cmd == LC_FUNCTION_STARTS) {
            std::cout << "LC_FUNCTION_STARTS" << std::endl;
        } else if (pcmd->cmd == LC_DATA_IN_CODE) {
            std::cout << "LC_DATA_IN_CODE" << std::endl;
        } else if (pcmd->cmd == LC_SOURCE_VERSION) {
            std::cout << "LC_SOURCE_VERSION" << std::endl;
        } else if (pcmd->cmd == LC_BUILD_VERSION) {
            std::cout << "LC_BUILD_VERSION" << std::endl;
        } else if (pcmd->cmd == LC_MAIN) {
            std::cout << "LC_MAIN" << std::endl;
        } else if (pcmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            std::cout << "LC_DYLD_EXPORTS_TRIE" << std::endl;
        } else if (pcmd->cmd == LC_DYLD_CHAINED_FIXUPS) {
            std::cout << "LC_DYLD_CHAINED_FIXUPS" << std::endl;
        } else {
            std::cout << "UNKNOWN" << std::endl;
            std::cout << "    type: " << pcmd->cmd << std::endl;
        }

        idx += pcmd->cmdsize;
    }
    std::cout << "Done." << std::endl;
}
