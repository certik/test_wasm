#include <iostream>
#include <fstream>
#include <vector>

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

#define MH_MAGIC_64 0xfeedfacf
#define CPU_ARCH_ABI64 0x01000000
#define CPU_TYPE_ARM 12
#define CPU_TYPE_ARM64 (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#define LC_UUID                27
#define LC_SEGMENT_64          25
#define LC_SYMTAB              2
#define LC_DYSYMTAB            11
#define LC_LOAD_DYLIB          12
#define LC_LOAD_DYLINKER       14
#define LC_CODE_SIGNATURE      29
#define LC_FUNCTION_STARTS     38
#define LC_DATA_IN_CODE        41
#define LC_SOURCE_VERSION      42
#define LC_BUILD_VERSION       50
#define LC_MAIN                2147483688
#define LC_DYLD_EXPORTS_TRIE   2147483699
#define LC_DYLD_CHAINED_FIXUPS 2147483700

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


struct mach_header_64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

struct load_command {
   uint32_t cmd;
   uint32_t cmdsize;
};

struct uuid_command
   {
   uint32_t cmd;
   uint32_t cmdsize;
   uint8_t uuid[16];
};

struct segment_command {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint32_t vmaddr;
    uint32_t vmsize;
    uint32_t fileoff;
    uint32_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
};

struct symtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
};

struct dysymtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
};

union lc_str {
    uint32_t offset;
};

struct dylib {
    union lc_str name;
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compatibility_version;
};

struct dylib_command {
    uint32_t cmd;
    uint32_t cmdsize;
    struct dylib dylib;
};

bool read_file(const std::string &filename, std::vector<uint8_t> &data)
{
    std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary
            | std::ios::ate);

    std::ifstream::pos_type filesize = ifs.tellg();
    if (filesize < 0) return false;

    ifs.seekg(0, std::ios::beg);

    data.resize(filesize);
    char *p = (char*)(&data[0]);
    ifs.read(p, filesize);
    return true;
}

std::string uuid_to_str(uint8_t uuid[16]) {
    char str[37] = {};
    sprintf(str,
    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],
        uuid[15]
    );
    return str;
}

int main() {
    std::vector<uint8_t> data;
    read_file("test.x", data);
    std::cout << data.size() << std::endl;
    mach_header_64 *pheader = (mach_header_64*)(&data[0]);
    ASSERT(pheader->magic == MH_MAGIC_64)
    ASSERT(pheader->cputype == CPU_TYPE_ARM64)
    std::cout << "ncmds: " << pheader->ncmds << std::endl;
    std::cout << "sizeofcmds: " << pheader->sizeofcmds << std::endl;
    size_t idx = sizeof(mach_header_64);
    for (size_t ncmd=0; ncmd < pheader->ncmds; ncmd++) {
        std::cout << "Load command " << ncmd << std::endl;
        load_command *pcmd = (load_command*)(&data[idx]);
        if (pcmd->cmd == LC_UUID) {
            uuid_command *p = (uuid_command*)(&data[idx]);
            std::cout << "  uuid: " << uuid_to_str(p->uuid) << std::endl;
        } else if (pcmd->cmd == LC_SEGMENT_64) {
            segment_command *p = (segment_command*)(&data[idx]);
            std::cout << "  segment: " << p->segname << std::endl;
        } else if (pcmd->cmd == LC_SYMTAB) {
            symtab_command *p = (symtab_command*)(&data[idx]);
            std::cout << "  symtab number of symbols: " << p->nsyms <<std::endl;
        } else if (pcmd->cmd == LC_DYSYMTAB) {
            dysymtab_command *p = (dysymtab_command*)(&data[idx]);
            std::cout << "  dynamic linking symtab number of local symbols: " << p->nlocalsym <<std::endl;
        } else if (pcmd->cmd == LC_LOAD_DYLIB) {
            dylib_command *p = (dylib_command*)(&data[idx]);
            size_t str_idx = idx+p->dylib.name.offset;
            std::string str = (char *)(&data[str_idx]);
            std::cout << "  dylib: " << str <<std::endl;
        } else if (pcmd->cmd == LC_LOAD_DYLINKER) {
            std::cout << "  LC_LOAD_DYLINKER" << std::endl;
        } else if (pcmd->cmd == LC_CODE_SIGNATURE) {
            std::cout << "  LC_CODE_SIGNATURE" << std::endl;
        } else if (pcmd->cmd == LC_FUNCTION_STARTS) {
            std::cout << "  LC_FUNCTION_STARTS" << std::endl;
        } else if (pcmd->cmd == LC_DATA_IN_CODE) {
            std::cout << "  LC_DATA_IN_CODE" << std::endl;
        } else if (pcmd->cmd == LC_SOURCE_VERSION) {
            std::cout << "  LC_SOURCE_VERSION" << std::endl;
        } else if (pcmd->cmd == LC_BUILD_VERSION) {
            std::cout << "  LC_BUILD_VERSION" << std::endl;
        } else if (pcmd->cmd == LC_MAIN) {
            std::cout << "  LC_MAIN" << std::endl;
        } else if (pcmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            std::cout << "  LC_DYLD_EXPORTS_TRIE" << std::endl;
        } else if (pcmd->cmd == LC_DYLD_CHAINED_FIXUPS) {
            std::cout << "  LC_DYLD_CHAINED_FIXUPS" << std::endl;
        } else {
            std::cout << "  type: " << pcmd->cmd << std::endl;
        }

        idx += pcmd->cmdsize;
    }
    std::cout << "Done." << std::endl;
}
