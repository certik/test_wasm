#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cassert>
#include "wasm_to_wat.hpp"

// #define WAT_DEBUG

#ifdef WAT_DEBUG
#define DEBUG(s) std::cout << s << std::endl
#else
#define DEBUG(s)
#endif

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

void decode_type_section(uint32_t offset) {
    // read type section contents
    uint32_t no_of_func_types = read_unsinged_num(offset);
    DEBUG("no_of_func_types: " + std::to_string(no_of_func_types));
    func_types.resize(no_of_func_types);

    for (uint32_t i = 0; i < no_of_func_types; i++) {
        if (wasm_bytes[offset] != 0x60) {
            std::cout << "Error: Invalid type section" << std::endl;
            exit(1);
        }
        offset++;

        // read result type 1
        uint32_t no_of_params = read_unsinged_num(offset);
        func_types[i].param_types.resize(no_of_params);

        for (uint32_t j = 0; j < no_of_params; j++) {
            func_types[i].param_types[j] = wasm_bytes[offset++];
        }

        uint32_t no_of_results = read_unsinged_num(offset);
        func_types[i].result_types.resize(no_of_results);

        for (uint32_t j = 0; j < no_of_results; j++) {
            func_types[i].result_types[j] = wasm_bytes[offset++];
        }
    }
}

void decode_function_section(uint32_t offset) {
    // read function section contents
    uint32_t no_of_indices = read_unsinged_num(offset);
    DEBUG("no_of_indices: " + std::to_string(no_of_indices));
    type_indices.resize(no_of_indices);

    for (uint32_t i = 0; i < no_of_indices; i++) {
        type_indices[i] = read_unsinged_num(offset);
    }
}

void decode_export_section(uint32_t offset) {
    // read export section contents
    uint32_t no_of_exports = read_unsinged_num(offset);
    DEBUG("no_of_exports: " + std::to_string(no_of_exports));
    exports.resize(no_of_exports);

    for (uint32_t i = 0; i < no_of_exports; i++) {
        uint32_t name_size = read_unsinged_num(offset);
        exports[i].name.resize(name_size);
        for (uint32_t j = 0; j < name_size; j++) {
            exports[i].name[j] = wasm_bytes[offset++];
        }
        DEBUG("export name: " + exports[i].name);
        exports[i].kind = wasm_bytes[offset++];
        DEBUG("export kind: " + std::to_string(exports[i].kind));
        exports[i].index = read_unsinged_num(offset);
        DEBUG("export index: " + std::to_string(exports[i].index));
    }
}

void decode_code_section(uint32_t offset) {
    // read code section contents
    uint32_t no_of_codes = read_unsinged_num(offset);
    DEBUG("no_of_codes: " + std::to_string(no_of_codes));
    codes.resize(no_of_codes);

    for (uint32_t i = 0; i < no_of_codes; i++) {
        codes[i].size = read_unsinged_num(offset);
        uint32_t no_of_locals = read_unsinged_num(offset);
        DEBUG("no_of_locals: " + std::to_string(no_of_locals));
        codes[i].locals.resize(no_of_locals);

        DEBUG("Entering loop");
        for (uint32_t j = 0U; j < no_of_locals; j++) {
            codes[i].locals[j].count = read_unsinged_num(offset);
            DEBUG("count: " + std::to_string(codes[i].locals[j].count));
            codes[i].locals[j].type = wasm_bytes[offset++];
            DEBUG("type: " + std::to_string(codes[i].locals[j].type));
        }
        DEBUG("Exiting loop");

        uint8_t cur_byte = wasm_bytes[offset++];
        while (cur_byte != 0x0B) {
            switch (cur_byte) {
                case 0x20: {  // local.get
                    uint32_t index = read_unsinged_num(offset);
                    codes[i].instructions.push_back(
                        std::make_unique<LocalGetInst>(cur_byte, index));
                    break;
                }
                case 0x21: {  // local.set
                    uint32_t index = read_unsinged_num(offset);
                    codes[i].instructions.push_back(
                        std::make_unique<LocalSetInst>(cur_byte, index));
                    break;
                }
                case 0x6A: {  // i32.add
                    codes[i].instructions.push_back(
                        std::make_unique<I32AddInst>(cur_byte));
                    break;
                }
                case 0x6B: {  // i32.sub
                    codes[i].instructions.push_back(
                        std::make_unique<I32SubInst>(cur_byte));
                    break;
                }
                case 0x6C: {  // i32.mul
                    codes[i].instructions.push_back(
                        std::make_unique<I32MulInst>(cur_byte));
                    break;
                }
                case 0x6D: {  // i32.div_s
                    codes[i].instructions.push_back(
                        std::make_unique<I32DivInst>(cur_byte));
                    break;
                }
                case 0x41: {  // i32.const
                    DEBUG("i32.const");
                    int val = read_singed_num(offset);
                    DEBUG("val: " + std::to_string(val));
                    codes[i].instructions.push_back(
                        std::make_unique<I32ConstInst>(cur_byte, val));
                    break;
                }
                case 0x10: {  // call func
                    uint32_t index = read_unsinged_num(offset);
                    codes[i].instructions.push_back(
                        std::make_unique<CallInst>(cur_byte, index));
                    break;
                }
                case 0x0F:{
                    codes[i].instructions.push_back(
                        std::make_unique<ReturnInst>(cur_byte));
                    break;
                }
                default: {
                    std::cout << "Error: Instruction " << std::to_string(cur_byte)
                              << " not supported" << std::endl;
                    exit(1);
                }
            }
            cur_byte = wasm_bytes[offset++];
        }
    }
}

void decode_wasm() {
    // first 8 bytes are magic number and wasm version number
    // currently, in this first version, we are skipping them
    uint32_t index = 8U;

    while (index < wasm_bytes.size()) {
        uint32_t section_id = read_unsinged_num(index);
        uint32_t section_size = read_unsinged_num(index);
        switch (section_id) {
            case 1U:
                decode_type_section(index);
                // exit(0);
                break;
            case 3U:
                decode_function_section(index);
                // exit(0);
                break;
            case 7U:
                decode_export_section(index);
                // exit(0);
                break;
            case 10U:
                decode_code_section(index);
                // exit(0)
                break;
            default:
                std::cout << "Unknown section id: " << section_id << std::endl;
                break;
        }
        index += section_size;
    }

    assert(index == wasm_bytes.size());
    assert(type_indices.size() == codes.size());
}

void hexdump(void *ptr, int buflen) {
    unsigned char *buf = (unsigned char *)ptr;
    int i, j;
    for (i = 0; i < buflen; i += 16) {
        printf("%06x: ", i);
        for (j = 0; j < 16; j++) {
            if (i + j < buflen)
                printf("%02x ", buf[i + j]);
            else
                printf("   ");
        }
        printf(" ");
        for (j = 0; j < 16; j++) {
            if (i + j < buflen)
                printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
        printf("\n");
    }
}

std::string get_wat() {
    std::string result = "(module";
    for (uint32_t i = 0; i < type_indices.size(); i++) {
        result += "\n    (func $" + std::to_string(i);
        result += "\n        (param";
        uint32_t func_index = type_indices[i];
        for (uint32_t j = 0; j < func_types[func_index].param_types.size();
             j++) {
            result +=
                " " + type_to_string[func_types[func_index].param_types[j]];
        }
        result += ") (result";
        for (uint32_t j = 0; j < func_types[func_index].result_types.size();
             j++) {
            result +=
                " " + type_to_string[func_types[func_index].result_types[j]];
        }
        result += ")";
        result += "\n        (local";
        for (uint32_t j = 0; j < codes[i].locals.size(); j++) {
            for (uint32_t k = 0; k < codes[i].locals[j].count; k++) {
                result += " " + type_to_string[codes[i].locals[j].type];
            }
        }
        result += ")";
        for (uint32_t j = 0; j < codes[i].instructions.size(); j++) {
            result += "\n        " + codes[i].instructions[j]->to_string();
        }
        result += "\n    )";
    }

    for (uint32_t i = 0; i < exports.size(); i++) {
        result += "\n    (export \"" + exports[i].name + "\" (" +
                  kind_to_string[exports[i].kind] + " $" +
                  std::to_string(exports[i].index) + "))";
    }
    result += "\n)";

    return result;
}

int main() {
    load_file("test2.wasm");

#ifdef WAT_DEBUG
    hexdump(wasm_bytes.data(), wasm_bytes.size());
    std::cout << std::endl;
#endif

    decode_wasm();

#ifdef WAT_DEBUG
    std::cout << "Decoding Successful!\n" << std::endl;
    std::cout << "Printing WAT\n" << std::endl;
#endif

    std::cout << get_wat() << std::endl;
    return 0;
}
