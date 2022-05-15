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
            segment_command *p = (segment_command*)(&data[idx]);
            std::cout << "    segname: " << p->segname << std::endl;
            std::cout << "    vmaddr: " << p->vmaddr << std::endl;
            std::cout << "    vmsize: " << p->vmsize << std::endl;
            std::cout << "    fileoff: " << p->fileoff << std::endl;
            std::cout << "    filesize: " << p->filesize << std::endl;
            std::cout << "    maxprot: " << p->maxprot << std::endl;
            std::cout << "    initprot: " << p->initprot << std::endl;
            std::cout << "    nsects: " << p->nsects << std::endl;
            std::cout << "    flags: " << p->flags << std::endl;
        } else if (pcmd->cmd == LC_SYMTAB) {
            std::cout << "LC_SYMTAB" << std::endl;
            symtab_command *p = (symtab_command*)(&data[idx]);
            std::cout << "    Number of symbols: " << p->nsyms <<std::endl;
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
