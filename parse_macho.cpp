#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>

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
 * Good resource with an example parser:
 *
 * https://github.com/qyang-nj/llios/
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
 * The instruction decoding is typically done using the following template:
 *
 *       } else if ((inst & 0x7fe0fc00) == 0x1ac00800) {
 *           //             sf               Rm            Rn    Rd
 *           // mask:  hex(0b0_11_11111111_00000_11111_1_00000_00000)
 *           // value: hex(0b0_00_11010110_00000_00001_0_00000_00000)
 *           // C5.6.214 UDIV
 *           uint32_t Rd    = (inst >>  0) & 0b11111;
 *           uint32_t Rn    = (inst >>  5) & 0b11111;
 *           uint32_t Rm    = (inst >> 16) & 0b11111;
 *           uint32_t sf    = (inst >> 31) & 0b1;
 *           return a64::udiv(sf, Rm, Rn, Rd);
 *       }
 *
 * In the mask the various variable parts (above: sf, Rm, Rn, Rd) are masked
 * by 0, and non-variable parts by 1. For every 0 in the mask there is 0 in
 * the value; for every 1 in the mask, the value must specify 0 or 1, based on
 * the ARM manual specification for the instruction. The mask and value uniquely
 * determine the instruction. They can be evaluated in Python and converted to a
 * hexadecimal number that is then used in the if condition. The variables are
 * then extracted using a shift and an "and" for a given width. After that we
 * know both the instruction type and all variables and we pass them to a
 * function, such as a64::udiv().
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

#define SignExtend32(x, n) (((((uint32_t)(x)) & (1 << ((n)-1))) == (1 << ((n)-1))) ? (-(((~((uint32_t)(x))) & ((1<<(n))-1)) + 1)) : (uint32_t)(x))

void print_bytes(uint8_t *data, size_t size) {
    std::cout << "DATA (" << size << "):";
    for (size_t i=0; i < size; i++) {
        std::cout << " " << std::hex << (uint64_t)data[i] << std::dec;
    }
    std::cout << std::endl;
}

std::string ascii_or_empty(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    return std::string(s, len);
}

void print_data_range(const std::vector<uint8_t> &data, size_t offset,
        size_t size, const std::string &indent) {
    std::cout << indent << "RAW [" << offset << ", " << (offset + size)
        << ") size=" << size << std::endl;
    ASSERT(offset <= data.size());
    ASSERT(size <= data.size() - offset);
    for (size_t i=0; i < size; i++) {
        if (i % 16 == 0) {
            std::cout << indent << "  +" << std::setw(6) << std::setfill('0')
                << std::hex << i << std::dec << std::setfill(' ') << ":";
        }
        std::cout << " " << std::setw(2) << std::setfill('0') << std::hex
            << (uint64_t)data[offset + i] << std::dec << std::setfill(' ');
        if ((i % 16) == 15 || i + 1 == size) {
            std::cout << std::endl;
        }
    }
}

void maybe_print_data_range(bool raw, const std::vector<uint8_t> &data,
        size_t offset, size_t size, const std::string &indent) {
    if (raw) {
        print_data_range(data, offset, size, indent);
    }
}

std::string version_to_str(uint32_t version) {
    // X.Y.Z is encoded in nibbles xxxx.yy.zz
    uint32_t patch = (version >>  0) & 0xFF;
    uint32_t minor = (version >>  8) & 0xFF;
    uint32_t major = (version >> 16) & 0xFFFF;
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
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
                //return "wsp";
                return "sp";
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

std::string shex(int32_t n) {
    if (n >= 0) {
        return hex(n);
    } else {
        return "-" + hex(-n);
    }
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

    std::string ret() {
        std::string s = "ret";
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

    std::string mul(uint32_t sf, uint32_t Rm, uint32_t Rn, uint32_t Rd) {
        std::string s = "mul "
            + reg(sf, Rd, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1);
        return s;
    }

    std::string madd(uint32_t sf, uint32_t Rm, uint32_t Ra, uint32_t Rn, uint32_t Rd) {
        std::string s = "madd "
            + reg(sf, Rd, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1) + ", "
            + reg(sf, Ra, 1);
        return s;
    }

    std::string udiv(uint32_t sf, uint32_t Rm, uint32_t Rn, uint32_t Rd) {
        std::string s = "udiv "
            + reg(sf, Rd, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1);
        return s;
    }

    std::string str_imm12(uint32_t sf, uint32_t imm12, uint32_t Rn, uint32_t Rt) {
        std::string s = "str "
            + reg(sf, Rt, 1) + ", ["
            + reg(sf, Rn, 0);
        if (imm12 != 0) {
            s = s + ", #" + hex(imm12);
        }
            s = s + "]";
        return s;
    }

    std::string ldr_pimm(uint32_t sf, uint32_t pimm, uint32_t Rn, uint32_t Rt) {
        std::string s = "ldr "
            + reg(sf, Rt, 1) + ", ["
            + reg(1, Rn, 0);
        if (pimm != 0) {
            s = s + ", #" + hex(pimm);
        }
            s = s + "]";
        return s;
    }

    std::string str(uint32_t sf, uint32_t Rt, uint32_t Rn, uint32_t Rm) {
        std::string s = "str "
            + reg(sf, Rt, 1) + ", "
            + reg(sf, Rn, 1) + ", "
            + reg(sf, Rm, 1);
        return s;
    }

    std::string stur(uint32_t sf, int32_t imm9, uint32_t Rn, uint32_t Rt) {
        std::string s = "stur "
            + reg(sf, Rt, 1) + ", ["
            + reg(1, Rn, 0);
        if (imm9 != 0) {
            s = s + ", #" + shex(imm9);
        }
            s = s + "]";
        return s;
    }

    std::string adrp(int32_t imm, uint32_t Rd) {
        std::string s = "adrp " + reg(1, Rd, 1) + ", " + shex(imm)
            + " ; relative offset";
        return s;
    }

    std::string stp(uint32_t sf, uint32_t imm7, uint32_t Rt2, uint32_t Rn, uint32_t Rt) {
        std::string s = "stp "
            + reg(sf, Rt, 1) + ", " + reg(sf, Rt2, 1)
            + ", [" + reg(1, Rn, 0);
        if (imm7 != 0) {
            s = s + ", #" + hex(imm7);
        }
            s = s + "]";
        return s;
    }

    std::string ldp(uint32_t sf, uint32_t imm, uint32_t Rt2, uint32_t Rn, uint32_t Rt) {
        std::string s = "ldp "
            + reg(sf, Rt, 1) + ", " + reg(sf, Rt2, 1)
            + ", [" + reg(1, Rn, 0);
        if (imm != 0) {
            s = s + ", #" + hex(imm);
        }
            s = s + "]";
        return s;
    }

    // Relative offset in bytes
    std::string bl(int32_t offset) {
        std::string s = "bl " + shex(offset) + " ; relative offset";
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
            if ((inst & 0x9f000000) == 0x90000000) {
                //             immlo               immhi         Rd
                // mask:  hex(0b1_00_11111_0000000000000000000_00000)
                // value: hex(0b1_00_10000_0000000000000000000_00000)
                // C5.6.10 ADRP
                uint32_t Rd    = (inst >>  0) & 0b11111;
                uint32_t immhi = (inst >>  5) & ((1<<19)-1);
                uint32_t immlo = (inst >> 29) & 0b11;
                uint32_t imm = immhi << 2 | immlo;
                int32_t  simm = SignExtend32(imm, 21);
                return a64::adrp(simm, Rd);
            }
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
        } else if ((inst & 0xfffffc1f) == 0xd61f0000) {
            // C5.6.26 BR
            uint32_t Rn = (inst >> 5) & 0b11111;
            return "br " + reg(1, Rn, 1);
        } else if (inst >> 12 == 0xd65f0) {
            // C5.6.148 RET
            return a64::ret();
        } else if ((inst & 0xfc000000) == 0x94000000) {
            //                                 imm26
            // mask:  hex(0b1_11111_00000000000000000000000000)
            // value: hex(0b1_00101_00000000000000000000000000)
            // C5.6.26 BL
            uint32_t imm26 = inst & ((1<<26)-1);
            int32_t offset = SignExtend32(imm26, 26);
            int32_t label = offset*4;
            return a64::bl(label);
        } else {
            return "Branch, exception generation and system instructions";
        }
    } else if (((inst >> 25) & 0b0101) == 0b0100) {
        // C3.3 Loads and stores
        if ((inst & 0xbfc00000) == 0xb9000000) {
            //            size                 imm12      Rn    Rt
            // mask:  hex(0b10_111_1_11_11_000000000000_00000_00000)
            // value: hex(0b10_111_0_01_00_000000000000_00000_00000)
            // C5.6.178 STR (immediate, unsigned offset)
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm12 = (inst >> 10) & 0b111111111111;
            uint32_t sf  = (inst >> 30) & 0b1;
            uint32_t size  = 2+sf;
            imm12 = imm12 << size;
            return a64::str_imm12(sf, imm12, Rn, Rt);
        } else if ((inst & 0xbfe00c00) == 0xb8200800) {
            //            size                 Rm  opt S      Rn    Rt
            // mask:  hex(0b10_111_1_11_11_1_00000_000_0_11_00000_00000)
            // value: hex(0b10_111_0_00_00_1_00000_000_0_10_00000_00000)
            // C5.6.179 STR
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            //uint32_t S     = (inst >> 12) & 0b1;
            //uint32_t opt   = (inst >> 13) & 0b111;
            uint32_t Rm    = (inst >> 16) & 0b11111;
            uint32_t size  = (inst >> 30) & 0b1;
            return a64::str(size, Rt, Rn, Rm);
        } else if ((inst & 0xbfe00c00) == 0xb8000000) {
            //              sf                  imm9        Rn    Rt
            // mask:  hex(0b10_111_1_11_11_1_000000000_11_00000_00000)
            // value: hex(0b10_111_0_00_00_0_000000000_00_00000_00000)
            // C5.6.187 STUR
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm9  = (inst >> 12) & 0b111111111;
            int32_t  simm9 = SignExtend32(imm9, 9);
            uint32_t sf    = (inst >> 30) & 0b1;
            return a64::stur(sf, simm9, Rn, Rt);
        } else if ((inst & 0x7fc00000) == 0x29000000) {
            //             sf                imm7   Rt2    Rn    Rt
            // mask:  hex(0b01_111_1_111_1_0000000_00000_00000_00000)
            // value: hex(0b00_101_0_010_0_0000000_00000_00000_00000)
            // C5.6.177 STP
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t Rt2   = (inst >> 10) & 0b11111;
            uint32_t imm7  = (inst >> 15) & 0b1111111;
            uint32_t sf    = (inst >> 31) & 0b1;
            imm7 = imm7 << 3;
            return a64::stp(sf, imm7, Rt2, Rn, Rt);
        } else if ((inst & 0x7fc00000) == 0x29400000) {
            //             sf                imm7   Rt2    Rn    Rt
            // mask:  hex(0b01_111_1_111_1_0000000_00000_00000_00000)
            // value: hex(0b00_101_0_010_1_0000000_00000_00000_00000)
            // C5.6.81 LDP
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t Rt2   = (inst >> 10) & 0b11111;
            uint32_t imm7  = (inst >> 15) & 0b1111111;
            uint32_t sf    = (inst >> 31) & 0b1;
            imm7 = imm7 << 3;
            return a64::ldp(sf, imm7, Rt2, Rn, Rt);
        } else if ((inst & 0xbfc00000) == 0xb9400000) {
            //              sf                 imm12      Rn    Rt
            // mask:  hex(0b10_111_1_11_11_000000000000_00000_00000)
            // value: hex(0b10_111_0_01_01_000000000000_00000_00000)
            // C5.6.83 LDR (immediate, unsigned offset)
            uint32_t Rt    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t imm12 = (inst >> 10) & 0b111111111111;
            uint32_t sf  = (inst >> 30) & 0b1;
            uint32_t size  = 2+sf;
            imm12 = imm12 << size;
            return a64::ldr_pimm(sf, imm12, Rn, Rt);
        }
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
        } else if ((inst & 0x7fe08000) == 0x1b000000) {
            //             sf                Rm      Ra    Rn    Rd
            // mask:  hex(0b0_11_11111_111_00000_1_00000_00000_00000)
            // value: hex(0b0_00_11011_000_00000_0_00000_00000_00000)
            // C5.6.119 MADD
            // C5.6.133 MUL
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t Ra    = (inst >> 10) & 0b11111;
            uint32_t Rm    = (inst >> 16) & 0b11111;
            uint32_t sf    = (inst >> 31) & 0b1;
            if (Ra == 0b11111) {
                return a64::mul(sf, Rm, Rn, Rd);
            } else {
                return a64::madd(sf, Rm, Ra, Rn, Rd);
            }
        } else if ((inst & 0x7fe0fc00) == 0x1ac00800) {
            //             sf               Rm            Rn    Rd
            // mask:  hex(0b0_11_11111111_00000_11111_1_00000_00000)
            // value: hex(0b0_00_11010110_00000_00001_0_00000_00000)
            // C5.6.214 UDIV
            uint32_t Rd    = (inst >>  0) & 0b11111;
            uint32_t Rn    = (inst >>  5) & 0b11111;
            uint32_t Rm    = (inst >> 16) & 0b11111;
            uint32_t sf    = (inst >> 31) & 0b1;
            return a64::udiv(sf, Rm, Rn, Rd);
        }
        return "Data processing - register: " + std::to_string(inst);
    } else if (((inst >> 25) & 0b0111) == 0b0111) {
        return "Data processing - SIMD and floating point";
    } else {
        return "??";
    }
    // TODO: move this up
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

uint32_t read_u32(const std::vector<uint8_t> &data, size_t offset) {
    ASSERT(offset + 4 <= data.size())
    return (uint32_t)data[offset]
        | ((uint32_t)data[offset + 1] << 8)
        | ((uint32_t)data[offset + 2] << 16)
        | ((uint32_t)data[offset + 3] << 24);
}

uint32_t read_be_u32(const std::vector<uint8_t> &data, size_t offset) {
    ASSERT(offset + 4 <= data.size())
    return ((uint32_t)data[offset] << 24)
        | ((uint32_t)data[offset + 1] << 16)
        | ((uint32_t)data[offset + 2] << 8)
        | (uint32_t)data[offset + 3];
}

uint64_t read_u64(const std::vector<uint8_t> &data, size_t offset) {
    ASSERT(offset + 8 <= data.size())
    return (uint64_t)data[offset]
        | ((uint64_t)data[offset + 1] << 8)
        | ((uint64_t)data[offset + 2] << 16)
        | ((uint64_t)data[offset + 3] << 24)
        | ((uint64_t)data[offset + 4] << 32)
        | ((uint64_t)data[offset + 5] << 40)
        | ((uint64_t)data[offset + 6] << 48)
        | ((uint64_t)data[offset + 7] << 56);
}

uint64_t read_uleb128(const std::vector<uint8_t> &data, size_t begin, size_t end,
        size_t &cursor) {
    ASSERT(cursor >= begin)
    ASSERT(cursor <= end)
    uint64_t value = 0;
    unsigned shift = 0;
    while (cursor < end) {
        uint8_t byte = data[cursor++];
        value |= ((uint64_t)(byte & 0x7f)) << shift;
        if ((byte & 0x80) == 0) {
            return value;
        }
        shift += 7;
        ASSERT(shift < 64)
    }
    ASSERT(false && "invalid uleb128")
    return 0;
}

void decode_cstring_section(const std::vector<uint8_t> &data, uint32_t offset,
        uint64_t size, uint64_t addr) {
    std::cout << "        C strings:" << std::endl;
    size_t start = offset;
    size_t end = offset + size;
    size_t i = start;
    size_t idx = 0;
    while (i < end) {
        size_t s = i;
        while (i < end && data[i] != '\0') i++;
        std::string value((const char*)&data[s], i - s);
        std::cout << "            [" << idx << "] addr=0x" << std::hex
            << (addr + (s - start)) << std::dec << " \"" << value << "\""
            << std::endl;
        idx++;
        while (i < end && data[i] == '\0') i++;
    }
}

void decode_pointer_section(const std::vector<uint8_t> &data, uint32_t offset,
        uint64_t size, uint64_t addr) {
    if ((size % 8) != 0) {
        std::cout << "        Pointer decode skipped: size not multiple of 8"
            << std::endl;
        return;
    }
    size_t n = size / 8;
    std::cout << "        Pointers (" << n << "):" << std::endl;
    for (size_t i = 0; i < n; i++) {
        uint64_t value = read_u64(data, offset + i * 8);
        std::cout << "            [" << i << "] addr=0x" << std::hex
            << (addr + i * 8) << " -> 0x" << value << std::dec << std::endl;
    }
}

void decode_relocations(const std::vector<uint8_t> &data, uint32_t reloff,
        uint32_t nreloc, uint64_t section_addr) {
    if (nreloc == 0) return;
    std::cout << "        Relocations (" << nreloc << "):" << std::endl;
    for (size_t i = 0; i < nreloc; i++) {
        size_t r = reloff + i * 8;
        uint32_t w0 = read_u32(data, r + 0);
        uint32_t w1 = read_u32(data, r + 4);
        int32_t r_address = (int32_t)w0;
        uint32_t r_symbolnum = w1 & 0x00ffffff;
        uint32_t r_pcrel = (w1 >> 24) & 0x1;
        uint32_t r_length = (w1 >> 25) & 0x3;
        uint32_t r_extern = (w1 >> 27) & 0x1;
        uint32_t r_type = (w1 >> 28) & 0xf;
        std::cout << "            [" << i << "] r_address=" << r_address
            << " (vmaddr=0x" << std::hex << (section_addr + r_address)
            << std::dec << ") r_symbolnum=" << r_symbolnum
            << " r_pcrel=" << r_pcrel
            << " r_length=" << r_length
            << " r_extern=" << r_extern
            << " r_type=" << r_type
            << std::endl;
    }
}

void decode_literal_words(const std::vector<uint8_t> &data, uint32_t offset,
        uint64_t size, uint64_t addr, uint32_t width) {
    ASSERT(width == 4 || width == 8)
    if (size % width != 0) {
        std::cout << "        Literal decode skipped: size not multiple of "
            << width << std::endl;
        return;
    }
    size_t n = size / width;
    std::cout << "        Literal " << (width * 8) << "-bit values (" << n
        << "):" << std::endl;
    for (size_t i = 0; i < n; i++) {
        if (width == 4) {
            uint32_t value = read_u32(data, offset + i * width);
            std::cout << "            [" << i << "] addr=0x" << std::hex
                << (addr + i * width) << " value=0x" << value << std::dec
                << std::endl;
        } else {
            uint64_t value = read_u64(data, offset + i * width);
            std::cout << "            [" << i << "] addr=0x" << std::hex
                << (addr + i * width) << " value=0x" << value << std::dec
                << std::endl;
        }
    }
}

std::string cs_magic_to_str(uint32_t magic) {
    if (magic == 0xfade0cc0) return "CSMAGIC_EMBEDDED_SIGNATURE";
    if (magic == 0xfade0c02) return "CSMAGIC_CODEDIRECTORY";
    if (magic == 0xfade0c01) return "CSMAGIC_REQUIREMENTS";
    if (magic == 0xfade7171) return "CSMAGIC_BLOBWRAPPER";
    if (magic == 0xfade0b01) return "CSMAGIC_EMBEDDED_ENTITLEMENTS";
    return "UNKNOWN";
}

std::string cs_slot_to_str(uint32_t slot_type) {
    if (slot_type == 0) return "CSSLOT_CODEDIRECTORY";
    if (slot_type == 1) return "CSSLOT_INFOSLOT";
    if (slot_type == 2) return "CSSLOT_REQUIREMENTS";
    if (slot_type == 3) return "CSSLOT_RESOURCEDIR";
    if (slot_type == 4) return "CSSLOT_APPLICATION";
    if (slot_type == 5) return "CSSLOT_ENTITLEMENTS";
    if (slot_type == 7) return "CSSLOT_DER_ENTITLEMENTS";
    return "UNKNOWN_SLOT";
}

std::string dic_kind_to_str(uint16_t kind) {
    if (kind == 1) return "DATA";
    if (kind == 2) return "JUMP_TABLE8";
    if (kind == 3) return "JUMP_TABLE16";
    if (kind == 4) return "JUMP_TABLE32";
    if (kind == 5) return "ABS_JUMP_TABLE32";
    return "UNKNOWN";
}

void decode_code_directory_blob(const std::vector<uint8_t> &data, size_t off,
        size_t limit, const std::string &indent) {
    ASSERT(off + 44 <= limit)
    uint32_t magic = read_be_u32(data, off + 0);
    uint32_t length = read_be_u32(data, off + 4);
    ASSERT(off + length <= limit)
    uint32_t version = read_be_u32(data, off + 8);
    uint32_t flags = read_be_u32(data, off + 12);
    uint32_t hashOffset = read_be_u32(data, off + 16);
    uint32_t identOffset = read_be_u32(data, off + 20);
    uint32_t nSpecialSlots = read_be_u32(data, off + 24);
    uint32_t nCodeSlots = read_be_u32(data, off + 28);
    uint32_t codeLimit = read_be_u32(data, off + 32);
    uint8_t hashSize = data[off + 36];
    uint8_t hashType = data[off + 37];
    uint8_t platform = data[off + 38];
    uint8_t pageSize = data[off + 39];
    uint32_t spare2 = read_be_u32(data, off + 40);

    std::cout << indent << "magic      : 0x" << std::hex << magic
        << std::dec << " (" << cs_magic_to_str(magic) << ")" << std::endl;
    std::cout << indent << "length     : " << length << std::endl;
    std::cout << indent << "version    : 0x" << std::hex << version
        << std::dec << std::endl;
    std::cout << indent << "flags      : 0x" << std::hex << flags
        << std::dec << std::endl;
    std::cout << indent << "hashOffset : " << hashOffset << std::endl;
    std::cout << indent << "identOffset: " << identOffset << std::endl;
    std::cout << indent << "nSpecialSlots: " << nSpecialSlots << std::endl;
    std::cout << indent << "nCodeSlots   : " << nCodeSlots << std::endl;
    std::cout << indent << "codeLimit    : " << codeLimit << std::endl;
    std::cout << indent << "hashSize/hashType/platform/pageSize: "
        << (uint32_t)hashSize << "/" << (uint32_t)hashType
        << "/" << (uint32_t)platform << "/" << (uint32_t)pageSize
        << std::endl;
    std::cout << indent << "spare2      : " << spare2 << std::endl;

    if (length >= 48) {
        uint32_t scatterOffset = read_be_u32(data, off + 44);
        std::cout << indent << "scatterOffset: " << scatterOffset << std::endl;
    }
    if (length >= 52) {
        uint32_t teamOffset = read_be_u32(data, off + 48);
        std::cout << indent << "teamOffset   : " << teamOffset << std::endl;
    }

    if (identOffset > 0 && identOffset < length) {
        size_t ident_start = off + identOffset;
        size_t ident_end = off + length;
        size_t cur = ident_start;
        while (cur < ident_end && data[cur] != '\0') cur++;
        std::string ident((const char*)&data[ident_start], cur - ident_start);
        std::cout << indent << "identifier: " << ident << std::endl;
    }
}

void decode_code_signature(const std::vector<uint8_t> &data, uint32_t dataoff,
        uint32_t datasize) {
    std::cout << "    decoded code signature:" << std::endl;
    if (datasize == 0) {
        std::cout << "        (empty)" << std::endl;
        return;
    }
    ASSERT(dataoff <= data.size())
    ASSERT(datasize <= data.size() - dataoff)
    size_t begin = dataoff;
    size_t end = dataoff + datasize;
    ASSERT(begin + 12 <= end)
    uint32_t magic = read_be_u32(data, begin + 0);
    uint32_t length = read_be_u32(data, begin + 4);
    std::cout << "        superblob magic : 0x" << std::hex << magic << std::dec
        << " (" << cs_magic_to_str(magic) << ")" << std::endl;
    std::cout << "        superblob length: " << length << std::endl;
    if (magic != 0xfade0cc0) {
        std::cout << "        not an embedded signature superblob" << std::endl;
        return;
    }
    uint32_t count = read_be_u32(data, begin + 8);
    std::cout << "        blob count      : " << count << std::endl;
    ASSERT(begin + 12 + (size_t)count * 8 <= end)
    for (size_t i = 0; i < count; i++) {
        size_t entry = begin + 12 + i * 8;
        uint32_t slot_type = read_be_u32(data, entry + 0);
        uint32_t slot_off = read_be_u32(data, entry + 4);
        std::cout << "        blob[" << i << "] type=" << slot_type << " ("
            << cs_slot_to_str(slot_type) << ")"
            << " offset=" << slot_off << std::endl;
        if (slot_off >= datasize) {
            std::cout << "            invalid offset" << std::endl;
            continue;
        }
        size_t blob = begin + slot_off;
        ASSERT(blob + 8 <= end)
        uint32_t blob_magic = read_be_u32(data, blob + 0);
        uint32_t blob_length = read_be_u32(data, blob + 4);
        std::cout << "            magic : 0x" << std::hex << blob_magic
            << std::dec << " (" << cs_magic_to_str(blob_magic) << ")"
            << std::endl;
        std::cout << "            length: " << blob_length << std::endl;
        if (blob + blob_length > end) {
            std::cout << "            invalid blob length" << std::endl;
            continue;
        }
        if (blob_magic == 0xfade0c02) {
            decode_code_directory_blob(data, blob, end, "            ");
        }
    }
}
struct nlist_64_local {
    uint32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

int main(int argc, char **argv) {
    bool raw_dump = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--raw") {
            raw_dump = true;
        } else {
            std::cerr << "Usage: " << argv[0] << " [--raw]" << std::endl;
            return 1;
        }
    }

    std::vector<uint8_t> data;
    read_file("test.x", data);
    ASSERT(data.size() >= sizeof(mach_header_64))
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
    maybe_print_data_range(raw_dump, data, 0, sizeof(mach_header_64), "    ");
    uint64_t image_vmaddr_base = 0;
    bool image_vmaddr_base_valid = false;
    size_t idx = sizeof(mach_header_64);
    for (size_t ncmd=0; ncmd < pheader->ncmds; ncmd++) {
        ASSERT(idx + sizeof(load_command) <= data.size())
        load_command *pcmd = (load_command*)(&data[idx]);
        ASSERT(pcmd->cmdsize >= sizeof(load_command))
        ASSERT(idx + pcmd->cmdsize <= data.size())

        std::cout << "Load command " << std::setfill(' ')
            << std::setw(2) << ncmd << " ";
        std::cout << "(offset=" << idx << ") ";
        if (pcmd->cmd == LC_UUID) {
            std::cout << "LC_UUID" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(uuid_command) << std::endl;
            uuid_command *p = (uuid_command*)(&data[idx]);
            std::cout << "    UUID: " << uuid_to_str(p->uuid) << std::endl;
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
        } else if (pcmd->cmd == LC_SEGMENT_64) {
            std::cout << "LC_SEGMENT_64" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            segment_command_64 *p = (segment_command_64*)(&data[idx]);
            std::cout << "    expect : " << sizeof(segment_command_64) + p->nsects*sizeof(section_64) << std::endl;
            std::cout << "    segname: " << ascii_or_empty(p->segname, sizeof(p->segname)) << std::endl;
            std::cout << "    vmaddr: 0x" << std::hex << p->vmaddr << std::dec << std::endl;
            std::cout << "    vmsize: 0x" << std::hex << p->vmsize << std::dec << std::endl;
            std::cout << "    fileoff: " << p->fileoff << std::endl;
            std::cout << "    filesize: " << p->filesize << std::endl;
            std::cout << "    maxprot: " << perm2str(p->maxprot) << " (" << p->maxprot << ")" << std::endl;
            std::cout << "    initprot: " << perm2str(p->initprot) << " (" << p->initprot << ")" << std::endl;
            std::cout << "    nsects: " << p->nsects << std::endl;
            std::cout << "    flags: " << p->flags << std::endl;
            if (!image_vmaddr_base_valid
                    && ascii_or_empty(p->segname, sizeof(p->segname)) == "__TEXT") {
                image_vmaddr_base = p->vmaddr;
                image_vmaddr_base_valid = true;
            }
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            if (p->filesize > 0) {
                ASSERT(p->fileoff <= data.size())
                ASSERT(p->filesize <= data.size() - p->fileoff)
                maybe_print_data_range(raw_dump, data, p->fileoff, p->filesize, "    ");
            }
            for (size_t nsection=0; nsection < p->nsects; nsection++) {
                size_t section_idx = idx + sizeof(segment_command_64) + nsection*sizeof(section_64);
                section_64 *s = (section_64*)(&data[section_idx]);
                std::cout << "    Section " << nsection << std::endl;
                std::string sectname = ascii_or_empty(s->sectname, sizeof(s->sectname));
                std::string segname = ascii_or_empty(s->segname, sizeof(s->segname));
                std::cout << "        sectname: " << sectname << std::endl;
                std::cout << "        segname: " << segname << std::endl;
                std::cout << "        addr: 0x" << std::hex << s->addr << std::dec << std::endl;
                std::cout << "        size: 0x" << std::hex << s->size << std::dec << std::endl;
                std::cout << "        offset: " << s->offset << std::endl;
                std::cout << "        align: " << s->align << std::endl;
                std::cout << "        reloff: " << s->reloff << std::endl;
                std::cout << "        nreloc: " << s->nreloc << std::endl;
                std::cout << "        flags: " << s->flags << std::endl;
                std::cout << "        reserved1: " << s->reserved1 << std::endl;
                std::cout << "        reserved2: " << s->reserved2 << std::endl;
                uint32_t section_type = s->flags & 0xFF;
                bool is_zerofill = section_type == 0x1;
                if (is_zerofill) {
                    std::cout << "        DATA: zerofill section (not stored in file)" << std::endl;
                } else if (s->size > 0) {
                    ASSERT(s->offset <= data.size())
                    ASSERT(s->size <= data.size() - s->offset)
                    maybe_print_data_range(raw_dump, data, s->offset, s->size, "        ");
                }

                if (s->nreloc > 0) {
                    size_t reloc_size = s->nreloc * 8;
                    ASSERT(s->reloff <= data.size())
                    ASSERT(reloc_size <= data.size() - s->reloff)
                    maybe_print_data_range(raw_dump, data, s->reloff, reloc_size, "        ");
                    decode_relocations(data, s->reloff, s->nreloc, s->addr);
                }

                if (!is_zerofill && s->size > 0) {
                    uint32_t stype = section_type;
                    if (stype == 2 || sectname == "__cstring") {
                        decode_cstring_section(data, s->offset, s->size, s->addr);
                    } else if (stype == 8 || sectname == "__text"
                            || sectname == "__stubs"
                            || sectname == "__stub_helper") {
                        if (sectname == "__stubs" && s->reserved2 > 0
                                && s->size % s->reserved2 == 0) {
                            size_t nstubs = s->size / s->reserved2;
                            std::cout << "        Symbol stubs (" << nstubs
                                << "), stub size: " << s->reserved2 << std::endl;
                            for (size_t i = 0; i < nstubs; i++) {
                                size_t stub_off = s->offset + i * s->reserved2;
                                uint64_t stub_addr = s->addr + i * s->reserved2;
                                if (s->reserved2 % 4 == 0) {
                                    size_t ninstr = s->reserved2 / 4;
                                    std::cout << "        Stub " << i << ":" << std::endl;
                                    decode_instructions((uint32_t*)&data[stub_off],
                                        ninstr, stub_addr);
                                }
                            }
                        } else {
                            uint64_t n = s->size / 4;
                            ASSERT(n * 4 == s->size)
                            decode_instructions((uint32_t*)&data[s->offset], n,
                                s->addr);
                        }
                    } else if (stype == 6 || stype == 7 || stype == 9
                            || stype == 10 || sectname == "__got"
                            || sectname == "__la_symbol_ptr") {
                        decode_pointer_section(data, s->offset, s->size, s->addr);
                    } else if (stype == 4) {
                        decode_literal_words(data, s->offset, s->size, s->addr, 8);
                    } else if (stype == 3) {
                        decode_literal_words(data, s->offset, s->size, s->addr, 4);
                    } else {
                        // Generic fallback for any unhandled section type.
                        decode_literal_words(data, s->offset, s->size, s->addr, 4);
                    }
                }
            }
        } else if (pcmd->cmd == LC_SYMTAB) {
            std::cout << "LC_SYMTAB" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(symtab_command) << std::endl;
            symtab_command *p = (symtab_command*)(&data[idx]);
            std::cout << "    Number of symbols: " << p->nsyms <<std::endl;
            std::cout << "    symoff: " << p->symoff <<std::endl;
            std::cout << "    stroff: " << p->stroff <<std::endl;
            std::cout << "    strsize: " << p->strsize <<std::endl;
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            ASSERT(p->stroff <= data.size())
            ASSERT(p->strsize <= data.size() - p->stroff)
            maybe_print_data_range(raw_dump, data, p->stroff, p->strsize, "    ");
            size_t symtab_size = p->nsyms * sizeof(nlist_64_local);
            ASSERT(p->symoff <= data.size())
            ASSERT(symtab_size <= data.size() - p->symoff)
            maybe_print_data_range(raw_dump, data, p->symoff, symtab_size, "    ");
            nlist_64_local *syms = (nlist_64_local*)(&data[p->symoff]);
            for (size_t i=0; i < p->nsyms; i++) {
                nlist_64_local *sym = &syms[i];
                std::string name = "<bad n_strx>";
                if (sym->n_strx < p->strsize) {
                    const char *name_ptr = (const char*)&data[p->stroff + sym->n_strx];
                    size_t max_len = p->strsize - sym->n_strx;
                    name = ascii_or_empty(name_ptr, max_len);
                }
                std::cout << "    Symbol " << i << std::endl;
                std::cout << "        n_strx : " << sym->n_strx << std::endl;
                std::cout << "        n_type : 0x" << std::hex << (uint32_t)sym->n_type << std::dec << std::endl;
                std::cout << "        n_sect : " << (uint32_t)sym->n_sect << std::endl;
                std::cout << "        n_desc : 0x" << std::hex << sym->n_desc << std::dec << std::endl;
                std::cout << "        n_value: 0x" << std::hex << sym->n_value << std::dec << std::endl;
                std::cout << "        name   : " << name << std::endl;
            }
        } else if (pcmd->cmd == LC_DYSYMTAB) {
            std::cout << "LC_DYSYMTAB" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(dysymtab_command) << std::endl;
            dysymtab_command *p = (dysymtab_command*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    Number of local symbols: " << p->nlocalsym <<std::endl;
            std::cout << "    Number of external defined symbols: " << p->nextdefsym <<std::endl;
            std::cout << "    Number of undefined symbols: " << p->nundefsym <<std::endl;
            if (p->indirectsymoff != 0 && p->nindirectsyms != 0) {
                size_t nbytes = p->nindirectsyms * sizeof(uint32_t);
                ASSERT(p->indirectsymoff <= data.size())
                ASSERT(nbytes <= data.size() - p->indirectsymoff)
                maybe_print_data_range(raw_dump, data, p->indirectsymoff, nbytes, "    ");
            }
            if (p->extreloff != 0 && p->nextrel != 0) {
                size_t nbytes = p->nextrel * 8;
                ASSERT(p->extreloff <= data.size())
                ASSERT(nbytes <= data.size() - p->extreloff)
                maybe_print_data_range(raw_dump, data, p->extreloff, nbytes, "    ");
            }
            if (p->locreloff != 0 && p->nlocrel != 0) {
                size_t nbytes = p->nlocrel * 8;
                ASSERT(p->locreloff <= data.size())
                ASSERT(nbytes <= data.size() - p->locreloff)
                maybe_print_data_range(raw_dump, data, p->locreloff, nbytes, "    ");
            }
        } else if (pcmd->cmd == LC_LOAD_DYLIB) {
            std::cout << "LC_LOAD_DYLIB" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(dylib_command) << std::endl;
            dylib_command *p = (dylib_command*)(&data[idx]);
            size_t str_idx = idx+p->dylib.name.offset;
            std::string str = (char *)(&data[str_idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    Dylib name: " << str <<std::endl;
            std::cout << "    timestamp: " << p->dylib.timestamp <<std::endl;
            std::cout << "    current_version: " << version_to_str(p->dylib.current_version) <<std::endl;
            std::cout << "    compatibility_version: " << version_to_str(p->dylib.compatibility_version) <<std::endl;
        } else if (pcmd->cmd == LC_LOAD_DYLINKER) {
            std::cout << "LC_LOAD_DYLINKER" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(dylinker_command) << std::endl;
            dylinker_command *p = (dylinker_command*)(&data[idx]);
            size_t str_idx = idx+p->name.offset;
            std::string str = (char *)(&data[str_idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    name offset: " << std::to_string(p->name.offset) << std::endl;
            std::cout << "    name: " << str <<std::endl;
        } else if (pcmd->cmd == LC_CODE_SIGNATURE) {
            std::cout << "LC_CODE_SIGNATURE" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(section_offset_len) << std::endl;
            section_offset_len *p = (section_offset_len*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    dataoff : " << std::to_string(p->offset) << std::endl;
            std::cout << "    datasize: " << std::to_string(p->len) << std::endl;
            if (p->len > 0) {
                ASSERT(p->offset <= data.size())
                ASSERT(p->len <= data.size() - p->offset)
                maybe_print_data_range(raw_dump, data, p->offset, p->len, "    ");
                decode_code_signature(data, p->offset, p->len);
            } else {
                std::cout << "    decoded code signature: (empty)" << std::endl;
            }
        } else if (pcmd->cmd == LC_FUNCTION_STARTS) {
            std::cout << "LC_FUNCTION_STARTS" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(section_offset_len) << std::endl;
            section_offset_len *p = (section_offset_len*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    dataoff : " << std::to_string(p->offset) << std::endl;
            std::cout << "    datasize: " << std::to_string(p->len) << std::endl;
            if (p->len > 0) {
                ASSERT(p->offset <= data.size())
                ASSERT(p->len <= data.size() - p->offset)
                maybe_print_data_range(raw_dump, data, p->offset, p->len, "    ");
                std::cout << "    decoded function starts:" << std::endl;
                size_t begin = p->offset;
                size_t end = p->offset + p->len;
                size_t cursor = begin;
                uint64_t func_off = 0;
                size_t nfunc = 0;
                while (cursor < end) {
                    uint64_t delta = read_uleb128(data, begin, end, cursor);
                    if (delta == 0) break;
                    func_off += delta;
                    std::cout << "        [" << nfunc << "] fileoff=" << func_off;
                    if (image_vmaddr_base_valid) {
                        std::cout << " vmaddr=0x" << std::hex
                            << (image_vmaddr_base + func_off) << std::dec;
                    }
                    std::cout << std::endl;
                    nfunc++;
                }
            }
        } else if (pcmd->cmd == LC_DATA_IN_CODE) {
            std::cout << "LC_DATA_IN_CODE" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(section_offset_len) << std::endl;
            section_offset_len *p = (section_offset_len*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    dataoff : " << std::to_string(p->offset) << std::endl;
            std::cout << "    datasize: " << std::to_string(p->len) << std::endl;
            if (p->len > 0) {
                ASSERT(p->offset <= data.size())
                ASSERT(p->len <= data.size() - p->offset)
                maybe_print_data_range(raw_dump, data, p->offset, p->len, "    ");
                if (p->len % 8 == 0) {
                    size_t n = p->len / 8;
                    std::cout << "    decoded data-in-code entries: " << n
                        << std::endl;
                    for (size_t i = 0; i < n; i++) {
                        size_t off = p->offset + i * 8;
                        uint32_t entry_off = read_u32(data, off);
                        uint16_t entry_len = (uint16_t)(data[off + 4]
                            | ((uint16_t)data[off + 5] << 8));
                        uint16_t entry_kind = (uint16_t)(data[off + 6]
                            | ((uint16_t)data[off + 7] << 8));
                        std::cout << "        [" << i << "] offset=" << entry_off
                            << " length=" << entry_len
                            << " kind=" << entry_kind;
                        if (image_vmaddr_base_valid) {
                            std::cout << " vmaddr=0x" << std::hex
                                << (image_vmaddr_base + entry_off) << std::dec;
                        }
                        std::cout << std::endl;
                    }
                }
            } else {
                std::cout << "    decoded data-in-code entries: none" << std::endl;
            }
        } else if (pcmd->cmd == LC_SOURCE_VERSION) {
            std::cout << "LC_SOURCE_VERSION" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(source_version_command) << std::endl;
            source_version_command *p = (source_version_command*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    version : " << std::to_string(p->version) << std::endl;
        } else if (pcmd->cmd == LC_BUILD_VERSION) {
            std::cout << "LC_BUILD_VERSION" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(build_version_command) << std::endl;
            build_version_command *p = (build_version_command*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    platform: " << std::to_string(p->platform) << std::endl;
            std::cout << "    minos   : " << version_to_str(p->minos) << std::endl;
            std::cout << "    sdk   : " << version_to_str(p->sdk) << std::endl;
            std::cout << "    ntools   : " << std::to_string(p->ntools) << std::endl;
            size_t trailer = pcmd->cmdsize - sizeof(build_version_command);
            if (trailer > 0) {
                // build_tool_version array
                maybe_print_data_range(raw_dump, data, idx + sizeof(build_version_command), trailer, "    ");
            }
        } else if (pcmd->cmd == LC_MAIN) {
            std::cout << "LC_MAIN" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(entry_point_command) << std::endl;
            entry_point_command *p = (entry_point_command*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    entryoff : " << std::to_string(p->entryoff) << std::endl;
            std::cout << "    stacksize: " << std::to_string(p->stacksize) << std::endl;
        } else if (pcmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            std::cout << "LC_DYLD_EXPORTS_TRIE" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(section_offset_len) << std::endl;
            section_offset_len *p = (section_offset_len*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    offset: " << p->offset <<std::endl;
            std::cout << "    len: " << p->len <<std::endl;
            if (p->len > 0) {
                ASSERT(p->offset <= data.size())
                ASSERT(p->len <= data.size() - p->offset)
                maybe_print_data_range(raw_dump, data, p->offset, p->len, "    ");
            }
        } else if (pcmd->cmd == LC_DYLD_CHAINED_FIXUPS) {
            std::cout << "LC_DYLD_CHAINED_FIXUPS" << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            std::cout << "    expect : " << sizeof(section_offset_len) << std::endl;
            section_offset_len *p = (section_offset_len*)(&data[idx]);
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
            std::cout << "    offset: " << p->offset <<std::endl;
            std::cout << "    len: " << p->len <<std::endl;
            if (p->len > 0) {
                ASSERT(p->offset <= data.size())
                ASSERT(p->len <= data.size() - p->offset)
                maybe_print_data_range(raw_dump, data, p->offset, p->len, "    ");
            }
        } else {
            std::cout << "UNKNOWN" << std::endl;
            std::cout << "    type: " << pcmd->cmd << std::endl;
            std::cout << "    cmdsize: " << pcmd->cmdsize << std::endl;
            maybe_print_data_range(raw_dump, data, idx, pcmd->cmdsize, "    ");
        }

        idx += pcmd->cmdsize;
    }
    std::cout << "Done." << std::endl;
    std::cout << "    idx      = " << idx << std::endl;
    std::cout << "    filesize = " << data.size() << std::endl;
}
