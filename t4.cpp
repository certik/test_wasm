#include <vector>
#include <fstream>
#include <cassert>
#include <cstring>

std::vector<uint8_t> encode_signed_leb128(int32_t n) {
    std::vector<uint8_t> out;
    auto more = true;
    do {
        uint8_t byte = n & 0x7f;
        n >>= 7;
        more = !((((n == 0) && ((byte & 0x40) == 0)) ||
                  ((n == -1) && ((byte & 0x40) != 0))));
        if (more) {
            byte |= 0x80;
        }
        out.emplace_back(byte);
    } while (more);
    return out;
}

std::vector<uint8_t> encode_unsigned_leb128(uint32_t n) {
    std::vector<uint8_t> out;
    do {
        uint8_t byte = n & 0x7f;
        n >>= 7;
        if (n != 0) {
            byte |= 0x80;
        }
        out.emplace_back(byte);
    } while (n != 0);
    return out;
}

class WASMAssembler {
   public:
    uint8_t i32 = 0x7F;
    uint8_t i64 = 0x7E;
    uint8_t f32 = 0x7D;
    uint8_t f64 = 0x7C;

    std::vector<uint8_t> code;

    WASMAssembler() { code.clear(); }

    // function to save to binary file with the given filename
    void save_bin(const char* filename) {
        FILE* fp = fopen(filename, "wb");
        fwrite(code.data(), sizeof(uint8_t), code.size(), fp);
        fclose(fp);
    }

    // function to emit header of Wasm Binary Format
    void emit_header() {
        code.push_back(0x00);
        code.push_back(0x61);
        code.push_back(0x73);
        code.push_back(0x6D);
        code.push_back(0x01);
        code.push_back(0x00);
        code.push_back(0x00);
        code.push_back(0x00);
    }

    // function to emit unsigned 32 bit integer
    void emit_u32(uint32_t x) {
        std::vector<uint8_t> leb128 = encode_unsigned_leb128(x);
        code.insert(code.end(), leb128.begin(), leb128.end());
    }

    // function to emit signed 32 bit integer
    void emit_i32(int32_t x) {
        std::vector<uint8_t> leb128 = encode_signed_leb128(x);
        code.insert(code.end(), leb128.begin(), leb128.end());
    }

    // function to append a given bytecode to the end of the code
    void emit_b8(uint8_t x) { code.push_back(x); }

    void emit_u32_b32_idx(uint8_t idx, uint8_t i){
        /*
        Encodes the integer `i` using LEB128 and adds trailing zeros to always
        occupy 4 bytes. Stores the int `i` at the index `idx` in `code`.
        */
        std::vector<uint8_t> num = encode_unsigned_leb128(i);
        std::vector<uint8_t> num_4b = {0x80, 0x80, 0x80, 0x00};
        assert(num.size() <= 4);
        for (int i = 0; i < num.size(); i++) {
            num_4b[i] |= num[i];
        }
        for(int i = 0; i < 4; i++){
            code[idx+i] = num_4b[i];
        }
    }

    // function to fixup length at the given length index
    void fixup_len(uint32_t len_idx) {
        uint32_t section_len = code.size() - len_idx - 4u;
        emit_u32_b32_idx(len_idx, section_len);
    }

    // function to emit length placeholder
    uint32_t emit_len_placeholder() {
        uint32_t len_idx = code.size();
        code.push_back(0x00);
        code.push_back(0x00);
        code.push_back(0x00);
        code.push_back(0x00);
        return len_idx;
    }

    // function to emit a i32.const instruction
    void emit_i32_const(int32_t x) {
        code.push_back(0x41);
        emit_i32(x);
    }

    // function to emit end of wasm expression
    void emit_end() { code.push_back(0x0B); }

    // function to emit get local variable at given index
    void emit_get_local(uint32_t idx) {
        code.push_back(0x20);
        emit_u32(idx);
    }

    // function to emit i32.add instruction
    void emit_i32_add() { code.push_back(0x6A); }

    // function to emit call instruction
    void emit_call(uint32_t idx) {
        code.push_back(0x10);
        emit_u32(idx);
    }
};

// Functions to emit WASM Sections

void emit_fn_type(WASMAssembler& wasm, std::vector<uint8_t> param_types,
                  std::vector<uint8_t> return_types) {
    wasm.emit_b8(0x60);
    wasm.emit_u32(param_types.size());
    wasm.code.insert(wasm.code.end(), param_types.begin(), param_types.end());
    wasm.emit_u32(return_types.size());
    wasm.code.insert(wasm.code.end(), return_types.begin(), return_types.end());
}

void emit_type_section(WASMAssembler& wasm) {
    wasm.emit_u32(1);
    uint32_t len_idx = wasm.emit_len_placeholder();

    wasm.emit_u32(2);  // number of functions
    emit_fn_type(wasm, {}, {wasm.i32});
    emit_fn_type(wasm, {wasm.i32, wasm.i32}, {wasm.i32});
    wasm.fixup_len(len_idx);
}

void emit_function_section(WASMAssembler& wasm) {
    wasm.emit_u32(3);
    uint32_t len_idx = wasm.emit_len_placeholder();

    wasm.emit_u32(2);  // number of functions

    // function indices
    wasm.emit_u32(0);
    wasm.emit_u32(1);
    wasm.fixup_len(len_idx);
}

void emit_export_fn(WASMAssembler& wasm, const std::string& name,
                    uint32_t idx) {
    std::vector<uint8_t> name_bytes(name.size());
    std::memcpy(name_bytes.data(), name.data(), name.size());
    wasm.emit_u32(name_bytes.size());
    wasm.code.insert(wasm.code.end(), name_bytes.begin(), name_bytes.end());
    wasm.emit_b8(0x00);
    wasm.emit_u32(idx);
}

void emit_export_section(WASMAssembler& wasm) {
    wasm.emit_u32(7);
    uint32_t len_idx = wasm.emit_len_placeholder();

    wasm.emit_u32(2);  // number of functions
    emit_export_fn(wasm, "get_const_val", 0);
    emit_export_fn(wasm, "add_two_nums", 1);
    wasm.fixup_len(len_idx);
}

void emit_function_1(WASMAssembler& wasm) {
    uint32_t len_idx = wasm.emit_len_placeholder();

    // Local vars
    wasm.emit_u32(0);

    // Instructions
    wasm.emit_i32_const(-10);
    wasm.emit_end();

    wasm.fixup_len(len_idx);
}

void emit_function_2(WASMAssembler& wasm) {
    uint32_t len_idx = wasm.emit_len_placeholder();

    // Local vars
    wasm.emit_u32(0);

    // Instructions
    wasm.emit_get_local(0);
    wasm.emit_get_local(1);
    wasm.emit_i32_add();
    wasm.emit_call(0);
    wasm.emit_i32_add();
    wasm.emit_end();

    wasm.fixup_len(len_idx);
}

void emit_code_section(WASMAssembler& wasm) {
    wasm.emit_u32(10);
    uint32_t len_idx = wasm.emit_len_placeholder();

    wasm.emit_u32(2);  // number of functions
    emit_function_1(wasm);
    emit_function_2(wasm);
    wasm.fixup_len(len_idx);
}

int main() {
    WASMAssembler wasm;
    wasm.emit_header();
    emit_type_section(wasm);
    emit_function_section(wasm);
    emit_export_section(wasm);
    emit_code_section(wasm);
    wasm.save_bin("test.wasm");
}