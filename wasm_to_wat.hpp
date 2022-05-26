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
