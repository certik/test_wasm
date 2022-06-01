#ifndef LFORTRAN_WASM_UTILS_H
#define LFORTRAN_WASM_UTILS_H

#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>

// this is temporary, we may not need this when we integrate with LFortran
namespace LFortran {
std::string LFortranException(std::string msg) { return "LFortranException: " + msg; }
}  // namespace LFortran

std::unordered_map<uint8_t, std::string> type_to_string = {{0x7F, "i32"}, {0x7E, "i64"}, {0x7D, "f32"}, {0x7C, "f64"}};

std::unordered_map<uint8_t, std::string> kind_to_string = {{0x00, "func"}, {0x01, "table"}, {0x02, "mem"}, {0x03, "global"}};

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
    uint32_t insts_start_index;
};

std::vector<uint8_t> wasm_bytes;
std::vector<FuncType> func_types;
std::vector<uint32_t> type_indices;
std::vector<Export> exports;
std::vector<Code> codes;

uint32_t decode_unsigned_leb128(uint32_t& offset) {
    uint32_t result = 0U;
    uint32_t shift = 0U;
    while (true) {
        uint8_t byte = wasm_bytes[offset++];
        result |= (byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }
}

int32_t decode_signed_leb128(uint32_t& offset) {
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

    return result;
}

void load_file(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    wasm_bytes.resize(size);
    file.read((char*)wasm_bytes.data(), size);
    file.close();
}

uint8_t read_byte(uint32_t& offset) {
    if (offset >= wasm_bytes.size()) {
        throw LFortran::LFortranException("read_byte: offset out of bounds");
    }
    return wasm_bytes[offset++];
}

float read_float(uint32_t& offset) {
    // to implement
    return 0.00;
}

double read_double(uint32_t& offset) {
    // to implement
    return 0.00;
}

int32_t read_signed_num(uint32_t& offset) { return decode_signed_leb128(offset); }

uint32_t read_unsigned_num(uint32_t& offset) { return decode_unsigned_leb128(offset); }

#endif  // LFORTRAN_WASM_UTILS_H
