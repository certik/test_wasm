#include <iostream>
#include <vector>
#include <sstream>

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

std::string reg(uint32_t sf, uint32_t Rn, uint32_t zr) {
    if (Rn == 31) {
        if (zr == 1) {
            if (sf == 0) {
                // 32 bit
                return "wzr";
            } else {
                // 64 bit
                return "xzr";
            }
        } else {
            if (sf == 0) {
                // 32 bit
                return "wsp";
            } else {
                // 64 bit
                return "sp";
            }
        }
    } else {
        if (sf == 0) {
            // 32 bit
            return "w" + std::to_string(Rn);
        } else {
            // 64 bit
            return "x" + std::to_string(Rn);
        }
    }
}

std::string hex(uint32_t n) {
    std::stringstream out;
    out << "0x" << std::hex << n;
    return out.str();
}

namespace a64 {
    std::string add(uint32_t sf, uint32_t shift, uint32_t imm12, uint32_t Rn,
            uint32_t Rd) {
        if ((Rd == 0b11111 || Rn == 0b11111) && shift == 0 && imm12 == 0) {
            std::string s = "mov " + reg(sf, Rd, 0) + ", " + reg(sf, Rn, 0);
            return s;
        }
        std::string s = "add " + reg(sf, Rd, 0) + ", " + reg(sf, Rn, 0)
            + ", #" + hex(imm12);
        if (shift > 0) {
            s += ", " + std::to_string(shift);
        }
        return s;
    }
    std::string adds(uint32_t sf, uint32_t shift, uint32_t imm12, uint32_t Rn,
            uint32_t Rd) {
        std::string s = "adds " + reg(sf, Rd, 1) + ", " + reg(sf, Rn, 0)
            + ", #" + hex(imm12);
        if (shift > 0) {
            s += ", " + std::to_string(shift);
        }
        return s;
    }
    std::string sub(uint32_t sf, uint32_t shift, uint32_t imm12, uint32_t Rn,
            uint32_t Rd) {
        std::string s = "sub " + reg(sf, Rd, 0) + ", " + reg(sf, Rn, 0)
            + ", #" + hex(imm12);
        if (shift > 0) {
            s += ", " + std::to_string(shift);
        }
        return s;
    }
    std::string subs(uint32_t sf, uint32_t shift, uint32_t imm12, uint32_t Rn,
            uint32_t Rd) {
        std::string s = "subs " + reg(sf, Rd, 1) + ", " + reg(sf, Rn, 0)
            + ", #" + hex(imm12);
        if (shift > 0) {
            s += ", " + std::to_string(shift);
        }
        return s;
    }

    std::string movn(uint32_t sf, uint32_t hw, uint32_t imm16, uint32_t Rd) {
        uint32_t shift = hw*16;
        std::string s = "movn " + reg(sf, Rd, 1)
            + ", #" + hex(imm16);
        if (shift > 0) {
            s += ", lsl #" + std::to_string(shift);
        }
        return s;
    }

    std::string movz(uint32_t sf, uint32_t hw, uint32_t imm16, uint32_t Rd) {
        uint32_t shift = hw*16;
        if (! (imm16 == 0 && hw != 0)) {
            uint32_t imm = imm16 << shift;
            std::string s = "mov " + reg(sf, Rd, 1)
                + ", #" + hex(imm);
            return s;
        }
        std::string s = "movz " + reg(sf, Rd, 1)
            + ", #" + hex(imm16);
        if (shift > 0) {
            s += ", lsl #" + std::to_string(shift);
        }
        return s;
    }

    std::string movk(uint32_t sf, uint32_t hw, uint32_t imm16, uint32_t Rd) {
        uint32_t shift = hw*16;
        std::string s = "movk " + reg(sf, Rd, 1)
            + ", #" + hex(imm16);
        if (shift > 0) {
            s += ", lsl #" + std::to_string(shift);
        }
        return s;
    }

    std::string svc(uint32_t imm16) {
        std::string s = "svc #" + hex(imm16);
        return s;
    }

    std::string decode_shift(uint32_t shift) {
        if (shift == 0b00) {
            return "lsl";
        } else if (shift == 0b01) {
            return "lsr";
        } else if (shift == 0b10) {
            return "asr";
        } else {
            return "reserved";
        }
    }

    std::string add2(uint32_t sf, uint32_t shift, uint32_t Rm, uint32_t imm6,
            uint32_t Rn, uint32_t Rd) {
        std::string s = "add "
            + reg(sf, Rd, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1);
        if (imm6 != 0) {
            s += ", " + decode_shift(shift) + " #" + std::to_string(imm6);
        }
        return s;
    }

    std::string sub2(uint32_t sf, uint32_t shift, uint32_t Rm, uint32_t imm6,
            uint32_t Rn, uint32_t Rd) {
        std::string s = "sub "
            + reg(sf, Rd, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1);
        if (imm6 != 0) {
            s += ", " + decode_shift(shift) + " #" + std::to_string(imm6);
        }
        return s;
    }

}


/*
Section C5.6 A64 Base Instruction Descriptions, Alphabetical list
In ARM Architecture Reference Manual: ARMv8, for ARMv8-A architecture profile
*/
std::string decode_instruction(uint32_t inst) {
    // C3.1 A64 instruction index by encoding
    if        (((inst >> 25) & 0b1100) == 0b0000) {
        // Unallocated
        return "unallocated";
    } else if (((inst >> 25) & 0b1110) == 0b1000) {
        // C3.4 Data processing - immediate
        if        (((inst >> 23) & 0b110) == 0b000) {
            // C3.4.6 PC-rel. addressing
            return "C3.4.6 PC-rel. addressing";
        } else if (((inst >> 23) & 0b110) == 0b010) {
            // C3.4.1 Add/subtract (immediate)
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm12 = (inst >> 10) & 0b111111111111;
            uint32_t shift = (inst >> 22) & 0b11;
            uint32_t S     = (inst >> 29) & 0b1;
            uint32_t op    = (inst >> 30) & 0b1;
            uint32_t sf    = (inst >> 31) & 0b1;
            if (op == 0) {
                if (S == 0) {
                    return a64::add(sf, shift, imm12, Rn, Rd);
                } else {
                    return a64::adds(sf, shift, imm12, Rn, Rd);
                }
            } else {
                if (S == 0) {
                    return a64::sub(sf, shift, imm12, Rn, Rd);
                } else {
                    return a64::subs(sf, shift, imm12, Rn, Rd);
                }
            }
        } else if (((inst >> 23) & 0b111) == 0b100) {
            // C3.4.4 Logical (immediate)
            return "C3.4.4 Logical (immediate)";
        } else if (((inst >> 23) & 0b111) == 0b101) {
            // C3.4.5 Move wide (immediate)
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t imm16 = (inst >>  5) & 0xffff;
            uint32_t hw    = (inst >> 21) & 0b11;
            uint32_t opc   = (inst >> 29) & 0b11;
            uint32_t sf    = (inst >> 31) & 0b1;
            if (opc == 0b00) {
                return a64::movn(sf, hw, imm16, Rd);
            } else if (opc == 0b10) {
                return a64::movz(sf, hw, imm16, Rd);
            } else if (opc == 0b11) {
                return a64::movk(sf, hw, imm16, Rd);
            }
        } else if (((inst >> 23) & 0b111) == 0b110) {
            // C3.4.2 Bitfield
            return "C3.4.2 Bitfield";
        } else if (((inst >> 23) & 0b111) == 0b111) {
            // C3.4.3 Extract
            return "C3.4.3 Extract";
        }
    } else if (((inst >> 25) & 0b1110) == 0b1010) {
        // Branch, exception generation and system instructions
        // mask:  hex(0b11111111_111_00000000_00000000_111_11)
        // value: hex(0b11010100_000_00000000_00000000_000_01)
        if        ((inst & 0xffe0001f) == 0xd4000001) {
            // svc
            uint32_t imm16 = (inst >>  5) & 0xffff;
            return a64::svc(imm16);
        } else {
            return "Branch, exception generation and system instructions";
        }
    } else if (((inst >> 25) & 0b0101) == 0b0100) {
        // C3.3 Loads and stores
        return "Loads and stores";
    } else if (((inst >> 25) & 0b0111) == 0b0101) {
        if ((inst >> 24) == 0b10001011) {
            // C5.6.5 ADD (shifted register)
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm6  = (inst >> 10) & 0b111111;
            uint32_t Rm    = (inst >> 16) & 0b11111;
            uint32_t shift = (inst >> 22) & 0b11;
            uint32_t sf    = (inst >> 31) & 0b1;
            return a64::add2(sf, shift, Rm, imm6, Rn, Rd);
        } else if ((inst >> 24) == 0b11001011) {
            // C5.6.196 SUB (shifted register)
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm6  = (inst >> 10) & 0b111111;
            uint32_t Rm    = (inst >> 16) & 0b11111;
            uint32_t shift = (inst >> 22) & 0b11;
            uint32_t sf    = (inst >> 31) & 0b1;
            return a64::sub2(sf, shift, Rm, imm6, Rn, Rd);
        }
        return "Data processing - register: " + std::to_string(inst);
    } else if (((inst >> 25) & 0b0111) == 0b0111) {
        return "Data processing - SIMD and floating point";
    } else {
        return "??";
    }
    // TODO: move this up
    if (inst >> 12 == 0xd65f0) {
        // C5.6.148 RET
        return "ret";
    }
    if (inst >> 24 == 0b10101010) {
        // C5.6.125 MOV (register), sf = 1 (64 bit)
        uint32_t Rd = inst & 0b11111;
        uint32_t Rm = (inst >> 16) & 0b11111;
        return "mov x" + std::to_string(Rd) + ", x" + std::to_string(Rm);
    }
    if (inst >> 23 == 0b010100101) {
        // C5.6.123 MOV (wide immediate), sf = 0 (32 bit)
        uint32_t Rd = inst & 0b11111;
        uint32_t imm16 = (inst >> 5) & 0xffff;
        return "mov w" + std::to_string(Rd) + ", #" + std::to_string(imm16);
    }
    if (inst >> 21 == 0b00011011000) {
        // C5.6.133 MUL, sf = 0 (32 bit)
        uint32_t Rd = inst & 0b11111;
        uint32_t Rn = (inst >> 5) & 0b11111;
        uint32_t Ra = (inst >> 10) & 0b11111;
        uint32_t Rm = (inst >> 16) & 0b11111;
        if (Ra == 0b11111) {
            return "mul w" + std::to_string(Rd)
                + ", w" + std::to_string(Rn)
                + ", w" + std::to_string(Rm);
        } else {
            return "madd w" + std::to_string(Rd)
                + ", w" + std::to_string(Rn)
                + ", w" + std::to_string(Rm)
                + ", wzr";
        }
    }
    if (inst >> 24 == 0b01101011) {
        // C5.6.199 SUBS (shifted register), sf = 0 (32 bit)
        uint32_t Rd = inst & 0b11111;
        uint32_t Rn = (inst >> 5) & 0b11111;
        uint32_t imm6 = (inst >> 10) & 0b111111;
        uint32_t Rm = (inst >> 16) & 0b11111;
        uint32_t shift = (inst >> 22) & 0b11;
        std::string shiftstr;
        if (imm6 > 0) {
            shiftstr = ", ";
            if (shift == 0b00) {
                shiftstr += "lsl";
            } else if (shift == 0b01) {
                shiftstr += "lsr";
            } else if (shift == 0b10) {
                shiftstr += "asr";
            } else {
                shiftstr += "reserved";
            }
            shiftstr += " #" + std::to_string(imm6);
        }
        return "subs w" + std::to_string(Rd)
            + ", w" + std::to_string(Rn)
            + ", w" + std::to_string(Rm)
            + shiftstr;
    }
    return "?";
}

void decode_instructions(uint32_t *data, size_t n, uint64_t addr) {
    std::cout << "        Instructions in binary (address + code), equivalent to `otool -t test.x`: " << std::endl;
    for (size_t i=0; i < n; i+=4) {
        std::cout << "            " << std::hex
            << addr + i*4 << " "
            << data[i];
        if (i+1 < n) std::cout <<  " " << data[i+1];
        if (i+2 < n) std::cout <<  " " << data[i+2];
        if (i+3 < n) std::cout <<  " " << data[i+3];
        std::cout << std::dec << std::endl;
    }
    std::cout << "        Instructions in asm, equivalent to `otool -tv test.x`: " << std::endl;
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
