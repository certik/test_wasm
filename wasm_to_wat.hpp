#include <iostream>
#include <vector>
#include <unordered_map>

// #define WAT_DEBUG

#ifdef WAT_DEBUG
#define DEBUG(s) std::cout << s << std::endl
#else
#define DEBUG(s)
#endif

std::unordered_map<uint8_t, std::string> type_to_string = {
        {0x7F, "i32"}, {0x7E, "i64"}, {0x7D, "f32"}, {0x7C, "f64"}};

std::unordered_map<uint8_t, std::string> kind_to_string = {
        {0x00, "func"}, {0x01, "table"}, {0x02, "mem"}, {0x03, "global"}};
        
struct FuncType {
    std::vector<uint8_t> param_types;
    std::vector<uint8_t> result_types;
};

struct Export {
    std::string name;
    uint8_t kind;
    uint32_t index;
};

struct Local {
    uint32_t count;
    uint8_t type;
};

struct Code {
    int size;
    std::vector<Local> locals;
};

std::vector<uint8_t> wasm_bytes;
std::vector<FuncType> func_types;
std::vector<uint32_t> type_indices;
std::vector<Export> exports;
std::vector<Code> codes;

uint32_t decode_unsigned_leb128(uint32_t &offset) {
    uint32_t result = 0U;
    uint32_t shift = 0U;
    while (true) {
        uint8_t byte = wasm_bytes[offset++];
        result |= (byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            DEBUG("decode_unsigned_leb128 returned: " + std::to_string(result));
            return result;
        }
        shift += 7;
    }
}

int32_t decode_signed_leb128(uint32_t &offset) {
    int32_t result = 0;
    uint32_t shift = 0U;
    uint32_t size = 32U;
    uint8_t byte;

    do {
        byte = wasm_bytes[offset++];
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);

    if ((shift < size) && (byte & 0x40)) {
        result |= (~0 << shift);
    }

    DEBUG("decode_signed_leb128 returned: " + std::to_string(result));
    return result;
}

void load_file(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    wasm_bytes.resize(size);
    file.read((char *)wasm_bytes.data(), size);
    file.close();
}

int32_t read_singed_num(uint32_t &offset) {
    return decode_signed_leb128(offset);
}

uint32_t read_unsinged_num(uint32_t &offset) {
    return decode_unsigned_leb128(offset);
}

bool isBetween(uint8_t val, uint8_t min, uint8_t max) {
    return (val >= min && val <= max);
}

void visit_ControlInstruction(std::string &result, std::string &indent,
                              uint32_t &offset) {
    uint8_t cur_byte = wasm_bytes[offset++];
    switch (cur_byte) {
        case 0x0F: {  // return
            result += indent + "return";
            break;
        }
        case 0x10: {  // call function
            uint32_t func_index = read_unsinged_num(offset);
            result += indent + "call " + std::to_string(func_index);
            break;
        }
        default: {
            std::cout << "Control Instruction (" << std::hex << cur_byte
                      << std::dec;
            std::cout << ") Not yet supported" << std::endl;
            break;
        }
    }

    return;
}

void visit_ReferenceInstruction(std::string &result, std::string &indent,
                                uint32_t &offset) {
    // not yet supported
    return;
}

void visit_ParametricInstruction(std::string &result, std::string &indent,
                                 uint32_t &offset) {
    // not yet supported
    return;
}

void visit_VariableInstruction(std::string &result, std::string &indent,
                               uint32_t &offset) {
    uint8_t cur_byte = wasm_bytes[offset++];
    uint32_t var_index = read_unsinged_num(offset);

    switch (cur_byte) {
        case 0x20: {  // local.get
            result += indent + "local.get " + std::to_string(var_index);
            break;
        }
        case 0x21: {  // local.set
            result += indent + "local.set " + std::to_string(var_index);
            break;
        }
        default: {
            std::cout << "Variable Instruction (" << std::hex << cur_byte
                      << std::dec;
            std::cout << ") Not yet supported" << std::endl;
            break;
        }
    }

    return;
}

void visit_TableInstruction(std::string &result, std::string &indent,
                            uint32_t &offset) {
    // not yet supported
    return;
}

void visit_MemoryInstruction(std::string &result, std::string &indent,
                             uint32_t &offset) {
    // not yet supported
    return;
}

void visit_NumericInstruction(std::string &result, std::string &indent,
                              uint32_t &offset) {
    uint8_t cur_byte = wasm_bytes[offset++];
    switch (cur_byte) {
        case 0x41: {  // i32.const
            int32_t value = read_singed_num(offset);
            result += indent + "i32.const " + std::to_string(value);
            break;
        }
        // case 0x42:{ // i64.const

        //     break;
        // }
        // case 0x43:{ // f32.const

        //     break;
        // }
        // case 0x44:{ // f64.const

        //     break;
        // }
        case 0x6A: {  // i32.add
            result += indent + "i32.add";
            break;
        }
        case 0x6B: {  // i32.sub
            result += indent + "i32.sub";
            break;
        }
        case 0x6C: {  // i32.mul
            result += indent + "i32.mul";
            break;
        }
        case 0x6D: {  // i32.div_s
            result += indent + "i32.div_s";
            break;
        }
        default: {
            std::cout << "Numeric Instruction (" << std::hex << cur_byte
                      << std::dec;
            std::cout << ") Not yet supported" << std::endl;
            break;
        }
    }

    return;
}

void visit_VectorInstruction(std::string &result, std::string &indent,
                             uint32_t &offset) {
    // not yet supported
    return;
}

void visit_Instructions(std::string &result, std::string &indent,
                       uint32_t &offset) {
    uint8_t cur_byte = wasm_bytes[offset];

    if (cur_byte == 0x0B) { // End of Expressions
        return;
    }

    if (isBetween(cur_byte, 0x00, 0x04) ||
        isBetween(cur_byte, 0x0C, 0x11)) {  // Control Instructions
        visit_ControlInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0xD0, 0xD2)) {  // Reference Instructions
        visit_ReferenceInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0x1A, 0x1C)) {  // Parametric Instructions
        visit_ParametricInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0x20, 0x24)) {  // Variable Instructions
        visit_VariableInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0x25,0x26)) {  // Table Instructions (0xFC is currently dropped)
        visit_TableInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0x28, 0x29) ||
               isBetween(cur_byte, 0x2A, 0x40)) {  // Memory Instructions (0xFC is currently dropped)
        visit_MemoryInstruction(result, indent, offset);
    }
    else if (isBetween(cur_byte, 0x41, 0xC4)) {  // Numeric Instructions (0xFC is currently dropped)
        visit_NumericInstruction(result, indent, offset);
    }
    else if (cur_byte == 0xFD) {  // Vector Instructions
        visit_VectorInstruction(result, indent, offset);
    }

    visit_Instructions(result, indent, offset);
}
