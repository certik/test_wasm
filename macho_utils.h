#include <fstream>
#include <vector>

/*
 * This file provides common utilities used for reading and writing Mach-O files
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

struct uuid_command {
   uint32_t cmd;
   uint32_t cmdsize;
   uint8_t uuid[16];
};

struct segment_command_64 {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
};

struct section_64 {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
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

struct section_offset_len {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t offset;
    uint32_t len;
};

struct dyld_chained_fixups_header
{
    uint32_t    fixups_version;
    uint32_t    starts_offset;
    uint32_t    imports_offset;
    uint32_t    symbols_offset;
    uint32_t    imports_count;
    uint32_t    imports_format;
    uint32_t    symbols_format;
};

struct dylinker_command {
   uint32_t cmd;
   uint32_t cmdsize;
   union lc_str name;
};

struct linkedit_data_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t dataoff;
  uint32_t datasize;
};

// ---------------------------------------------------------------------

// Reads a file `filename` to `data`
// Returns `true` for succeess, `false` for fail
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

void write_file(const std::string &filename, const std::vector<uint8_t> &data)
{
    std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary);
    ofs.write((char*)(&data[0]), data.size());
    ofs.close();
}

// Convert 16 byte UUID to a string
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

std::string perm2str(uint32_t perm) {
    std::string s;
    if (perm & 1) {
        s += "r";
    } else {
        s += "-";
    }
    if (perm & (1 << 1)) {
        s += "w";
    } else {
        s += "-";
    }
    if (perm & (1 << 2)) {
        s += "x";
    } else {
        s += "-";
    }
    return s;
}
