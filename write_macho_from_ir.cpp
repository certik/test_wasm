#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

#include <CommonCrypto/CommonDigest.h>

#include "macho_utils.h"

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "REQUIRE failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

static const char *kLlvmIr = R"IR(
@prefix = private unnamed_addr constant [24 x i8] c"numbers via print_i64:\0A\00", align 1
@suffix = private unnamed_addr constant [7 x i8] c"done.\0A\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@itoa_buf = global [32 x i8] zeroinitializer, align 1

declare i64 @write(i32, ptr, i64)
declare void @exit(i32)
declare i64 @strlen(ptr)

define noundef ptr @int_to_string(i32 noundef %0, ptr noundef returned writeonly %1) local_unnamed_addr {
  %3 = icmp sgt i32 %0, 0
  br i1 %3, label %5, label %4

4:
  store i8 0, ptr %1, align 1
  br label %26

5:
  %6 = phi i32 [ %9, %5 ], [ %0, %2 ]
  %7 = phi i32 [ %8, %5 ], [ 0, %2 ]
  %8 = add nuw nsw i32 %7, 1
  %9 = udiv i32 %6, 10
  %10 = icmp ult i32 %6, 10
  br i1 %10, label %11, label %5

11:
  %12 = zext nneg i32 %8 to i64
  %13 = getelementptr inbounds i8, ptr %1, i64 %12
  store i8 0, ptr %13, align 1
  br i1 %3, label %14, label %26

14:
  %15 = phi i64 [ %23, %14 ], [ %12, %11 ]
  %16 = phi i32 [ %18, %14 ], [ %0, %11 ]
  %17 = freeze i32 %16
  %18 = udiv i32 %17, 10
  %19 = mul i32 %18, 10
  %20 = sub i32 %17, %19
  %21 = trunc nuw nsw i32 %20 to i8
  %22 = or disjoint i8 %21, 48
  %23 = add nsw i64 %15, -1
  %24 = getelementptr inbounds i8, ptr %1, i64 %23
  store i8 %22, ptr %24, align 1
  %25 = icmp ult i32 %16, 10
  br i1 %25, label %26, label %14

26:
  ret ptr %1
}

define void @print_i64(i64 %n) {
entry:
  %n32 = trunc i64 %n to i32
  %s = call ptr @int_to_string(i32 %n32, ptr @itoa_buf)
  %len = call i64 @strlen(ptr %s)
  %written = call i64 @write(i32 1, ptr %s, i64 %len)
  %nl_written = call i64 @write(i32 1, ptr @nl, i64 1)
  ret void
}

define i32 @main() {
entry:
  %prefix_written = call i64 @write(i32 1, ptr @prefix, i64 23)
  call void @print_i64(i64 0)
  call void @print_i64(i64 7)
  call void @print_i64(i64 42)
  call void @print_i64(i64 12345)
  %suffix_written = call i64 @write(i32 1, ptr @suffix, i64 6)
  call void @exit(i32 42)
  ret i32 42
}
)IR";

enum class OpKind {
    WriteGlobal,
    PrintI64,
    ExitCode,
    ReturnCode,
};

struct Operation {
    OpKind kind;
    std::string symbol;
    int64_t value;
};

struct ProgramIR {
    std::unordered_map<std::string, std::string> globals;
    std::vector<std::string> global_order;
    std::vector<std::string> int_to_string_body;
    std::vector<std::string> print_i64_body;
    std::vector<Operation> ops;
};

static void vec_append(std::vector<uint8_t> &data, const void *item, size_t size) {
    const uint8_t *p = (const uint8_t *)item;
    data.insert(data.end(), p, p + size);
}

static void append_u8(std::vector<uint8_t> &out, uint8_t v) {
    out.push_back(v);
}

static void append_le16(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
}

static void append_le32(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back((uint8_t)(v & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
    out.push_back((uint8_t)((v >> 16) & 0xff));
    out.push_back((uint8_t)((v >> 24) & 0xff));
}

static void append_le64(std::vector<uint8_t> &out, uint64_t v) {
    for (int i = 0; i < 8; i++) out.push_back((uint8_t)((v >> (8 * i)) & 0xff));
}

static void append_be32(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back((uint8_t)((v >> 24) & 0xff));
    out.push_back((uint8_t)((v >> 16) & 0xff));
    out.push_back((uint8_t)((v >> 8) & 0xff));
    out.push_back((uint8_t)(v & 0xff));
}

static void append_uleb128(std::vector<uint8_t> &out, uint64_t value) {
    do {
        uint8_t byte = (uint8_t)(value & 0x7f);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

static void append_cstr(std::vector<uint8_t> &out, const std::string &s) {
    out.insert(out.end(), s.begin(), s.end());
    out.push_back('\0');
}

static void append_padding_to(std::vector<uint8_t> &out, size_t target_size) {
    REQUIRE(out.size() <= target_size);
    out.insert(out.end(), target_size - out.size(), 0);
}

static std::string trim_copy(const std::string &s) {
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b])) b++;
    size_t e = s.size();
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

static int64_t parse_i64_after(const std::string &line, const std::string &needle) {
    const size_t p = line.find(needle);
    REQUIRE(p != std::string::npos);
    size_t i = p + needle.size();
    while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
    bool neg = false;
    if (i < line.size() && line[i] == '-') {
        neg = true;
        i++;
    }
    REQUIRE(i < line.size() && std::isdigit((unsigned char)line[i]));
    int64_t v = 0;
    while (i < line.size() && std::isdigit((unsigned char)line[i])) {
        v = v * 10 + (line[i] - '0');
        i++;
    }
    return neg ? -v : v;
}

static int64_t parse_i64_after_last(const std::string &line, const std::string &needle) {
    const size_t p = line.rfind(needle);
    REQUIRE(p != std::string::npos);
    return parse_i64_after(line.substr(p), needle);
}

static std::string parse_global_name_from_def(const std::string &line) {
    REQUIRE(!line.empty() && line[0] == '@');
    size_t i = 1;
    while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '.')) i++;
    REQUIRE(i > 1);
    return line.substr(1, i - 1);
}

static std::string parse_symbol_after(const std::string &line, const std::string &needle) {
    const size_t p = line.find(needle);
    REQUIRE(p != std::string::npos);
    size_t i = p + needle.size();
    size_t b = i;
    while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '.')) i++;
    REQUIRE(i > b);
    return line.substr(b, i - b);
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string decode_llvm_c_string(const std::string &encoded) {
    std::string out;
    for (size_t i = 0; i < encoded.size(); i++) {
        const char c = encoded[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        REQUIRE(i + 1 < encoded.size());
        if (i + 2 < encoded.size()) {
            const int hi = hex_nibble(encoded[i + 1]);
            const int lo = hex_nibble(encoded[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        const char esc = encoded[++i];
        if (esc == '\\' || esc == '"') {
            out.push_back(esc);
            continue;
        }
        if (esc == 'n') {
            out.push_back('\n');
            continue;
        }
        REQUIRE(false);
    }
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

static ProgramIR parse_program_ir(const std::string &ir_text) {
    ProgramIR program;
    bool in_main = false;
    bool in_int_to_string = false;
    bool in_print_i64 = false;
    size_t start = 0;
    while (start <= ir_text.size()) {
        const size_t end = ir_text.find('\n', start);
        const std::string line = trim_copy(ir_text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!line.empty()) {
            if (line[0] == '@' && line.find(" c\"") != std::string::npos) {
                const std::string global_name = parse_global_name_from_def(line);
                const size_t cpos = line.find("c\"");
                REQUIRE(cpos != std::string::npos);
                const size_t qend = line.find('"', cpos + 2);
                REQUIRE(qend != std::string::npos);
                if (program.globals.find(global_name) == program.globals.end()) {
                    program.global_order.push_back(global_name);
                }
                program.globals[global_name] = decode_llvm_c_string(line.substr(cpos + 2, qend - (cpos + 2)));
            } else if (line.rfind("define i32 @main(", 0) == 0) {
                in_main = true;
            } else if (line.rfind("define noundef ptr @int_to_string(", 0) == 0) {
                in_int_to_string = true;
            } else if (line.rfind("define void @print_i64(", 0) == 0) {
                in_print_i64 = true;
            } else if (in_main && line == "}") {
                in_main = false;
            } else if (in_int_to_string && line == "}") {
                in_int_to_string = false;
            } else if (in_print_i64 && line == "}") {
                in_print_i64 = false;
            } else if (in_main && line.find("@write(") != std::string::npos) {
                const std::string symbol = parse_symbol_after(line, "ptr @");
                const int64_t write_len = parse_i64_after_last(line, "i64 ");
                program.ops.push_back(Operation{OpKind::WriteGlobal, symbol, write_len});
            } else if (in_main && line.find("call void @print_i64(") != std::string::npos) {
                const int64_t value = parse_i64_after_last(line, "i64 ");
                program.ops.push_back(Operation{OpKind::PrintI64, "", value});
            } else if (in_int_to_string) {
                program.int_to_string_body.push_back(line);
            } else if (in_print_i64 && line != "entry:") {
                program.print_i64_body.push_back(line);
            } else if (in_main && line.find("call void @exit(") != std::string::npos) {
                const int64_t exit_code = parse_i64_after(line, "i32 ");
                program.ops.push_back(Operation{OpKind::ExitCode, "", exit_code});
            } else if (in_main && line.rfind("ret i32 ", 0) == 0) {
                const int64_t ret_code = parse_i64_after(line, "ret i32 ");
                program.ops.push_back(Operation{OpKind::ReturnCode, "", ret_code});
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }

    REQUIRE(!program.globals.empty());
    REQUIRE(!program.int_to_string_body.empty());
    REQUIRE(!program.print_i64_body.empty());
    REQUIRE(!program.ops.empty());
    return program;
}

static uint32_t arm64_movz_64(uint8_t rd, uint16_t imm16, uint8_t shift) {
    REQUIRE(rd <= 31);
    REQUIRE((shift % 16) == 0);
    const uint8_t hw = (uint8_t)(shift / 16);
    REQUIRE(hw <= 3);
    return 0xd2800000U | ((uint32_t)hw << 21) | ((uint32_t)imm16 << 5) | (uint32_t)rd;
}

static uint32_t arm64_adrp(uint8_t rd, int64_t page_delta) {
    REQUIRE(rd <= 31);
    REQUIRE(page_delta >= -(1 << 20));
    REQUIRE(page_delta <= ((1 << 20) - 1));
    const uint32_t immlo = (uint32_t)(page_delta & 0x3);
    const uint32_t immhi = (uint32_t)((page_delta >> 2) & 0x7ffff);
    return 0x90000000U | (immlo << 29) | (immhi << 5) | (uint32_t)rd;
}

static uint32_t arm64_add_imm_64(uint8_t rd, uint8_t rn, uint16_t imm12, uint8_t shift) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    REQUIRE(shift == 0 || shift == 12);
    const uint32_t sh = shift == 12 ? 1U : 0U;
    return 0x91000000U | (sh << 22) | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_bl(int32_t imm26) {
    REQUIRE(imm26 >= -(1 << 25));
    REQUIRE(imm26 <= ((1 << 25) - 1));
    return 0x94000000U | ((uint32_t)imm26 & 0x03ffffffU);
}

static uint32_t arm64_ldr_imm_u64(uint8_t rt, uint8_t rn, uint16_t byte_offset) {
    REQUIRE(rt <= 31);
    REQUIRE(rn <= 31);
    REQUIRE((byte_offset % 8) == 0);
    const uint16_t imm12 = (uint16_t)(byte_offset / 8);
    REQUIRE(imm12 <= 0x0fff);
    return 0xf9400000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

static uint32_t arm64_br(uint8_t rn) {
    REQUIRE(rn <= 31);
    return 0xd61f0000U | ((uint32_t)rn << 5);
}

static uint32_t arm64_ret() {
    return 0xd65f03c0U;
}

static void emit_arm64(std::vector<uint8_t> &out, uint32_t inst) {
    append_le32(out, inst);
}

static int64_t arm64_adrp_page_delta(uint64_t from_insn_addr, uint64_t to_addr) {
    const uint64_t from_page = from_insn_addr & ~0xfffULL;
    const uint64_t to_page = to_addr & ~0xfffULL;
    return (int64_t)((to_page - from_page) / 4096);
}

static int32_t arm64_bl_imm26_from_addrs(uint64_t from_insn_addr, uint64_t to_addr) {
    const int64_t delta = (int64_t)to_addr - (int64_t)from_insn_addr;
    REQUIRE((delta % 4) == 0);
    return (int32_t)(delta / 4);
}

struct CStringPlan {
    std::vector<uint8_t> bytes;
    std::unordered_map<std::string, uint64_t> addr;
};

struct LabelPatch {
    size_t at_word;
    std::string label;
    enum class Kind { B, CBZW } kind;
    uint8_t rt;
};

struct AsmBuilder {
    std::vector<uint32_t> words;
    std::unordered_map<std::string, size_t> labels;
    std::vector<LabelPatch> patches;
};

static uint32_t arm64_add_imm_32(uint8_t rd, uint8_t rn, uint16_t imm12) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    return 0x11000000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_sub_imm_64(uint8_t rd, uint8_t rn, uint16_t imm12) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    return 0xd1000000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_add_reg_64(uint8_t rd, uint8_t rn, uint8_t rm) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(rm <= 31);
    return 0x8b000000U | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_udiv_32(uint8_t rd, uint8_t rn, uint8_t rm) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(rm <= 31);
    return 0x1ac00800U | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_mul_32(uint8_t rd, uint8_t rn, uint8_t rm) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(rm <= 31);
    return 0x1b000000U | ((uint32_t)rm << 16) | (31U << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_sub_reg_32(uint8_t rd, uint8_t rn, uint8_t rm) {
    REQUIRE(rd <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(rm <= 31);
    return 0x4b000000U | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

static uint32_t arm64_strb_uimm(uint8_t rt, uint8_t rn, uint16_t imm12) {
    REQUIRE(rt <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    return 0x39000000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

static uint32_t arm64_ldrb_uimm(uint8_t rt, uint8_t rn, uint16_t imm12) {
    REQUIRE(rt <= 31);
    REQUIRE(rn <= 31);
    REQUIRE(imm12 <= 0x0fff);
    return 0x39400000U | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

static uint32_t arm64_b(int32_t imm26) {
    REQUIRE(imm26 >= -(1 << 25));
    REQUIRE(imm26 <= ((1 << 25) - 1));
    return 0x14000000U | ((uint32_t)imm26 & 0x03ffffffU);
}

static uint32_t arm64_cbz_w(uint8_t rt, int32_t imm19) {
    REQUIRE(rt <= 31);
    REQUIRE(imm19 >= -(1 << 18));
    REQUIRE(imm19 <= ((1 << 18) - 1));
    return 0x34000000U | (((uint32_t)imm19 & 0x7ffffU) << 5) | (uint32_t)rt;
}

static void asm_label(AsmBuilder &a, const std::string &name) {
    a.labels[name] = a.words.size();
}

static void asm_emit(AsmBuilder &a, uint32_t inst) {
    a.words.push_back(inst);
}

static void asm_emit_b_to(AsmBuilder &a, const std::string &label) {
    a.patches.push_back(LabelPatch{a.words.size(), label, LabelPatch::Kind::B, 0});
    asm_emit(a, 0);
}

static void asm_emit_cbzw_to(AsmBuilder &a, uint8_t rt, const std::string &label) {
    a.patches.push_back(LabelPatch{a.words.size(), label, LabelPatch::Kind::CBZW, rt});
    asm_emit(a, 0);
}

static void asm_resolve_patches(AsmBuilder &a) {
    for (const LabelPatch &p : a.patches) {
        const auto it = a.labels.find(p.label);
        REQUIRE(it != a.labels.end());
        const int64_t delta_words = (int64_t)it->second - (int64_t)p.at_word;
        if (p.kind == LabelPatch::Kind::B) {
            a.words[p.at_word] = arm64_b((int32_t)delta_words);
        } else if (p.kind == LabelPatch::Kind::CBZW) {
            a.words[p.at_word] = arm64_cbz_w(p.rt, (int32_t)delta_words);
        } else {
            REQUIRE(false);
        }
    }
}

static std::vector<uint8_t> asm_to_bytes(const AsmBuilder &a) {
    std::vector<uint8_t> out;
    out.reserve(a.words.size() * 4);
    for (uint32_t w : a.words) append_le32(out, w);
    return out;
}

static void validate_int_to_string_ir(const ProgramIR &program) {
    bool saw_udiv = false;
    bool saw_mul = false;
    bool saw_sub = false;
    bool saw_digit_store = false;
    bool saw_ret = false;
    for (const std::string &line : program.int_to_string_body) {
        if (line.find("udiv i32") != std::string::npos) saw_udiv = true;
        if (line.find("mul i32") != std::string::npos) saw_mul = true;
        if (line.find("sub i32") != std::string::npos) saw_sub = true;
        if (line.find("store i8 %22, ptr %24") != std::string::npos) saw_digit_store = true;
        if (line == "ret ptr %1") saw_ret = true;
    }
    REQUIRE(saw_udiv && saw_mul && saw_sub && saw_digit_store && saw_ret);
}

static void validate_print_i64_ir(const ProgramIR &program) {
    bool saw_convert = false;
    bool saw_strlen = false;
    bool saw_write_digits = false;
    bool saw_write_nl = false;
    bool saw_ret = false;
    for (const std::string &line : program.print_i64_body) {
        if (line == "%s = call ptr @int_to_string(i32 %n32, ptr @itoa_buf)") saw_convert = true;
        if (line == "%len = call i64 @strlen(ptr %s)") saw_strlen = true;
        if (line == "%written = call i64 @write(i32 1, ptr %s, i64 %len)") saw_write_digits = true;
        if (line == "%nl_written = call i64 @write(i32 1, ptr @nl, i64 1)") saw_write_nl = true;
        if (line == "ret void") saw_ret = true;
    }
    REQUIRE(saw_convert && saw_strlen && saw_write_digits && saw_write_nl && saw_ret);
}

static CStringPlan build_cstring_plan(const ProgramIR &program, uint64_t cstring_addr) {
    CStringPlan plan;
    uint64_t addr = cstring_addr;
    for (const std::string &name : program.global_order) {
        const auto it = program.globals.find(name);
        REQUIRE(it != program.globals.end());
        plan.addr[name] = addr;
        plan.bytes.insert(plan.bytes.end(), it->second.begin(), it->second.end());
        append_u8(plan.bytes, 0);
        addr += (uint64_t)it->second.size() + 1;
    }
    return plan;
}

static size_t op_encoded_size(const Operation &op) {
    switch (op.kind) {
        case OpKind::WriteGlobal: return 5 * 4;
        case OpKind::PrintI64: return 2 * 4;
        case OpKind::ExitCode: return 2 * 4;
        case OpKind::ReturnCode: return 2 * 4;
    }
    REQUIRE(false);
}

static size_t print_i64_encoded_size() {
    return 22 * 4;
}

static size_t int_to_string_encoded_size() {
    return 23 * 4;
}

static std::vector<uint8_t> build_int_to_string_bytes() {
    AsmBuilder a;
    asm_emit_cbzw_to(a, 0, "zero");
    asm_emit(a, arm64_add_imm_32(2, 0, 0));      // w2 = w0
    asm_emit(a, arm64_movz_64(3, 0, 0));         // x3 = 0
    asm_emit(a, arm64_movz_64(10, 10, 0));       // w10 = 10
    asm_label(a, "count");
    asm_emit_cbzw_to(a, 2, "count_done");
    asm_emit(a, arm64_add_imm_64(3, 3, 1, 0));   // x3++
    asm_emit(a, arm64_udiv_32(2, 2, 10));        // w2 /= 10
    asm_emit_b_to(a, "count");
    asm_label(a, "count_done");
    asm_emit(a, arm64_add_reg_64(4, 1, 3));      // x4 = x1 + x3
    asm_emit(a, arm64_strb_uimm(31, 4, 0));      // *x4 = 0
    asm_emit(a, arm64_add_imm_32(5, 0, 0));      // w5 = w0
    asm_label(a, "fill");
    asm_emit_cbzw_to(a, 5, "ret");
    asm_emit(a, arm64_udiv_32(6, 5, 10));        // w6 = w5 / 10
    asm_emit(a, arm64_mul_32(7, 6, 10));         // w7 = w6 * 10
    asm_emit(a, arm64_sub_reg_32(8, 5, 7));      // w8 = w5 - w7
    asm_emit(a, arm64_add_imm_32(8, 8, 48));     // w8 += '0'
    asm_emit(a, arm64_sub_imm_64(4, 4, 1));      // x4--
    asm_emit(a, arm64_strb_uimm(8, 4, 0));       // *x4 = digit
    asm_emit(a, arm64_add_imm_32(5, 6, 0));      // w5 = w6
    asm_emit_b_to(a, "fill");
    asm_label(a, "zero");
    asm_emit(a, arm64_strb_uimm(31, 1, 0));      // *x1 = 0
    asm_label(a, "ret");
    asm_emit(a, arm64_add_imm_64(0, 1, 0, 0));   // x0 = x1
    asm_emit(a, arm64_ret());
    asm_resolve_patches(a);
    return asm_to_bytes(a);
}

static std::vector<uint8_t> build_print_i64_bytes(uint64_t func_addr,
        uint64_t int_to_string_addr,
        uint64_t write_stub_addr,
        uint64_t nl_addr) {
    AsmBuilder a;
    asm_emit(a, arm64_sub_imm_64(31, 31, 64));   // sub sp, sp, #64
    asm_emit(a, arm64_add_imm_64(9, 0, 0, 0));   // x9 = x0
    asm_emit(a, arm64_add_imm_64(1, 31, 0, 0));  // x1 = sp
    asm_emit(a, arm64_add_imm_32(0, 9, 0));      // w0 = w9

    {
        const uint64_t at = func_addr + a.words.size() * 4;
        asm_emit(a, arm64_bl(arm64_bl_imm26_from_addrs(at, int_to_string_addr)));
    }

    asm_emit(a, arm64_add_imm_64(1, 0, 0, 0));   // x1 = x0
    asm_emit(a, arm64_movz_64(2, 0, 0));         // x2 = 0
    asm_emit(a, arm64_add_imm_64(4, 1, 0, 0));   // x4 = x1
    asm_label(a, "strlen");
    asm_emit(a, arm64_ldrb_uimm(3, 4, 0));       // w3 = *x4
    asm_emit_cbzw_to(a, 3, "strlen_done");
    asm_emit(a, arm64_add_imm_64(4, 4, 1, 0));   // x4++
    asm_emit(a, arm64_add_imm_64(2, 2, 1, 0));   // x2++
    asm_emit_b_to(a, "strlen");
    asm_label(a, "strlen_done");
    asm_emit(a, arm64_movz_64(0, 1, 0));         // x0 = 1

    {
        const uint64_t at = func_addr + a.words.size() * 4;
        asm_emit(a, arm64_bl(arm64_bl_imm26_from_addrs(at, write_stub_addr)));
    }

    asm_emit(a, arm64_movz_64(0, 1, 0));         // x0 = 1
    {
        const uint64_t at = func_addr + a.words.size() * 4;
        asm_emit(a, arm64_adrp(1, arm64_adrp_page_delta(at, nl_addr)));
    }
    asm_emit(a, arm64_add_imm_64(1, 1, (uint16_t)(nl_addr & 0xfffU), 0));
    asm_emit(a, arm64_movz_64(2, 1, 0));         // x2 = 1
    {
        const uint64_t at = func_addr + a.words.size() * 4;
        asm_emit(a, arm64_bl(arm64_bl_imm26_from_addrs(at, write_stub_addr)));
    }
    asm_emit(a, arm64_add_imm_64(31, 31, 64, 0)); // add sp, sp, #64
    asm_emit(a, arm64_ret());
    asm_resolve_patches(a);
    return asm_to_bytes(a);
}

static std::vector<uint8_t> build_main_bytes(const ProgramIR &program,
        uint64_t main_addr,
        uint64_t write_stub_addr,
        uint64_t exit_stub_addr,
        uint64_t print_i64_addr,
        const std::unordered_map<std::string, uint64_t> &global_addr) {
    std::vector<uint8_t> out;
    for (const Operation &op : program.ops) {
        if (op.kind == OpKind::WriteGlobal) {
            const auto it = global_addr.find(op.symbol);
            REQUIRE(it != global_addr.end());
            REQUIRE(op.value >= 0 && op.value <= 0xffff);
            const uint64_t str_addr = it->second;
            const uint64_t adrp_addr = main_addr + out.size() + 4;
            const uint64_t bl_addr = main_addr + out.size() + 16;
            emit_arm64(out, arm64_movz_64(0, 1, 0));
            emit_arm64(out, arm64_adrp(1, arm64_adrp_page_delta(adrp_addr, str_addr)));
            emit_arm64(out, arm64_add_imm_64(1, 1, (uint16_t)(str_addr & 0xfffU), 0));
            emit_arm64(out, arm64_movz_64(2, (uint16_t)op.value, 0));
            emit_arm64(out, arm64_bl(arm64_bl_imm26_from_addrs(bl_addr, write_stub_addr)));
            continue;
        }
        if (op.kind == OpKind::PrintI64) {
            REQUIRE(op.value >= 0 && op.value <= 0xffff);
            const uint64_t bl_addr = main_addr + out.size() + 4;
            emit_arm64(out, arm64_movz_64(0, (uint16_t)op.value, 0));
            emit_arm64(out, arm64_bl(arm64_bl_imm26_from_addrs(bl_addr, print_i64_addr)));
            continue;
        }
        if (op.kind == OpKind::ExitCode) {
            REQUIRE(op.value >= 0 && op.value <= 0xffff);
            const uint64_t bl_addr = main_addr + out.size() + 4;
            emit_arm64(out, arm64_movz_64(0, (uint16_t)op.value, 0));
            emit_arm64(out, arm64_bl(arm64_bl_imm26_from_addrs(bl_addr, exit_stub_addr)));
            continue;
        }
        if (op.kind == OpKind::ReturnCode) {
            REQUIRE(op.value >= 0 && op.value <= 0xffff);
            emit_arm64(out, arm64_movz_64(0, (uint16_t)op.value, 0));
            emit_arm64(out, arm64_ret());
            continue;
        }
        REQUIRE(false);
    }
    return out;
}

static std::vector<uint8_t> build_stub_bytes(uint64_t stubs_addr, uint64_t got_addr) {
    const uint64_t stub0_adrp_addr = stubs_addr + 0x0;
    const uint64_t stub1_adrp_addr = stubs_addr + 0xc;

    std::vector<uint8_t> out;
    emit_arm64(out, arm64_adrp(16, arm64_adrp_page_delta(stub0_adrp_addr, got_addr)));
    emit_arm64(out, arm64_ldr_imm_u64(16, 16, 0));
    emit_arm64(out, arm64_br(16));

    emit_arm64(out, arm64_adrp(16, arm64_adrp_page_delta(stub1_adrp_addr, got_addr)));
    emit_arm64(out, arm64_ldr_imm_u64(16, 16, 8));
    emit_arm64(out, arm64_br(16));
    REQUIRE(out.size() == 24);
    return out;
}

static std::vector<uint8_t> build_got_bytes() {
    std::vector<uint8_t> out;
    append_le64(out, 0x8010000000000000ULL);
    append_le64(out, 0x8000000000000001ULL);
    REQUIRE(out.size() == 16);
    return out;
}

static std::vector<uint8_t> build_chained_fixups_blob() {
    std::vector<uint8_t> out;
    append_le32(out, 0);
    append_le32(out, 0x20);
    append_le32(out, 0x50);
    append_le32(out, 0x58);
    append_le32(out, 2);
    append_le32(out, 1);
    append_le32(out, 0);

    append_le32(out, 0);
    append_le32(out, 4);
    append_le32(out, 0);
    append_le32(out, 0);
    append_le32(out, 0x18);
    append_le32(out, 0);
    append_le32(out, 0);

    append_le32(out, 0x18);
    append_le16(out, 0x4000);
    append_le16(out, 6);
    append_le64(out, 0x4000);
    append_le32(out, 0);
    append_le16(out, 1);
    append_le16(out, 0);

    append_le32(out, 0x00000201);
    append_le32(out, 0x00000e01);

    append_u8(out, 0x00);
    append_cstr(out, "_exit");
    append_cstr(out, "_write");
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    REQUIRE(out.size() == 104);
    return out;
}

static std::vector<uint8_t> build_exports_trie_blob() {
    std::vector<uint8_t> out;
    append_u8(out, 0x00);
    append_u8(out, 0x01);
    append_cstr(out, "_");
    append_uleb128(out, 0x12);
    append_u8(out, 0x00);
    append_u8(out, 0x00);
    append_u8(out, 0x00);

    append_u8(out, 0x00);
    append_u8(out, 0x02);
    append_u8(out, 0x00);
    append_u8(out, 0x00);
    append_u8(out, 0x00);
    append_u8(out, 0x03);
    append_u8(out, 0x00);
    append_uleb128(out, 0x410);
    append_u8(out, 0x00);

    append_u8(out, 0x00);
    append_u8(out, 0x02);
    append_cstr(out, "_mh_execute_header");
    append_uleb128(out, 0x09);
    append_cstr(out, "main");
    append_uleb128(out, 0x0d);
    append_u8(out, 0x00);
    append_u8(out, 0x00);
    REQUIRE(out.size() == 48);
    return out;
}

struct SymbolDef {
    std::string name;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

static void build_symbol_and_string_tables(std::vector<uint8_t> &symtab,
        std::vector<uint8_t> &indirect_syms,
        std::vector<uint8_t> &strtab,
        uint64_t msg_addr,
        uint64_t msg_len,
        uint64_t main_addr) {
    const std::vector<std::string> name_pool = {
        "__mh_execute_header", "_main", "_exit", "_write", "msg", "msg_len"
    };

    strtab.clear();
    strtab.push_back(0x20);
    strtab.push_back(0x00);

    std::unordered_map<std::string, uint32_t> strx;
    for (const auto &name : name_pool) {
        strx[name] = (uint32_t)strtab.size();
        append_cstr(strtab, name);
    }
    append_padding_to(strtab, 56);

    const std::vector<SymbolDef> symbols = {
        {"msg",                 0x0e, 3, 0x0000, msg_addr},
        {"msg_len",             0x02, 0, 0x0000, msg_len},
        {"__mh_execute_header", 0x0f, 1, 0x0010, 0x100000000ULL},
        {"_main",               0x0f, 1, 0x0000, main_addr},
        {"_exit",               0x01, 0, 0x0100, 0x0ULL},
        {"_write",              0x01, 0, 0x0100, 0x0ULL},
    };

    symtab.clear();
    for (const auto &s : symbols) {
        append_le32(symtab, strx[s.name]);
        append_u8(symtab, s.n_type);
        append_u8(symtab, s.n_sect);
        append_le16(symtab, s.n_desc);
        append_le64(symtab, s.n_value);
    }
    REQUIRE(symtab.size() == 96);

    indirect_syms.clear();
    append_le32(indirect_syms, 4);
    append_le32(indirect_syms, 5);
    append_le32(indirect_syms, 4);
    append_le32(indirect_syms, 5);
    REQUIRE(indirect_syms.size() == 16);
}

static std::vector<uint8_t> build_function_starts_blob() {
    std::vector<uint8_t> out;
    append_uleb128(out, 0x410);
    append_u8(out, 0x00);
    while (out.size() < 8) append_u8(out, 0x00);
    REQUIRE(out.size() == 8);
    return out;
}

static std::vector<uint8_t> build_code_signature_blob(
        const std::vector<uint8_t> &image, uint32_t code_limit) {
    REQUIRE(code_limit <= image.size());
    const uint32_t page_size = 4096;
    const uint32_t page_shift = 12;
    const std::string ident = "test_ir.x";
    const uint32_t n_code_slots = (code_limit + page_size - 1) / page_size;
    const uint32_t ident_offset = 88;
    const uint32_t hash_offset = ident_offset + (uint32_t)ident.size() + 1;
    const uint32_t cd_len = hash_offset + n_code_slots * CC_SHA256_DIGEST_LENGTH;

    std::vector<std::array<uint8_t, CC_SHA256_DIGEST_LENGTH>> page_hashes;
    page_hashes.reserve(n_code_slots);
    for (uint32_t i = 0; i < n_code_slots; i++) {
        const uint32_t start = i * page_size;
        const uint32_t remain = code_limit - start;
        const uint32_t len = remain < page_size ? remain : page_size;
        std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> digest = {};
        CC_SHA256(&image[start], len, digest.data());
        page_hashes.push_back(digest);
    }

    std::vector<uint8_t> cd;
    append_be32(cd, 0xfade0c02);
    append_be32(cd, cd_len);
    append_be32(cd, 0x00020400);
    append_be32(cd, 0x00020002);
    append_be32(cd, hash_offset);
    append_be32(cd, ident_offset);
    append_be32(cd, 0);
    append_be32(cd, n_code_slots);
    append_be32(cd, code_limit);
    append_u8(cd, 32);
    append_u8(cd, 2);
    append_u8(cd, 0);
    append_u8(cd, page_shift);
    append_be32(cd, 0);
    append_be32(cd, 0);
    append_be32(cd, 0);

    append_padding_to(cd, 76);
    append_be32(cd, 0x1c);
    append_be32(cd, 0x0);
    append_be32(cd, 0x1);
    REQUIRE(cd.size() == ident_offset);
    append_cstr(cd, ident);
    REQUIRE(cd.size() == hash_offset);

    for (const auto &h : page_hashes) cd.insert(cd.end(), h.begin(), h.end());
    REQUIRE(cd.size() == cd_len);

    std::vector<uint8_t> superblob;
    append_be32(superblob, 0xfade0cc0);
    append_be32(superblob, 20 + cd_len);
    append_be32(superblob, 1);
    append_be32(superblob, 0);
    append_be32(superblob, 20);
    superblob.insert(superblob.end(), cd.begin(), cd.end());
    append_padding_to(superblob, 408);
    REQUIRE(superblob.size() == 408);
    return superblob;
}

int main() {
    const ProgramIR program = parse_program_ir(kLlvmIr);
    validate_int_to_string_ir(program);
    validate_print_i64_ir(program);
    std::cout << "Parsed LLVM IR ops:\n";
    for (const Operation &op : program.ops) {
        if (op.kind == OpKind::WriteGlobal) std::cout << "  write(@" << op.symbol << ", len=" << op.value << ")\n";
        if (op.kind == OpKind::PrintI64) std::cout << "  print_i64(" << op.value << ")\n";
        if (op.kind == OpKind::ExitCode) std::cout << "  exit(" << op.value << ")\n";
        if (op.kind == OpKind::ReturnCode) std::cout << "  ret " << op.value << "\n";
    }

    const uint64_t text_fileoff = 1040;
    const uint64_t text_addr = 0x100000000ULL + text_fileoff;
    size_t main_size = 0;
    for (const Operation &op : program.ops) main_size += op_encoded_size(op);
    const size_t print_size = print_i64_encoded_size();
    const size_t int_to_string_size = int_to_string_encoded_size();
    const size_t text_size = main_size + print_size + int_to_string_size;
    const uint64_t main_addr = text_addr;
    const uint64_t print_addr = main_addr + main_size;
    const uint64_t int_to_string_addr = print_addr + print_size;
    const uint64_t stubs_addr = text_addr + text_size;
    const uint64_t cstring_addr = stubs_addr + 24;

    const CStringPlan cstring_plan = build_cstring_plan(program, cstring_addr);
    const auto it_nl = cstring_plan.addr.find("nl");
    REQUIRE(it_nl != cstring_plan.addr.end());
    const uint64_t nl_addr = it_nl->second;
    const uint64_t write_stub_addr = stubs_addr + 0xc;
    const uint64_t exit_stub_addr = stubs_addr + 0x0;

    const std::vector<uint8_t> main_text = build_main_bytes(program, main_addr, write_stub_addr, exit_stub_addr, print_addr, cstring_plan.addr);
    const std::vector<uint8_t> print_text = build_print_i64_bytes(print_addr, int_to_string_addr, write_stub_addr, nl_addr);
    const std::vector<uint8_t> int_to_string_text = build_int_to_string_bytes();
    REQUIRE(main_text.size() == main_size);
    REQUIRE(print_text.size() == print_size);
    REQUIRE(int_to_string_text.size() == int_to_string_size);

    std::vector<uint8_t> text;
    text.reserve(text_size);
    text.insert(text.end(), main_text.begin(), main_text.end());
    text.insert(text.end(), print_text.begin(), print_text.end());
    text.insert(text.end(), int_to_string_text.begin(), int_to_string_text.end());

    const std::vector<uint8_t> stubs = build_stub_bytes(stubs_addr, 0x100004000ULL);
    const std::vector<uint8_t> cstr = cstring_plan.bytes;
    const std::vector<uint8_t> got = build_got_bytes();

    std::vector<uint8_t> data;
    data.reserve(33512);

    {
        mach_header_64 h = {
            .magic = MH_MAGIC_64,
            .cputype = CPU_TYPE_ARM64,
            .cpusubtype = 0,
            .filetype = 2,
            .ncmds = 17,
            .sizeofcmds = 976,
            .flags = 2097285,
            .reserved = 0,
        };
        vec_append(data, &h, sizeof(h));
    }
    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64, .cmdsize = 72, .segname = "__PAGEZERO",
            .vmaddr = 0, .vmsize = 0x100000000ULL, .fileoff = 0, .filesize = 0,
            .maxprot = 0, .initprot = 0, .nsects = 0, .flags = 0,
        };
        vec_append(data, &s, sizeof(s));
    }
    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64, .cmdsize = 312, .segname = "__TEXT",
            .vmaddr = 0x100000000ULL, .vmsize = 0x4000, .fileoff = 0, .filesize = 16384,
            .maxprot = 5, .initprot = 5, .nsects = 3, .flags = 0,
        };
        vec_append(data, &s, sizeof(s));

        section_64 text_sect = {
            .sectname = "__text", .segname = "__TEXT",
            .addr = text_addr, .size = (uint64_t)text.size(), .offset = (uint32_t)text_fileoff,
            .align = 4, .reloff = 0, .nreloc = 0, .flags = 2147484672,
            .reserved1 = 0, .reserved2 = 0,
        };
        vec_append(data, &text_sect, sizeof(text_sect));

        section_64 stubs_sect = {
            .sectname = "__stubs", .segname = "__TEXT",
            .addr = stubs_addr, .size = (uint64_t)stubs.size(), .offset = (uint32_t)(text_fileoff + text.size()),
            .align = 2, .reloff = 0, .nreloc = 0, .flags = 2147484680,
            .reserved1 = 0, .reserved2 = 12,
        };
        vec_append(data, &stubs_sect, sizeof(stubs_sect));

        section_64 cstring_sect = {
            .sectname = "__cstring", .segname = "__TEXT",
            .addr = cstring_addr, .size = (uint64_t)cstr.size(), .offset = (uint32_t)(text_fileoff + text.size() + stubs.size()),
            .align = 0, .reloff = 0, .nreloc = 0, .flags = 2,
            .reserved1 = 0, .reserved2 = 0,
        };
        vec_append(data, &cstring_sect, sizeof(cstring_sect));
    }
    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64, .cmdsize = 152, .segname = "__DATA_CONST",
            .vmaddr = 0x100004000ULL, .vmsize = 0x4000, .fileoff = 16384, .filesize = 16384,
            .maxprot = 3, .initprot = 3, .nsects = 1, .flags = 16,
        };
        vec_append(data, &s, sizeof(s));
        section_64 got_sect = {
            .sectname = "__got", .segname = "__DATA_CONST",
            .addr = 0x100004000ULL, .size = 0x10, .offset = 16384,
            .align = 3, .reloff = 0, .nreloc = 0, .flags = 6,
            .reserved1 = 2, .reserved2 = 0,
        };
        vec_append(data, &got_sect, sizeof(got_sect));
    }
    {
        segment_command_64 s = {
            .cmd = LC_SEGMENT_64, .cmdsize = 72, .segname = "__LINKEDIT",
            .vmaddr = 0x100008000ULL, .vmsize = 0x4000, .fileoff = 32768, .filesize = 744,
            .maxprot = 1, .initprot = 1, .nsects = 0, .flags = 0,
        };
        vec_append(data, &s, sizeof(s));
    }

    { section_offset_len s = {.cmd = LC_DYLD_CHAINED_FIXUPS, .cmdsize = 16, .offset = 32768, .len = 104}; vec_append(data, &s, sizeof(s)); }
    { section_offset_len s = {.cmd = LC_DYLD_EXPORTS_TRIE, .cmdsize = 16, .offset = 32872, .len = 48}; vec_append(data, &s, sizeof(s)); }
    { symtab_command s = {.cmd = LC_SYMTAB, .cmdsize = 24, .symoff = 32928, .nsyms = 6, .stroff = 33040, .strsize = 56}; vec_append(data, &s, sizeof(s)); }
    {
        dysymtab_command s = {
            .cmd = LC_DYSYMTAB, .cmdsize = 80, .ilocalsym = 0, .nlocalsym = 2,
            .iextdefsym = 2, .nextdefsym = 2, .iundefsym = 4, .nundefsym = 2,
            .tocoff = 0, .ntoc = 0, .modtaboff = 0, .nmodtab = 0,
            .extrefsymoff = 0, .nextrefsyms = 0, .indirectsymoff = 33024, .nindirectsyms = 4,
            .extreloff = 0, .nextrel = 0, .locreloff = 0, .nlocrel = 0,
        };
        vec_append(data, &s, sizeof(s));
    }
    {
        dylinker_command s = {.cmd = LC_LOAD_DYLINKER, .cmdsize = 32, .name = {.offset = 12}};
        vec_append(data, &s, sizeof(s));
        const char dyld_name[20] = "/usr/lib/dyld";
        vec_append(data, dyld_name, sizeof(dyld_name));
    }
    {
        uuid_command s = {
            .cmd = LC_UUID, .cmdsize = 24,
            .uuid = {0x27, 0x07, 0xdd, 0x62, 0x09, 0x67, 0x3c, 0xc0, 0xb2, 0xac, 0xef, 0xc3, 0x2b, 0x1c, 0xf6, 0x3a},
        };
        vec_append(data, &s, sizeof(s));
    }
    {
        build_version_command s = {.cmd = LC_BUILD_VERSION, .cmdsize = 32, .platform = 1, .minos = 0x000f0700, .sdk = 0, .ntools = 1};
        vec_append(data, &s, sizeof(s));
        append_le32(data, 3);
        append_le32(data, 0x04ce0100);
    }
    { source_version_command s = {.cmd = LC_SOURCE_VERSION, .cmdsize = 16, .version = 0}; vec_append(data, &s, sizeof(s)); }
    { entry_point_command s = {.cmd = LC_MAIN, .cmdsize = 24, .entryoff = text_fileoff, .stacksize = 0}; vec_append(data, &s, sizeof(s)); }
    {
        dylib_command s = {
            .cmd = LC_LOAD_DYLIB, .cmdsize = 56,
            .dylib = {.name = {.offset = 24}, .timestamp = 2, .current_version = 0x054c0000, .compatibility_version = 0x00010000},
        };
        vec_append(data, &s, sizeof(s));
        const char libsystem_name[32] = "/usr/lib/libSystem.B.dylib";
        vec_append(data, libsystem_name, sizeof(libsystem_name));
    }
    { section_offset_len s = {.cmd = LC_FUNCTION_STARTS, .cmdsize = 16, .offset = 32920, .len = 8}; vec_append(data, &s, sizeof(s)); }
    { section_offset_len s = {.cmd = LC_DATA_IN_CODE, .cmdsize = 16, .offset = 32928, .len = 0}; vec_append(data, &s, sizeof(s)); }
    { section_offset_len s = {.cmd = LC_CODE_SIGNATURE, .cmdsize = 16, .offset = 33104, .len = 408}; vec_append(data, &s, sizeof(s)); }

    REQUIRE(data.size() == 1008);
    append_padding_to(data, text_fileoff);
    data.insert(data.end(), text.begin(), text.end());
    data.insert(data.end(), stubs.begin(), stubs.end());
    data.insert(data.end(), cstr.begin(), cstr.end());
    append_padding_to(data, 16384);
    data.insert(data.end(), got.begin(), got.end());
    append_padding_to(data, 32768);

    const std::vector<uint8_t> chained_fixups = build_chained_fixups_blob();
    const std::vector<uint8_t> exports_trie = build_exports_trie_blob();
    const std::vector<uint8_t> function_starts = build_function_starts_blob();
    std::vector<uint8_t> symtab;
    std::vector<uint8_t> indirect_syms;
    std::vector<uint8_t> strtab;
    const auto it_prefix = cstring_plan.addr.find("prefix");
    REQUIRE(it_prefix != cstring_plan.addr.end());
    const uint64_t msg_addr = it_prefix->second;
    const uint64_t msg_len = 23;
    build_symbol_and_string_tables(symtab, indirect_syms, strtab, msg_addr, msg_len, text_addr);
    data.insert(data.end(), chained_fixups.begin(), chained_fixups.end());
    data.insert(data.end(), exports_trie.begin(), exports_trie.end());
    data.insert(data.end(), function_starts.begin(), function_starts.end());
    data.insert(data.end(), symtab.begin(), symtab.end());
    data.insert(data.end(), indirect_syms.begin(), indirect_syms.end());
    data.insert(data.end(), strtab.begin(), strtab.end());
    append_padding_to(data, 33104);

    const std::vector<uint8_t> codesig = build_code_signature_blob(data, 33104);
    data.insert(data.end(), codesig.begin(), codesig.end());
    REQUIRE(data.size() == 33512);

    write_file("test_ir.x", data);
    std::cout << "Wrote test_ir.x\n";
}
