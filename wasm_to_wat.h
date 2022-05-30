#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>

// #define WAT_DEBUG

#ifdef WAT_DEBUG
#define DEBUG(s) std::cout << s << std::endl
#else
#define DEBUG(s)
#endif

// this is temporary, we may not need this when we integrate with LFortran
namespace LFortran{
    std::string LFortranException(std::string msg){
        return "LFortranException: " + msg;
    }
}

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
    uint32_t insts_start_index;
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

int32_t read_signed_num(uint32_t &offset) {
    return decode_signed_leb128(offset);
}

uint32_t read_unsigned_num(uint32_t &offset) {
    return decode_unsigned_leb128(offset);
}

template <class Derived>
class BaseWASMVisitor {
   private:
    Derived& self() { return static_cast<Derived&>(*this); }

   public:
    void visit_Unreachable() { throw LFortran::LFortranException("visit_Unreachable() not implemented"); }

    void visit_Nop() { throw LFortran::LFortranException("visit_Nop() not implemented"); }

    void visit_Block() { throw LFortran::LFortranException("visit_Block() not implemented"); }

    void visit_Loop() { throw LFortran::LFortranException("visit_Loop() not implemented"); }

    void visit_If() { throw LFortran::LFortranException("visit_If() not implemented"); }

    void visit_Else() { throw LFortran::LFortranException("visit_Else() not implemented"); }

    void visit_Br(uint32_t /*labelidx*/) { throw LFortran::LFortranException("visit_Br() not implemented"); }

    void visit_BrIf(uint32_t /*labelidx*/) { throw LFortran::LFortranException("visit_BrIf() not implemented"); }

    void visit_BrTable(std::vector<uint32_t>& /*label_indices*/, uint32_t /*labelidx*/) { throw LFortran::LFortranException("visit_BrTable() not implemented"); }

    void visit_Return() { throw LFortran::LFortranException("visit_Return() not implemented"); }

    void visit_Call(uint32_t /*funcidx*/) { throw LFortran::LFortranException("visit_Call() not implemented"); }

    void visit_CallIndirect(uint32_t /*typeidx*/, uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_CallIndirect() not implemented"); }

    void visit_RefNull(uint8_t /*ref_type*/) { throw LFortran::LFortranException("visit_RefNull() not implemented"); }

    void visit_RefIsNull() { throw LFortran::LFortranException("visit_RefIsNull() not implemented"); }

    void visit_RefFunc(uint32_t /*funcidx*/) { throw LFortran::LFortranException("visit_RefFunc() not implemented"); }

    void visit_Drop() { throw LFortran::LFortranException("visit_Drop() not implemented"); }

    void visit_Select() { throw LFortran::LFortranException("visit_Select() not implemented"); }

    void visit_Select(std::vector<uint8_t>& /*val_types*/) { throw LFortran::LFortranException("visit_Select() not implemented"); }

    void visit_LocalGet(uint32_t /*localidx*/) { throw LFortran::LFortranException("visit_LocalGet() not implemented"); }

    void visit_LocalSet(uint32_t /*localidx*/) { throw LFortran::LFortranException("visit_LocalSet() not implemented"); }

    void visit_LocalTee(uint32_t /*localidx*/) { throw LFortran::LFortranException("visit_LocalTee() not implemented"); }

    void visit_GlobalGet(uint32_t /*globalidx*/) { throw LFortran::LFortranException("visit_GlobalGet() not implemented"); }

    void visit_GlobalSet(uint32_t /*globalidx*/) { throw LFortran::LFortranException("visit_GlobalSet() not implemented"); }

    void visit_TableGet(uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableGet() not implemented"); }

    void visit_TableSet(uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableSet() not implemented"); }

    void visit_TableInit(uint32_t /*elemidx*/, uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableInit() not implemented"); }

    void visit_ElemDrop(uint32_t /*elemidx*/) { throw LFortran::LFortranException("visit_ElemDrop() not implemented"); }

    void visit_TableCopy(uint32_t /*tableidx*/, uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableCopy() not implemented"); }

    void visit_TableGrow(uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableGrow() not implemented"); }

    void visit_TableSize(uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableSize() not implemented"); }

    void visit_TableFill(uint32_t /*tableidx*/) { throw LFortran::LFortranException("visit_TableFill() not implemented"); }

    void visit_I32Load(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Load() not implemented"); }

    void visit_I64Load(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load() not implemented"); }

    void visit_F32Load(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_F32Load() not implemented"); }

    void visit_F64Load(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_F64Load() not implemented"); }

    void visit_I32Load8S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Load8S() not implemented"); }

    void visit_I32Load8U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Load8U() not implemented"); }

    void visit_I32Load16S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Load16S() not implemented"); }

    void visit_I32Load16U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Load16U() not implemented"); }

    void visit_I64Load8S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load8S() not implemented"); }

    void visit_I64Load8U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load8U() not implemented"); }

    void visit_I64Load16S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load16S() not implemented"); }

    void visit_I64Load16U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load16U() not implemented"); }

    void visit_I64Load32S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load32S() not implemented"); }

    void visit_I64Load32U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Load32U() not implemented"); }

    void visit_I32Store(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Store() not implemented"); }

    void visit_I64Store(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Store() not implemented"); }

    void visit_F32Store(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_F32Store() not implemented"); }

    void visit_F64Store(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_F64Store() not implemented"); }

    void visit_I32Store8(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Store8() not implemented"); }

    void visit_I32Store16(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I32Store16() not implemented"); }

    void visit_I64Store8(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Store8() not implemented"); }

    void visit_I64Store16(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Store16() not implemented"); }

    void visit_I64Store32(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_I64Store32() not implemented"); }

    void visit_MemorySize() { throw LFortran::LFortranException("visit_MemorySize() not implemented"); }

    void visit_MemoryGrow() { throw LFortran::LFortranException("visit_MemoryGrow() not implemented"); }

    void visit_MemoryInit(uint32_t /*dataidx*/) { throw LFortran::LFortranException("visit_MemoryInit() not implemented"); }

    void visit_DataDrop(uint32_t /*dataidx*/) { throw LFortran::LFortranException("visit_DataDrop() not implemented"); }

    void visit_MemoryCopy() { throw LFortran::LFortranException("visit_MemoryCopy() not implemented"); }

    void visit_MemoryFill() { throw LFortran::LFortranException("visit_MemoryFill() not implemented"); }

    void visit_I32Const(uint32_t /*n*/) { throw LFortran::LFortranException("visit_I32Const() not implemented"); }

    void visit_I64Const(uint64_t /*n*/) { throw LFortran::LFortranException("visit_I64Const() not implemented"); }

    void visit_F32Const(float /*z*/) { throw LFortran::LFortranException("visit_F32Const() not implemented"); }

    void visit_F64Const(double /*z*/) { throw LFortran::LFortranException("visit_F64Const() not implemented"); }

    void visit_I32Eqz() { throw LFortran::LFortranException("visit_I32Eqz() not implemented"); }

    void visit_I32Eq() { throw LFortran::LFortranException("visit_I32Eq() not implemented"); }

    void visit_I32Ne() { throw LFortran::LFortranException("visit_I32Ne() not implemented"); }

    void visit_I32LtS() { throw LFortran::LFortranException("visit_I32LtS() not implemented"); }

    void visit_I32LtU() { throw LFortran::LFortranException("visit_I32LtU() not implemented"); }

    void visit_I32GtS() { throw LFortran::LFortranException("visit_I32GtS() not implemented"); }

    void visit_I32GtU() { throw LFortran::LFortranException("visit_I32GtU() not implemented"); }

    void visit_I32LeS() { throw LFortran::LFortranException("visit_I32LeS() not implemented"); }

    void visit_I32LeU() { throw LFortran::LFortranException("visit_I32LeU() not implemented"); }

    void visit_I32GeS() { throw LFortran::LFortranException("visit_I32GeS() not implemented"); }

    void visit_I32GeU() { throw LFortran::LFortranException("visit_I32GeU() not implemented"); }

    void visit_I64Eqz() { throw LFortran::LFortranException("visit_I64Eqz() not implemented"); }

    void visit_I64Eq() { throw LFortran::LFortranException("visit_I64Eq() not implemented"); }

    void visit_I64Ne() { throw LFortran::LFortranException("visit_I64Ne() not implemented"); }

    void visit_I64LtS() { throw LFortran::LFortranException("visit_I64LtS() not implemented"); }

    void visit_I64LtU() { throw LFortran::LFortranException("visit_I64LtU() not implemented"); }

    void visit_I64GtS() { throw LFortran::LFortranException("visit_I64GtS() not implemented"); }

    void visit_I64GtU() { throw LFortran::LFortranException("visit_I64GtU() not implemented"); }

    void visit_I64LeS() { throw LFortran::LFortranException("visit_I64LeS() not implemented"); }

    void visit_I64LeU() { throw LFortran::LFortranException("visit_I64LeU() not implemented"); }

    void visit_I64GeS() { throw LFortran::LFortranException("visit_I64GeS() not implemented"); }

    void visit_I64GeU() { throw LFortran::LFortranException("visit_I64GeU() not implemented"); }

    void visit_F32Eq() { throw LFortran::LFortranException("visit_F32Eq() not implemented"); }

    void visit_F32Ne() { throw LFortran::LFortranException("visit_F32Ne() not implemented"); }

    void visit_F32Lt() { throw LFortran::LFortranException("visit_F32Lt() not implemented"); }

    void visit_F32Gt() { throw LFortran::LFortranException("visit_F32Gt() not implemented"); }

    void visit_F32Le() { throw LFortran::LFortranException("visit_F32Le() not implemented"); }

    void visit_F32Ge() { throw LFortran::LFortranException("visit_F32Ge() not implemented"); }

    void visit_F64Eq() { throw LFortran::LFortranException("visit_F64Eq() not implemented"); }

    void visit_F64Ne() { throw LFortran::LFortranException("visit_F64Ne() not implemented"); }

    void visit_F64Lt() { throw LFortran::LFortranException("visit_F64Lt() not implemented"); }

    void visit_F64Gt() { throw LFortran::LFortranException("visit_F64Gt() not implemented"); }

    void visit_F64Le() { throw LFortran::LFortranException("visit_F64Le() not implemented"); }

    void visit_F64Ge() { throw LFortran::LFortranException("visit_F64Ge() not implemented"); }

    void visit_I32Clz() { throw LFortran::LFortranException("visit_I32Clz() not implemented"); }

    void visit_I32Ctz() { throw LFortran::LFortranException("visit_I32Ctz() not implemented"); }

    void visit_I32Popcnt() { throw LFortran::LFortranException("visit_I32Popcnt() not implemented"); }

    void visit_I32Add() { throw LFortran::LFortranException("visit_I32Add() not implemented"); }

    void visit_I32Sub() { throw LFortran::LFortranException("visit_I32Sub() not implemented"); }

    void visit_I32Mul() { throw LFortran::LFortranException("visit_I32Mul() not implemented"); }

    void visit_I32DivS() { throw LFortran::LFortranException("visit_I32DivS() not implemented"); }

    void visit_I32DivU() { throw LFortran::LFortranException("visit_I32DivU() not implemented"); }

    void visit_I32RemS() { throw LFortran::LFortranException("visit_I32RemS() not implemented"); }

    void visit_I32RemU() { throw LFortran::LFortranException("visit_I32RemU() not implemented"); }

    void visit_I32And() { throw LFortran::LFortranException("visit_I32And() not implemented"); }

    void visit_I32Or() { throw LFortran::LFortranException("visit_I32Or() not implemented"); }

    void visit_I32Xor() { throw LFortran::LFortranException("visit_I32Xor() not implemented"); }

    void visit_I32Shl() { throw LFortran::LFortranException("visit_I32Shl() not implemented"); }

    void visit_I32ShrS() { throw LFortran::LFortranException("visit_I32ShrS() not implemented"); }

    void visit_I32ShrU() { throw LFortran::LFortranException("visit_I32ShrU() not implemented"); }

    void visit_I32Rotl() { throw LFortran::LFortranException("visit_I32Rotl() not implemented"); }

    void visit_I32Rotr() { throw LFortran::LFortranException("visit_I32Rotr() not implemented"); }

    void visit_I64Clz() { throw LFortran::LFortranException("visit_I64Clz() not implemented"); }

    void visit_I64Ctz() { throw LFortran::LFortranException("visit_I64Ctz() not implemented"); }

    void visit_I64Popcnt() { throw LFortran::LFortranException("visit_I64Popcnt() not implemented"); }

    void visit_I64Add() { throw LFortran::LFortranException("visit_I64Add() not implemented"); }

    void visit_I64Sub() { throw LFortran::LFortranException("visit_I64Sub() not implemented"); }

    void visit_I64Mul() { throw LFortran::LFortranException("visit_I64Mul() not implemented"); }

    void visit_I64DivS() { throw LFortran::LFortranException("visit_I64DivS() not implemented"); }

    void visit_I64DivU() { throw LFortran::LFortranException("visit_I64DivU() not implemented"); }

    void visit_I64RemS() { throw LFortran::LFortranException("visit_I64RemS() not implemented"); }

    void visit_I64RemU() { throw LFortran::LFortranException("visit_I64RemU() not implemented"); }

    void visit_I64And() { throw LFortran::LFortranException("visit_I64And() not implemented"); }

    void visit_I64Or() { throw LFortran::LFortranException("visit_I64Or() not implemented"); }

    void visit_I64Xor() { throw LFortran::LFortranException("visit_I64Xor() not implemented"); }

    void visit_I64Shl() { throw LFortran::LFortranException("visit_I64Shl() not implemented"); }

    void visit_I64ShrS() { throw LFortran::LFortranException("visit_I64ShrS() not implemented"); }

    void visit_I64ShrU() { throw LFortran::LFortranException("visit_I64ShrU() not implemented"); }

    void visit_I64Rotl() { throw LFortran::LFortranException("visit_I64Rotl() not implemented"); }

    void visit_I64Rotr() { throw LFortran::LFortranException("visit_I64Rotr() not implemented"); }

    void visit_F32Abs() { throw LFortran::LFortranException("visit_F32Abs() not implemented"); }

    void visit_F32Neg() { throw LFortran::LFortranException("visit_F32Neg() not implemented"); }

    void visit_F32Ceil() { throw LFortran::LFortranException("visit_F32Ceil() not implemented"); }

    void visit_F32Floor() { throw LFortran::LFortranException("visit_F32Floor() not implemented"); }

    void visit_F32Trunc() { throw LFortran::LFortranException("visit_F32Trunc() not implemented"); }

    void visit_F32Nearest() { throw LFortran::LFortranException("visit_F32Nearest() not implemented"); }

    void visit_F32Sqrt() { throw LFortran::LFortranException("visit_F32Sqrt() not implemented"); }

    void visit_F32Add() { throw LFortran::LFortranException("visit_F32Add() not implemented"); }

    void visit_F32Sub() { throw LFortran::LFortranException("visit_F32Sub() not implemented"); }

    void visit_F32Mul() { throw LFortran::LFortranException("visit_F32Mul() not implemented"); }

    void visit_F32Div() { throw LFortran::LFortranException("visit_F32Div() not implemented"); }

    void visit_F32Min() { throw LFortran::LFortranException("visit_F32Min() not implemented"); }

    void visit_F32Max() { throw LFortran::LFortranException("visit_F32Max() not implemented"); }

    void visit_F32Copysign() { throw LFortran::LFortranException("visit_F32Copysign() not implemented"); }

    void visit_F64Abs() { throw LFortran::LFortranException("visit_F64Abs() not implemented"); }

    void visit_F64Neg() { throw LFortran::LFortranException("visit_F64Neg() not implemented"); }

    void visit_F64Ceil() { throw LFortran::LFortranException("visit_F64Ceil() not implemented"); }

    void visit_F64Floor() { throw LFortran::LFortranException("visit_F64Floor() not implemented"); }

    void visit_F64Trunc() { throw LFortran::LFortranException("visit_F64Trunc() not implemented"); }

    void visit_F64Nearest() { throw LFortran::LFortranException("visit_F64Nearest() not implemented"); }

    void visit_F64Sqrt() { throw LFortran::LFortranException("visit_F64Sqrt() not implemented"); }

    void visit_F64Add() { throw LFortran::LFortranException("visit_F64Add() not implemented"); }

    void visit_F64Sub() { throw LFortran::LFortranException("visit_F64Sub() not implemented"); }

    void visit_F64Mul() { throw LFortran::LFortranException("visit_F64Mul() not implemented"); }

    void visit_F64Div() { throw LFortran::LFortranException("visit_F64Div() not implemented"); }

    void visit_F64Min() { throw LFortran::LFortranException("visit_F64Min() not implemented"); }

    void visit_F64Max() { throw LFortran::LFortranException("visit_F64Max() not implemented"); }

    void visit_F64Copysign() { throw LFortran::LFortranException("visit_F64Copysign() not implemented"); }

    void visit_I32WrapI64() { throw LFortran::LFortranException("visit_I32WrapI64() not implemented"); }

    void visit_I32TruncF32S() { throw LFortran::LFortranException("visit_I32TruncF32S() not implemented"); }

    void visit_I32TruncF32U() { throw LFortran::LFortranException("visit_I32TruncF32U() not implemented"); }

    void visit_I32TruncF64S() { throw LFortran::LFortranException("visit_I32TruncF64S() not implemented"); }

    void visit_I32TruncF64U() { throw LFortran::LFortranException("visit_I32TruncF64U() not implemented"); }

    void visit_I64ExtendI32S() { throw LFortran::LFortranException("visit_I64ExtendI32S() not implemented"); }

    void visit_I64ExtendI32U() { throw LFortran::LFortranException("visit_I64ExtendI32U() not implemented"); }

    void visit_I64TruncF32S() { throw LFortran::LFortranException("visit_I64TruncF32S() not implemented"); }

    void visit_I64TruncF32U() { throw LFortran::LFortranException("visit_I64TruncF32U() not implemented"); }

    void visit_I64TruncF64S() { throw LFortran::LFortranException("visit_I64TruncF64S() not implemented"); }

    void visit_I64TruncF64U() { throw LFortran::LFortranException("visit_I64TruncF64U() not implemented"); }

    void visit_F32ConvertI32S() { throw LFortran::LFortranException("visit_F32ConvertI32S() not implemented"); }

    void visit_F32ConvertI32U() { throw LFortran::LFortranException("visit_F32ConvertI32U() not implemented"); }

    void visit_F32ConvertI64S() { throw LFortran::LFortranException("visit_F32ConvertI64S() not implemented"); }

    void visit_F32ConvertI64U() { throw LFortran::LFortranException("visit_F32ConvertI64U() not implemented"); }

    void visit_F32DemoteF64() { throw LFortran::LFortranException("visit_F32DemoteF64() not implemented"); }

    void visit_F64ConvertI32S() { throw LFortran::LFortranException("visit_F64ConvertI32S() not implemented"); }

    void visit_F64ConvertI32U() { throw LFortran::LFortranException("visit_F64ConvertI32U() not implemented"); }

    void visit_F64ConvertI64S() { throw LFortran::LFortranException("visit_F64ConvertI64S() not implemented"); }

    void visit_F64ConvertI64U() { throw LFortran::LFortranException("visit_F64ConvertI64U() not implemented"); }

    void visit_F64PromoteF32() { throw LFortran::LFortranException("visit_F64PromoteF32() not implemented"); }

    void visit_I32ReinterpretF32() { throw LFortran::LFortranException("visit_I32ReinterpretF32() not implemented"); }

    void visit_I64ReinterpretF64() { throw LFortran::LFortranException("visit_I64ReinterpretF64() not implemented"); }

    void visit_F32ReinterpretI32() { throw LFortran::LFortranException("visit_F32ReinterpretI32() not implemented"); }

    void visit_F64ReinterpretI64() { throw LFortran::LFortranException("visit_F64ReinterpretI64() not implemented"); }

    void visit_I32Extend8S() { throw LFortran::LFortranException("visit_I32Extend8S() not implemented"); }

    void visit_I32Extend16S() { throw LFortran::LFortranException("visit_I32Extend16S() not implemented"); }

    void visit_I64Extend8S() { throw LFortran::LFortranException("visit_I64Extend8S() not implemented"); }

    void visit_I64Extend16S() { throw LFortran::LFortranException("visit_I64Extend16S() not implemented"); }

    void visit_I64Extend32S() { throw LFortran::LFortranException("visit_I64Extend32S() not implemented"); }

    void visit_I32TruncSatF32S() { throw LFortran::LFortranException("visit_I32TruncSatF32S() not implemented"); }

    void visit_I32TruncSatF32U() { throw LFortran::LFortranException("visit_I32TruncSatF32U() not implemented"); }

    void visit_I32TruncSatF64S() { throw LFortran::LFortranException("visit_I32TruncSatF64S() not implemented"); }

    void visit_I32TruncSatF64U() { throw LFortran::LFortranException("visit_I32TruncSatF64U() not implemented"); }

    void visit_I64TruncSatF32S() { throw LFortran::LFortranException("visit_I64TruncSatF32S() not implemented"); }

    void visit_I64TruncSatF32U() { throw LFortran::LFortranException("visit_I64TruncSatF32U() not implemented"); }

    void visit_I64TruncSatF64S() { throw LFortran::LFortranException("visit_I64TruncSatF64S() not implemented"); }

    void visit_I64TruncSatF64U() { throw LFortran::LFortranException("visit_I64TruncSatF64U() not implemented"); }

    void visit_V128Load(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load() not implemented"); }

    void visit_V128Load8x8S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load8x8S() not implemented"); }

    void visit_V128Load8x8U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load8x8U() not implemented"); }

    void visit_V128Load16x4S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load16x4S() not implemented"); }

    void visit_V128Load16x4U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load16x4U() not implemented"); }

    void visit_V128Load32x2S(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load32x2S() not implemented"); }

    void visit_V128Load32x2U(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load32x2U() not implemented"); }

    void visit_V128Load8Splat(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load8Splat() not implemented"); }

    void visit_V128Load16Splat(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load16Splat() not implemented"); }

    void visit_V128Load32Splat(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load32Splat() not implemented"); }

    void visit_V128Load64Splat(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load64Splat() not implemented"); }

    void visit_V128Load32Zero(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load32Zero() not implemented"); }

    void visit_V128Load64Zero(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Load64Zero() not implemented"); }

    void visit_V128Store(uint32_t /*mem_align*/, uint32_t /*mem_offset*/) { throw LFortran::LFortranException("visit_V128Store() not implemented"); }

    void visit_V128Load8Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_V128Load8Lane() not implemented"); }

    void visit_V128Load16Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_V128Load16Lane() not implemented"); }

    void visit_V128Load32Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_V128Load32Lane() not implemented"); }

    void visit_V128Load64Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_V128Load64Lane() not implemented"); }

    void visit_V128Store8Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_V128Store8Lane() not implemented"); }

    void visit_V128Store16Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) {
        throw LFortran::LFortranException("visit_V128Store16Lane() not implemented");
    }

    void visit_V128Store32Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) {
        throw LFortran::LFortranException("visit_V128Store32Lane() not implemented");
    }

    void visit_V128Store64Lane(uint32_t /*mem_align*/, uint32_t /*mem_offset*/, uint8_t /*laneidx*/) {
        throw LFortran::LFortranException("visit_V128Store64Lane() not implemented");
    }

    void visit_V128Const(std::vector<uint8_t>& /*immediate_bytes16*/) { throw LFortran::LFortranException("visit_V128Const() not implemented"); }

    void visit_I8x16Shuffle(std::vector<uint8_t>& /*immediate_lane_indices16*/) { throw LFortran::LFortranException("visit_I8x16Shuffle() not implemented"); }

    void visit_I8x16ExtractLaneS(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I8x16ExtractLaneS() not implemented"); }

    void visit_I8x16ExtractLaneU(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I8x16ExtractLaneU() not implemented"); }

    void visit_I8x16ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I8x16ReplaceLane() not implemented"); }

    void visit_I16x8ExtractLaneS(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I16x8ExtractLaneS() not implemented"); }

    void visit_I16x8ExtractLaneU(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I16x8ExtractLaneU() not implemented"); }

    void visit_I16x8ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I16x8ReplaceLane() not implemented"); }

    void visit_I32x4ExtractLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I32x4ExtractLane() not implemented"); }

    void visit_I32x4ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I32x4ReplaceLane() not implemented"); }

    void visit_I64x2ExtractLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I64x2ExtractLane() not implemented"); }

    void visit_I64x2ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_I64x2ReplaceLane() not implemented"); }

    void visit_F32x4ExtractLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_F32x4ExtractLane() not implemented"); }

    void visit_F32x4ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_F32x4ReplaceLane() not implemented"); }

    void visit_F64x2ExtractLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_F64x2ExtractLane() not implemented"); }

    void visit_F64x2ReplaceLane(uint8_t /*laneidx*/) { throw LFortran::LFortranException("visit_F64x2ReplaceLane() not implemented"); }

    void visit_I8x16Swizzle() { throw LFortran::LFortranException("visit_I8x16Swizzle() not implemented"); }

    void visit_I8x16Splat() { throw LFortran::LFortranException("visit_I8x16Splat() not implemented"); }

    void visit_I16x8Splat() { throw LFortran::LFortranException("visit_I16x8Splat() not implemented"); }

    void visit_I32x4Splat() { throw LFortran::LFortranException("visit_I32x4Splat() not implemented"); }

    void visit_I64x2Splat() { throw LFortran::LFortranException("visit_I64x2Splat() not implemented"); }

    void visit_F32x4Splat() { throw LFortran::LFortranException("visit_F32x4Splat() not implemented"); }

    void visit_F64x2Splat() { throw LFortran::LFortranException("visit_F64x2Splat() not implemented"); }

    void visit_I8x16Eq() { throw LFortran::LFortranException("visit_I8x16Eq() not implemented"); }

    void visit_I8x16Ne() { throw LFortran::LFortranException("visit_I8x16Ne() not implemented"); }

    void visit_I8x16LtS() { throw LFortran::LFortranException("visit_I8x16LtS() not implemented"); }

    void visit_I8x16LtU() { throw LFortran::LFortranException("visit_I8x16LtU() not implemented"); }

    void visit_I8x16GtS() { throw LFortran::LFortranException("visit_I8x16GtS() not implemented"); }

    void visit_I8x16GtU() { throw LFortran::LFortranException("visit_I8x16GtU() not implemented"); }

    void visit_I8x16LeS() { throw LFortran::LFortranException("visit_I8x16LeS() not implemented"); }

    void visit_I8x16LeU() { throw LFortran::LFortranException("visit_I8x16LeU() not implemented"); }

    void visit_I8x16GeS() { throw LFortran::LFortranException("visit_I8x16GeS() not implemented"); }

    void visit_I8x16GeU() { throw LFortran::LFortranException("visit_I8x16GeU() not implemented"); }

    void visit_I16x8Eq() { throw LFortran::LFortranException("visit_I16x8Eq() not implemented"); }

    void visit_I16x8Ne() { throw LFortran::LFortranException("visit_I16x8Ne() not implemented"); }

    void visit_I16x8LtS() { throw LFortran::LFortranException("visit_I16x8LtS() not implemented"); }

    void visit_I16x8LtU() { throw LFortran::LFortranException("visit_I16x8LtU() not implemented"); }

    void visit_I16x8GtS() { throw LFortran::LFortranException("visit_I16x8GtS() not implemented"); }

    void visit_I16x8GtU() { throw LFortran::LFortranException("visit_I16x8GtU() not implemented"); }

    void visit_I16x8LeS() { throw LFortran::LFortranException("visit_I16x8LeS() not implemented"); }

    void visit_I16x8LeU() { throw LFortran::LFortranException("visit_I16x8LeU() not implemented"); }

    void visit_I16x8GeS() { throw LFortran::LFortranException("visit_I16x8GeS() not implemented"); }

    void visit_I16x8GeU() { throw LFortran::LFortranException("visit_I16x8GeU() not implemented"); }

    void visit_I32x4Eq() { throw LFortran::LFortranException("visit_I32x4Eq() not implemented"); }

    void visit_I32x4Ne() { throw LFortran::LFortranException("visit_I32x4Ne() not implemented"); }

    void visit_I32x4LtS() { throw LFortran::LFortranException("visit_I32x4LtS() not implemented"); }

    void visit_I32x4LtU() { throw LFortran::LFortranException("visit_I32x4LtU() not implemented"); }

    void visit_I32x4GtS() { throw LFortran::LFortranException("visit_I32x4GtS() not implemented"); }

    void visit_I32x4GtU() { throw LFortran::LFortranException("visit_I32x4GtU() not implemented"); }

    void visit_I32x4LeS() { throw LFortran::LFortranException("visit_I32x4LeS() not implemented"); }

    void visit_I32x4LeU() { throw LFortran::LFortranException("visit_I32x4LeU() not implemented"); }

    void visit_I32x4GeS() { throw LFortran::LFortranException("visit_I32x4GeS() not implemented"); }

    void visit_I32x4GeU() { throw LFortran::LFortranException("visit_I32x4GeU() not implemented"); }

    void visit_I64x2Eq() { throw LFortran::LFortranException("visit_I64x2Eq() not implemented"); }

    void visit_I64x2Ne() { throw LFortran::LFortranException("visit_I64x2Ne() not implemented"); }

    void visit_I64x2LtS() { throw LFortran::LFortranException("visit_I64x2LtS() not implemented"); }

    void visit_I64x2GtS() { throw LFortran::LFortranException("visit_I64x2GtS() not implemented"); }

    void visit_I64x2LeS() { throw LFortran::LFortranException("visit_I64x2LeS() not implemented"); }

    void visit_I64x2GeS() { throw LFortran::LFortranException("visit_I64x2GeS() not implemented"); }

    void visit_F32x4Eq() { throw LFortran::LFortranException("visit_F32x4Eq() not implemented"); }

    void visit_F32x4Ne() { throw LFortran::LFortranException("visit_F32x4Ne() not implemented"); }

    void visit_F32x4Lt() { throw LFortran::LFortranException("visit_F32x4Lt() not implemented"); }

    void visit_F32x4Gt() { throw LFortran::LFortranException("visit_F32x4Gt() not implemented"); }

    void visit_F32x4Le() { throw LFortran::LFortranException("visit_F32x4Le() not implemented"); }

    void visit_F32x4Ge() { throw LFortran::LFortranException("visit_F32x4Ge() not implemented"); }

    void visit_F64x2Eq() { throw LFortran::LFortranException("visit_F64x2Eq() not implemented"); }

    void visit_F64x2Ne() { throw LFortran::LFortranException("visit_F64x2Ne() not implemented"); }

    void visit_F64x2Lt() { throw LFortran::LFortranException("visit_F64x2Lt() not implemented"); }

    void visit_F64x2Gt() { throw LFortran::LFortranException("visit_F64x2Gt() not implemented"); }

    void visit_F64x2Le() { throw LFortran::LFortranException("visit_F64x2Le() not implemented"); }

    void visit_F64x2Ge() { throw LFortran::LFortranException("visit_F64x2Ge() not implemented"); }

    void visit_V128Not() { throw LFortran::LFortranException("visit_V128Not() not implemented"); }

    void visit_V128And() { throw LFortran::LFortranException("visit_V128And() not implemented"); }

    void visit_V128Andnot() { throw LFortran::LFortranException("visit_V128Andnot() not implemented"); }

    void visit_V128Or() { throw LFortran::LFortranException("visit_V128Or() not implemented"); }

    void visit_V128Xor() { throw LFortran::LFortranException("visit_V128Xor() not implemented"); }

    void visit_V128Bitselect() { throw LFortran::LFortranException("visit_V128Bitselect() not implemented"); }

    void visit_V128AnyTrue() { throw LFortran::LFortranException("visit_V128AnyTrue() not implemented"); }

    void visit_I8x16Abs() { throw LFortran::LFortranException("visit_I8x16Abs() not implemented"); }

    void visit_I8x16Neg() { throw LFortran::LFortranException("visit_I8x16Neg() not implemented"); }

    void visit_I8x16Popcnt() { throw LFortran::LFortranException("visit_I8x16Popcnt() not implemented"); }

    void visit_I8x16AllTrue() { throw LFortran::LFortranException("visit_I8x16AllTrue() not implemented"); }

    void visit_I8x16Bitmask() { throw LFortran::LFortranException("visit_I8x16Bitmask() not implemented"); }

    void visit_I8x16NarrowI16x8S() { throw LFortran::LFortranException("visit_I8x16NarrowI16x8S() not implemented"); }

    void visit_I8x16NarrowI16x8U() { throw LFortran::LFortranException("visit_I8x16NarrowI16x8U() not implemented"); }

    void visit_I8x16Shl() { throw LFortran::LFortranException("visit_I8x16Shl() not implemented"); }

    void visit_I8x16ShrS() { throw LFortran::LFortranException("visit_I8x16ShrS() not implemented"); }

    void visit_I8x16ShrU() { throw LFortran::LFortranException("visit_I8x16ShrU() not implemented"); }

    void visit_I8x16Add() { throw LFortran::LFortranException("visit_I8x16Add() not implemented"); }

    void visit_I8x16AddSatS() { throw LFortran::LFortranException("visit_I8x16AddSatS() not implemented"); }

    void visit_I8x16AddSatU() { throw LFortran::LFortranException("visit_I8x16AddSatU() not implemented"); }

    void visit_I8x16Sub() { throw LFortran::LFortranException("visit_I8x16Sub() not implemented"); }

    void visit_I8x16SubSatS() { throw LFortran::LFortranException("visit_I8x16SubSatS() not implemented"); }

    void visit_I8x16SubSatU() { throw LFortran::LFortranException("visit_I8x16SubSatU() not implemented"); }

    void visit_I8x16MinS() { throw LFortran::LFortranException("visit_I8x16MinS() not implemented"); }

    void visit_I8x16MinU() { throw LFortran::LFortranException("visit_I8x16MinU() not implemented"); }

    void visit_I8x16MaxS() { throw LFortran::LFortranException("visit_I8x16MaxS() not implemented"); }

    void visit_I8x16MaxU() { throw LFortran::LFortranException("visit_I8x16MaxU() not implemented"); }

    void visit_I8x16AvgrU() { throw LFortran::LFortranException("visit_I8x16AvgrU() not implemented"); }

    void visit_I16x8ExtaddPairwiseI8x16S() { throw LFortran::LFortranException("visit_I16x8ExtaddPairwiseI8x16S() not implemented"); }

    void visit_I16x8ExtaddPairwiseI8x16U() { throw LFortran::LFortranException("visit_I16x8ExtaddPairwiseI8x16U() not implemented"); }

    void visit_I16x8Abs() { throw LFortran::LFortranException("visit_I16x8Abs() not implemented"); }

    void visit_I16x8Neg() { throw LFortran::LFortranException("visit_I16x8Neg() not implemented"); }

    void visit_I16x8Q15mulrSatS() { throw LFortran::LFortranException("visit_I16x8Q15mulrSatS() not implemented"); }

    void visit_I16x8AllTrue() { throw LFortran::LFortranException("visit_I16x8AllTrue() not implemented"); }

    void visit_I16x8Bitmask() { throw LFortran::LFortranException("visit_I16x8Bitmask() not implemented"); }

    void visit_I16x8NarrowI32x4S() { throw LFortran::LFortranException("visit_I16x8NarrowI32x4S() not implemented"); }

    void visit_I16x8NarrowI32x4U() { throw LFortran::LFortranException("visit_I16x8NarrowI32x4U() not implemented"); }

    void visit_I16x8ExtendLowI8x16S() { throw LFortran::LFortranException("visit_I16x8ExtendLowI8x16S() not implemented"); }

    void visit_I16x8ExtendHighI8x16S() { throw LFortran::LFortranException("visit_I16x8ExtendHighI8x16S() not implemented"); }

    void visit_I16x8ExtendLowI8x16U() { throw LFortran::LFortranException("visit_I16x8ExtendLowI8x16U() not implemented"); }

    void visit_I16x8ExtendHighI8x16U() { throw LFortran::LFortranException("visit_I16x8ExtendHighI8x16U() not implemented"); }

    void visit_I16x8Shl() { throw LFortran::LFortranException("visit_I16x8Shl() not implemented"); }

    void visit_I16x8ShrS() { throw LFortran::LFortranException("visit_I16x8ShrS() not implemented"); }

    void visit_I16x8ShrU() { throw LFortran::LFortranException("visit_I16x8ShrU() not implemented"); }

    void visit_I16x8Add() { throw LFortran::LFortranException("visit_I16x8Add() not implemented"); }

    void visit_I16x8AddSatS() { throw LFortran::LFortranException("visit_I16x8AddSatS() not implemented"); }

    void visit_I16x8AddSatU() { throw LFortran::LFortranException("visit_I16x8AddSatU() not implemented"); }

    void visit_I16x8Sub() { throw LFortran::LFortranException("visit_I16x8Sub() not implemented"); }

    void visit_I16x8SubSatS() { throw LFortran::LFortranException("visit_I16x8SubSatS() not implemented"); }

    void visit_I16x8SubSatU() { throw LFortran::LFortranException("visit_I16x8SubSatU() not implemented"); }

    void visit_I16x8Mul() { throw LFortran::LFortranException("visit_I16x8Mul() not implemented"); }

    void visit_I16x8MinS() { throw LFortran::LFortranException("visit_I16x8MinS() not implemented"); }

    void visit_I16x8MinU() { throw LFortran::LFortranException("visit_I16x8MinU() not implemented"); }

    void visit_I16x8MaxS() { throw LFortran::LFortranException("visit_I16x8MaxS() not implemented"); }

    void visit_I16x8MaxU() { throw LFortran::LFortranException("visit_I16x8MaxU() not implemented"); }

    void visit_I16x8AvgrU() { throw LFortran::LFortranException("visit_I16x8AvgrU() not implemented"); }

    void visit_I16x8ExtmulLowI8x16S() { throw LFortran::LFortranException("visit_I16x8ExtmulLowI8x16S() not implemented"); }

    void visit_I16x8ExtmulHighI8x16S() { throw LFortran::LFortranException("visit_I16x8ExtmulHighI8x16S() not implemented"); }

    void visit_I16x8ExtmulLowI8x16U() { throw LFortran::LFortranException("visit_I16x8ExtmulLowI8x16U() not implemented"); }

    void visit_I16x8ExtmulHighI8x16U() { throw LFortran::LFortranException("visit_I16x8ExtmulHighI8x16U() not implemented"); }

    void visit_I32x4ExtaddPairwiseI16x8S() { throw LFortran::LFortranException("visit_I32x4ExtaddPairwiseI16x8S() not implemented"); }

    void visit_I32x4ExtaddPairwiseI16x8U() { throw LFortran::LFortranException("visit_I32x4ExtaddPairwiseI16x8U() not implemented"); }

    void visit_I32x4Abs() { throw LFortran::LFortranException("visit_I32x4Abs() not implemented"); }

    void visit_I32x4Neg() { throw LFortran::LFortranException("visit_I32x4Neg() not implemented"); }

    void visit_I32x4AllTrue() { throw LFortran::LFortranException("visit_I32x4AllTrue() not implemented"); }

    void visit_I32x4Bitmask() { throw LFortran::LFortranException("visit_I32x4Bitmask() not implemented"); }

    void visit_I32x4ExtendLowI16x8S() { throw LFortran::LFortranException("visit_I32x4ExtendLowI16x8S() not implemented"); }

    void visit_I32x4ExtendHighI16x8S() { throw LFortran::LFortranException("visit_I32x4ExtendHighI16x8S() not implemented"); }

    void visit_I32x4ExtendLowI16x8U() { throw LFortran::LFortranException("visit_I32x4ExtendLowI16x8U() not implemented"); }

    void visit_I32x4ExtendHighI16x8U() { throw LFortran::LFortranException("visit_I32x4ExtendHighI16x8U() not implemented"); }

    void visit_I32x4Shl() { throw LFortran::LFortranException("visit_I32x4Shl() not implemented"); }

    void visit_I32x4ShrS() { throw LFortran::LFortranException("visit_I32x4ShrS() not implemented"); }

    void visit_I32x4ShrU() { throw LFortran::LFortranException("visit_I32x4ShrU() not implemented"); }

    void visit_I32x4Add() { throw LFortran::LFortranException("visit_I32x4Add() not implemented"); }

    void visit_I32x4Sub() { throw LFortran::LFortranException("visit_I32x4Sub() not implemented"); }

    void visit_I32x4Mul() { throw LFortran::LFortranException("visit_I32x4Mul() not implemented"); }

    void visit_I32x4MinS() { throw LFortran::LFortranException("visit_I32x4MinS() not implemented"); }

    void visit_I32x4MinU() { throw LFortran::LFortranException("visit_I32x4MinU() not implemented"); }

    void visit_I32x4MaxS() { throw LFortran::LFortranException("visit_I32x4MaxS() not implemented"); }

    void visit_I32x4MaxU() { throw LFortran::LFortranException("visit_I32x4MaxU() not implemented"); }

    void visit_I32x4DotI16x8S() { throw LFortran::LFortranException("visit_I32x4DotI16x8S() not implemented"); }

    void visit_I32x4ExtmulLowI16x8S() { throw LFortran::LFortranException("visit_I32x4ExtmulLowI16x8S() not implemented"); }

    void visit_I32x4ExtmulHighI16x8S() { throw LFortran::LFortranException("visit_I32x4ExtmulHighI16x8S() not implemented"); }

    void visit_I32x4ExtmulLowI16x8U() { throw LFortran::LFortranException("visit_I32x4ExtmulLowI16x8U() not implemented"); }

    void visit_I32x4ExtmulHighI16x8U() { throw LFortran::LFortranException("visit_I32x4ExtmulHighI16x8U() not implemented"); }

    void visit_I64x2Abs() { throw LFortran::LFortranException("visit_I64x2Abs() not implemented"); }

    void visit_I64x2Neg() { throw LFortran::LFortranException("visit_I64x2Neg() not implemented"); }

    void visit_I64x2AllTrue() { throw LFortran::LFortranException("visit_I64x2AllTrue() not implemented"); }

    void visit_I64x2Bitmask() { throw LFortran::LFortranException("visit_I64x2Bitmask() not implemented"); }

    void visit_I64x2ExtendLowI32x4S() { throw LFortran::LFortranException("visit_I64x2ExtendLowI32x4S() not implemented"); }

    void visit_I64x2ExtendHighI32x4S() { throw LFortran::LFortranException("visit_I64x2ExtendHighI32x4S() not implemented"); }

    void visit_I64x2ExtendLowI32x4U() { throw LFortran::LFortranException("visit_I64x2ExtendLowI32x4U() not implemented"); }

    void visit_I64x2ExtendHighI32x4U() { throw LFortran::LFortranException("visit_I64x2ExtendHighI32x4U() not implemented"); }

    void visit_I64x2Shl() { throw LFortran::LFortranException("visit_I64x2Shl() not implemented"); }

    void visit_I64x2ShrS() { throw LFortran::LFortranException("visit_I64x2ShrS() not implemented"); }

    void visit_I64x2ShrU() { throw LFortran::LFortranException("visit_I64x2ShrU() not implemented"); }

    void visit_I64x2Add() { throw LFortran::LFortranException("visit_I64x2Add() not implemented"); }

    void visit_I64x2Sub() { throw LFortran::LFortranException("visit_I64x2Sub() not implemented"); }

    void visit_I64x2Mul() { throw LFortran::LFortranException("visit_I64x2Mul() not implemented"); }

    void visit_I64x2ExtmulLowI32x4S() { throw LFortran::LFortranException("visit_I64x2ExtmulLowI32x4S() not implemented"); }

    void visit_I64x2ExtmulHighI32x4S() { throw LFortran::LFortranException("visit_I64x2ExtmulHighI32x4S() not implemented"); }

    void visit_I64x2ExtmulLowI32x4U() { throw LFortran::LFortranException("visit_I64x2ExtmulLowI32x4U() not implemented"); }

    void visit_I64x2ExtmulHighI32x4U() { throw LFortran::LFortranException("visit_I64x2ExtmulHighI32x4U() not implemented"); }

    void visit_F32x4Ceil() { throw LFortran::LFortranException("visit_F32x4Ceil() not implemented"); }

    void visit_F32x4Floor() { throw LFortran::LFortranException("visit_F32x4Floor() not implemented"); }

    void visit_F32x4Trunc() { throw LFortran::LFortranException("visit_F32x4Trunc() not implemented"); }

    void visit_F32x4Nearest() { throw LFortran::LFortranException("visit_F32x4Nearest() not implemented"); }

    void visit_F32x4Abs() { throw LFortran::LFortranException("visit_F32x4Abs() not implemented"); }

    void visit_F32x4Neg() { throw LFortran::LFortranException("visit_F32x4Neg() not implemented"); }

    void visit_F32x4Sqrt() { throw LFortran::LFortranException("visit_F32x4Sqrt() not implemented"); }

    void visit_F32x4Add() { throw LFortran::LFortranException("visit_F32x4Add() not implemented"); }

    void visit_F32x4Sub() { throw LFortran::LFortranException("visit_F32x4Sub() not implemented"); }

    void visit_F32x4Mul() { throw LFortran::LFortranException("visit_F32x4Mul() not implemented"); }

    void visit_F32x4Div() { throw LFortran::LFortranException("visit_F32x4Div() not implemented"); }

    void visit_F32x4Min() { throw LFortran::LFortranException("visit_F32x4Min() not implemented"); }

    void visit_F32x4Max() { throw LFortran::LFortranException("visit_F32x4Max() not implemented"); }

    void visit_F32x4Pmin() { throw LFortran::LFortranException("visit_F32x4Pmin() not implemented"); }

    void visit_F32x4Pmax() { throw LFortran::LFortranException("visit_F32x4Pmax() not implemented"); }

    void visit_F64x2Ceil() { throw LFortran::LFortranException("visit_F64x2Ceil() not implemented"); }

    void visit_F64x2Floor() { throw LFortran::LFortranException("visit_F64x2Floor() not implemented"); }

    void visit_F64x2Trunc() { throw LFortran::LFortranException("visit_F64x2Trunc() not implemented"); }

    void visit_F64x2Nearest() { throw LFortran::LFortranException("visit_F64x2Nearest() not implemented"); }

    void visit_F64x2Abs() { throw LFortran::LFortranException("visit_F64x2Abs() not implemented"); }

    void visit_F64x2Neg() { throw LFortran::LFortranException("visit_F64x2Neg() not implemented"); }

    void visit_F64x2Sqrt() { throw LFortran::LFortranException("visit_F64x2Sqrt() not implemented"); }

    void visit_F64x2Add() { throw LFortran::LFortranException("visit_F64x2Add() not implemented"); }

    void visit_F64x2Sub() { throw LFortran::LFortranException("visit_F64x2Sub() not implemented"); }

    void visit_F64x2Mul() { throw LFortran::LFortranException("visit_F64x2Mul() not implemented"); }

    void visit_F64x2Div() { throw LFortran::LFortranException("visit_F64x2Div() not implemented"); }

    void visit_F64x2Min() { throw LFortran::LFortranException("visit_F64x2Min() not implemented"); }

    void visit_F64x2Max() { throw LFortran::LFortranException("visit_F64x2Max() not implemented"); }

    void visit_F64x2Pmin() { throw LFortran::LFortranException("visit_F64x2Pmin() not implemented"); }

    void visit_F64x2Pmax() { throw LFortran::LFortranException("visit_F64x2Pmax() not implemented"); }

    void visit_I32x4TruncSatF32x4S() { throw LFortran::LFortranException("visit_I32x4TruncSatF32x4S() not implemented"); }

    void visit_I32x4TruncSatF32x4U() { throw LFortran::LFortranException("visit_I32x4TruncSatF32x4U() not implemented"); }

    void visit_F32x4ConvertI32x4S() { throw LFortran::LFortranException("visit_F32x4ConvertI32x4S() not implemented"); }

    void visit_F32x4ConvertI32x4U() { throw LFortran::LFortranException("visit_F32x4ConvertI32x4U() not implemented"); }

    void visit_I32x4TruncSatF64x2SZero() { throw LFortran::LFortranException("visit_I32x4TruncSatF64x2SZero() not implemented"); }

    void visit_I32x4TruncSatF64x2UZero() { throw LFortran::LFortranException("visit_I32x4TruncSatF64x2UZero() not implemented"); }

    void visit_F64x2ConvertLowI32x4S() { throw LFortran::LFortranException("visit_F64x2ConvertLowI32x4S() not implemented"); }

    void visit_F64x2ConvertLowI32x4U() { throw LFortran::LFortranException("visit_F64x2ConvertLowI32x4U() not implemented"); }

    void visit_F32x4DemoteF64x2Zero() { throw LFortran::LFortranException("visit_F32x4DemoteF64x2Zero() not implemented"); }

    void visit_F64x2PromoteLowF32x4() { throw LFortran::LFortranException("visit_F64x2PromoteLowF32x4() not implemented"); }

    void decode_instructions(uint32_t offset, std::vector<uint8_t>& wasm_bytes) {
        uint8_t cur_byte = wasm_bytes[offset++];
        while (cur_byte != 0x0B) {
            switch (cur_byte) {
                case 0x00: {
                    self().visit_Unreachable();
                    break;
                }
                case 0x01: {
                    self().visit_Nop();
                    break;
                }
                case 0x02: {
                    self().visit_Block();
                    break;
                }
                case 0x03: {
                    self().visit_Loop();
                    break;
                }
                case 0x04: {
                    self().visit_If();
                    break;
                }
                case 0x05: {
                    self().visit_Else();
                    break;
                }
                case 0x0C: {
                    uint32_t labelidx = read_unsigned_num(offset);
                    self().visit_Br(labelidx);
                    break;
                }
                case 0x0D: {
                    uint32_t labelidx = read_unsigned_num(offset);
                    self().visit_BrIf(labelidx);
                    break;
                }
                case 0x0E: {
                    uint32_t vec_size = read_unsigned_num(offset);
                    std::vector<uint32_t> label_indices(vec_size);
                    for (uint32_t i = 0U; i < vec_size; i++) {
                        label_indices[i] = read_unsigned_num(i);
                    }
                    uint32_t labelidx = read_unsigned_num(offset);
                    self().visit_BrTable(label_indices, labelidx);
                    break;
                }
                case 0x0F: {
                    self().visit_Return();
                    break;
                }
                case 0x10: {
                    uint32_t funcidx = read_unsigned_num(offset);
                    self().visit_Call(funcidx);
                    break;
                }
                case 0x11: {
                    uint32_t typeidx = read_unsigned_num(offset);
                    uint32_t tableidx = read_unsigned_num(offset);
                    self().visit_CallIndirect(typeidx, tableidx);
                    break;
                }
                case 0xD0: {
                    uint8_t ref_type = wasm_bytes[offset++];
                    self().visit_RefNull(ref_type);
                    break;
                }
                case 0xD1: {
                    self().visit_RefIsNull();
                    break;
                }
                case 0xD2: {
                    uint32_t funcidx = read_unsigned_num(offset);
                    self().visit_RefFunc(funcidx);
                    break;
                }
                case 0x1A: {
                    self().visit_Drop();
                    break;
                }
                case 0x1B: {
                    self().visit_Select();
                    break;
                }
                case 0x1C: {
                    uint32_t vec_size = read_unsigned_num(offset);
                    std::vector<uint8_t> val_types(vec_size);
                    for (uint32_t i = 0U; i < vec_size; i++) {
                        val_types[i] = wasm_bytes[offset++];
                    }
                    self().visit_Select(val_types);
                    break;
                }
                case 0x20: {
                    uint32_t localidx = read_unsigned_num(offset);
                    self().visit_LocalGet(localidx);
                    break;
                }
                case 0x21: {
                    uint32_t localidx = read_unsigned_num(offset);
                    self().visit_LocalSet(localidx);
                    break;
                }
                case 0x22: {
                    uint32_t localidx = read_unsigned_num(offset);
                    self().visit_LocalTee(localidx);
                    break;
                }
                case 0x23: {
                    uint32_t globalidx = read_unsigned_num(offset);
                    self().visit_GlobalGet(globalidx);
                    break;
                }
                case 0x24: {
                    uint32_t globalidx = read_unsigned_num(offset);
                    self().visit_GlobalSet(globalidx);
                    break;
                }
                case 0x25: {
                    uint32_t tableidx = read_unsigned_num(offset);
                    self().visit_TableGet(tableidx);
                    break;
                }
                case 0x26: {
                    uint32_t tableidx = read_unsigned_num(offset);
                    self().visit_TableSet(tableidx);
                    break;
                }
                case 0x28: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Load(mem_align, mem_offset);
                    break;
                }
                case 0x29: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load(mem_align, mem_offset);
                    break;
                }
                case 0x2A: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_F32Load(mem_align, mem_offset);
                    break;
                }
                case 0x2B: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_F64Load(mem_align, mem_offset);
                    break;
                }
                case 0x2C: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Load8S(mem_align, mem_offset);
                    break;
                }
                case 0x2D: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Load8U(mem_align, mem_offset);
                    break;
                }
                case 0x2E: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Load16S(mem_align, mem_offset);
                    break;
                }
                case 0x2F: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Load16U(mem_align, mem_offset);
                    break;
                }
                case 0x30: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load8S(mem_align, mem_offset);
                    break;
                }
                case 0x31: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load8U(mem_align, mem_offset);
                    break;
                }
                case 0x32: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load16S(mem_align, mem_offset);
                    break;
                }
                case 0x33: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load16U(mem_align, mem_offset);
                    break;
                }
                case 0x34: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load32S(mem_align, mem_offset);
                    break;
                }
                case 0x35: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Load32U(mem_align, mem_offset);
                    break;
                }
                case 0x36: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Store(mem_align, mem_offset);
                    break;
                }
                case 0x37: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Store(mem_align, mem_offset);
                    break;
                }
                case 0x38: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_F32Store(mem_align, mem_offset);
                    break;
                }
                case 0x39: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_F64Store(mem_align, mem_offset);
                    break;
                }
                case 0x3A: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Store8(mem_align, mem_offset);
                    break;
                }
                case 0x3B: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I32Store16(mem_align, mem_offset);
                    break;
                }
                case 0x3C: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Store8(mem_align, mem_offset);
                    break;
                }
                case 0x3D: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Store16(mem_align, mem_offset);
                    break;
                }
                case 0x3E: {
                    uint32_t mem_align = read_unsigned_num(offset);
                    uint32_t mem_offset = read_unsigned_num(offset);
                    self().visit_I64Store32(mem_align, mem_offset);
                    break;
                }
                case 0x3F: {
                    self().visit_MemorySize();
                    break;
                }
                case 0x40: {
                    self().visit_MemoryGrow();
                    break;
                }
                case 0x41: {
                    int n = read_signed_num(offset);
                    self().visit_I32Const(n);
                    break;
                }
                case 0x42: {
                    // Note: we currently do not have leb128 conversion functions for 64bit integers
                    long long int n = read_signed_num(offset);
                    self().visit_I64Const(n);
                    break;
                }
                case 0x43: {
                    // Note: we currently do not have leb128 conversion functions for 32bit floats
                    float z = read_signed_num(offset);
                    self().visit_F32Const(z);
                    break;
                }
                case 0x44: {
                    // Note: we currently do not have leb128 conversion functions for 64bit floats
                    double z = read_signed_num(offset);
                    self().visit_F64Const(z);
                    break;
                }
                case 0x45: {
                    self().visit_I32Eqz();
                    break;
                }
                case 0x46: {
                    self().visit_I32Eq();
                    break;
                }
                case 0x47: {
                    self().visit_I32Ne();
                    break;
                }
                case 0x48: {
                    self().visit_I32LtS();
                    break;
                }
                case 0x49: {
                    self().visit_I32LtU();
                    break;
                }
                case 0x4A: {
                    self().visit_I32GtS();
                    break;
                }
                case 0x4B: {
                    self().visit_I32GtU();
                    break;
                }
                case 0x4C: {
                    self().visit_I32LeS();
                    break;
                }
                case 0x4D: {
                    self().visit_I32LeU();
                    break;
                }
                case 0x4E: {
                    self().visit_I32GeS();
                    break;
                }
                case 0x4F: {
                    self().visit_I32GeU();
                    break;
                }
                case 0x50: {
                    self().visit_I64Eqz();
                    break;
                }
                case 0x51: {
                    self().visit_I64Eq();
                    break;
                }
                case 0x52: {
                    self().visit_I64Ne();
                    break;
                }
                case 0x53: {
                    self().visit_I64LtS();
                    break;
                }
                case 0x54: {
                    self().visit_I64LtU();
                    break;
                }
                case 0x55: {
                    self().visit_I64GtS();
                    break;
                }
                case 0x56: {
                    self().visit_I64GtU();
                    break;
                }
                case 0x57: {
                    self().visit_I64LeS();
                    break;
                }
                case 0x58: {
                    self().visit_I64LeU();
                    break;
                }
                case 0x59: {
                    self().visit_I64GeS();
                    break;
                }
                case 0x5A: {
                    self().visit_I64GeU();
                    break;
                }
                case 0x5B: {
                    self().visit_F32Eq();
                    break;
                }
                case 0x5C: {
                    self().visit_F32Ne();
                    break;
                }
                case 0x5D: {
                    self().visit_F32Lt();
                    break;
                }
                case 0x5E: {
                    self().visit_F32Gt();
                    break;
                }
                case 0x5F: {
                    self().visit_F32Le();
                    break;
                }
                case 0x60: {
                    self().visit_F32Ge();
                    break;
                }
                case 0x61: {
                    self().visit_F64Eq();
                    break;
                }
                case 0x62: {
                    self().visit_F64Ne();
                    break;
                }
                case 0x63: {
                    self().visit_F64Lt();
                    break;
                }
                case 0x64: {
                    self().visit_F64Gt();
                    break;
                }
                case 0x65: {
                    self().visit_F64Le();
                    break;
                }
                case 0x66: {
                    self().visit_F64Ge();
                    break;
                }
                case 0x67: {
                    self().visit_I32Clz();
                    break;
                }
                case 0x68: {
                    self().visit_I32Ctz();
                    break;
                }
                case 0x69: {
                    self().visit_I32Popcnt();
                    break;
                }
                case 0x6A: {
                    self().visit_I32Add();
                    break;
                }
                case 0x6B: {
                    self().visit_I32Sub();
                    break;
                }
                case 0x6C: {
                    self().visit_I32Mul();
                    break;
                }
                case 0x6D: {
                    self().visit_I32DivS();
                    break;
                }
                case 0x6E: {
                    self().visit_I32DivU();
                    break;
                }
                case 0x6F: {
                    self().visit_I32RemS();
                    break;
                }
                case 0x70: {
                    self().visit_I32RemU();
                    break;
                }
                case 0x71: {
                    self().visit_I32And();
                    break;
                }
                case 0x72: {
                    self().visit_I32Or();
                    break;
                }
                case 0x73: {
                    self().visit_I32Xor();
                    break;
                }
                case 0x74: {
                    self().visit_I32Shl();
                    break;
                }
                case 0x75: {
                    self().visit_I32ShrS();
                    break;
                }
                case 0x76: {
                    self().visit_I32ShrU();
                    break;
                }
                case 0x77: {
                    self().visit_I32Rotl();
                    break;
                }
                case 0x78: {
                    self().visit_I32Rotr();
                    break;
                }
                case 0x79: {
                    self().visit_I64Clz();
                    break;
                }
                case 0x7A: {
                    self().visit_I64Ctz();
                    break;
                }
                case 0x7B: {
                    self().visit_I64Popcnt();
                    break;
                }
                case 0x7C: {
                    self().visit_I64Add();
                    break;
                }
                case 0x7D: {
                    self().visit_I64Sub();
                    break;
                }
                case 0x7E: {
                    self().visit_I64Mul();
                    break;
                }
                case 0x7F: {
                    self().visit_I64DivS();
                    break;
                }
                case 0x80: {
                    self().visit_I64DivU();
                    break;
                }
                case 0x81: {
                    self().visit_I64RemS();
                    break;
                }
                case 0x82: {
                    self().visit_I64RemU();
                    break;
                }
                case 0x83: {
                    self().visit_I64And();
                    break;
                }
                case 0x84: {
                    self().visit_I64Or();
                    break;
                }
                case 0x85: {
                    self().visit_I64Xor();
                    break;
                }
                case 0x86: {
                    self().visit_I64Shl();
                    break;
                }
                case 0x87: {
                    self().visit_I64ShrS();
                    break;
                }
                case 0x88: {
                    self().visit_I64ShrU();
                    break;
                }
                case 0x89: {
                    self().visit_I64Rotl();
                    break;
                }
                case 0x8A: {
                    self().visit_I64Rotr();
                    break;
                }
                case 0x8B: {
                    self().visit_F32Abs();
                    break;
                }
                case 0x8C: {
                    self().visit_F32Neg();
                    break;
                }
                case 0x8D: {
                    self().visit_F32Ceil();
                    break;
                }
                case 0x8E: {
                    self().visit_F32Floor();
                    break;
                }
                case 0x8F: {
                    self().visit_F32Trunc();
                    break;
                }
                case 0x90: {
                    self().visit_F32Nearest();
                    break;
                }
                case 0x91: {
                    self().visit_F32Sqrt();
                    break;
                }
                case 0x92: {
                    self().visit_F32Add();
                    break;
                }
                case 0x93: {
                    self().visit_F32Sub();
                    break;
                }
                case 0x94: {
                    self().visit_F32Mul();
                    break;
                }
                case 0x95: {
                    self().visit_F32Div();
                    break;
                }
                case 0x96: {
                    self().visit_F32Min();
                    break;
                }
                case 0x97: {
                    self().visit_F32Max();
                    break;
                }
                case 0x98: {
                    self().visit_F32Copysign();
                    break;
                }
                case 0x99: {
                    self().visit_F64Abs();
                    break;
                }
                case 0x9A: {
                    self().visit_F64Neg();
                    break;
                }
                case 0x9B: {
                    self().visit_F64Ceil();
                    break;
                }
                case 0x9C: {
                    self().visit_F64Floor();
                    break;
                }
                case 0x9D: {
                    self().visit_F64Trunc();
                    break;
                }
                case 0x9E: {
                    self().visit_F64Nearest();
                    break;
                }
                case 0x9F: {
                    self().visit_F64Sqrt();
                    break;
                }
                case 0xA0: {
                    self().visit_F64Add();
                    break;
                }
                case 0xA1: {
                    self().visit_F64Sub();
                    break;
                }
                case 0xA2: {
                    self().visit_F64Mul();
                    break;
                }
                case 0xA3: {
                    self().visit_F64Div();
                    break;
                }
                case 0xA4: {
                    self().visit_F64Min();
                    break;
                }
                case 0xA5: {
                    self().visit_F64Max();
                    break;
                }
                case 0xA6: {
                    self().visit_F64Copysign();
                    break;
                }
                case 0xA7: {
                    self().visit_I32WrapI64();
                    break;
                }
                case 0xA8: {
                    self().visit_I32TruncF32S();
                    break;
                }
                case 0xA9: {
                    self().visit_I32TruncF32U();
                    break;
                }
                case 0xAA: {
                    self().visit_I32TruncF64S();
                    break;
                }
                case 0xAB: {
                    self().visit_I32TruncF64U();
                    break;
                }
                case 0xAC: {
                    self().visit_I64ExtendI32S();
                    break;
                }
                case 0xAD: {
                    self().visit_I64ExtendI32U();
                    break;
                }
                case 0xAE: {
                    self().visit_I64TruncF32S();
                    break;
                }
                case 0xAF: {
                    self().visit_I64TruncF32U();
                    break;
                }
                case 0xB0: {
                    self().visit_I64TruncF64S();
                    break;
                }
                case 0xB1: {
                    self().visit_I64TruncF64U();
                    break;
                }
                case 0xB2: {
                    self().visit_F32ConvertI32S();
                    break;
                }
                case 0xB3: {
                    self().visit_F32ConvertI32U();
                    break;
                }
                case 0xB4: {
                    self().visit_F32ConvertI64S();
                    break;
                }
                case 0xB5: {
                    self().visit_F32ConvertI64U();
                    break;
                }
                case 0xB6: {
                    self().visit_F32DemoteF64();
                    break;
                }
                case 0xB7: {
                    self().visit_F64ConvertI32S();
                    break;
                }
                case 0xB8: {
                    self().visit_F64ConvertI32U();
                    break;
                }
                case 0xB9: {
                    self().visit_F64ConvertI64S();
                    break;
                }
                case 0xBA: {
                    self().visit_F64ConvertI64U();
                    break;
                }
                case 0xBB: {
                    self().visit_F64PromoteF32();
                    break;
                }
                case 0xBC: {
                    self().visit_I32ReinterpretF32();
                    break;
                }
                case 0xBD: {
                    self().visit_I64ReinterpretF64();
                    break;
                }
                case 0xBE: {
                    self().visit_F32ReinterpretI32();
                    break;
                }
                case 0xBF: {
                    self().visit_F64ReinterpretI64();
                    break;
                }
                case 0xC0: {
                    self().visit_I32Extend8S();
                    break;
                }
                case 0xC1: {
                    self().visit_I32Extend16S();
                    break;
                }
                case 0xC2: {
                    self().visit_I64Extend8S();
                    break;
                }
                case 0xC3: {
                    self().visit_I64Extend16S();
                    break;
                }
                case 0xC4: {
                    self().visit_I64Extend32S();
                    break;
                }
                case 0xFC: {
                    uint32_t num = read_unsigned_num(offset);
                    switch (num) {
                        case 12U: {
                            uint32_t elemidx = read_unsigned_num(offset);
                            uint32_t tableidx = read_unsigned_num(offset);
                            self().visit_TableInit(elemidx, tableidx);
                            break;
                        }
                        case 13U: {
                            uint32_t elemidx = read_unsigned_num(offset);
                            self().visit_ElemDrop(elemidx);
                            break;
                        }
                        case 14U: {
                            uint32_t tableidx = read_unsigned_num(offset);
                            uint32_t tableidx = read_unsigned_num(offset);
                            self().visit_TableCopy(tableidx, tableidx);
                            break;
                        }
                        case 15U: {
                            uint32_t tableidx = read_unsigned_num(offset);
                            self().visit_TableGrow(tableidx);
                            break;
                        }
                        case 16U: {
                            uint32_t tableidx = read_unsigned_num(offset);
                            self().visit_TableSize(tableidx);
                            break;
                        }
                        case 17U: {
                            uint32_t tableidx = read_unsigned_num(offset);
                            self().visit_TableFill(tableidx);
                            break;
                        }
                        case 8U: {
                            uint32_t dataidx = read_unsigned_num(offset);
                            self().visit_MemoryInit(dataidx);
                            break;
                        }
                        case 9U: {
                            uint32_t dataidx = read_unsigned_num(offset);
                            self().visit_DataDrop(dataidx);
                            break;
                        }
                        case 10U: {
                            self().visit_MemoryCopy();
                            break;
                        }
                        case 11U: {
                            self().visit_MemoryFill();
                            break;
                        }
                        case 0U: {
                            self().visit_I32TruncSatF32S();
                            break;
                        }
                        case 1U: {
                            self().visit_I32TruncSatF32U();
                            break;
                        }
                        case 2U: {
                            self().visit_I32TruncSatF64S();
                            break;
                        }
                        case 3U: {
                            self().visit_I32TruncSatF64U();
                            break;
                        }
                        case 4U: {
                            self().visit_I64TruncSatF32S();
                            break;
                        }
                        case 5U: {
                            self().visit_I64TruncSatF32U();
                            break;
                        }
                        case 6U: {
                            self().visit_I64TruncSatF64S();
                            break;
                        }
                        case 7U: {
                            self().visit_I64TruncSatF64U();
                            break;
                        }
                        default: {
                            throw LFortran::LFortranException("Incorrect num found for intruction with byte 0xFC");
                        }
                    }
                    break;
                }
                case 0xFD: {
                    uint32_t num = read_unsigned_num(offset);
                    switch (num) {
                        case 0U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load(mem_align, mem_offset);
                            break;
                        }
                        case 1U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load8x8S(mem_align, mem_offset);
                            break;
                        }
                        case 2U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load8x8U(mem_align, mem_offset);
                            break;
                        }
                        case 3U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load16x4S(mem_align, mem_offset);
                            break;
                        }
                        case 4U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load16x4U(mem_align, mem_offset);
                            break;
                        }
                        case 5U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load32x2S(mem_align, mem_offset);
                            break;
                        }
                        case 6U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load32x2U(mem_align, mem_offset);
                            break;
                        }
                        case 7U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load8Splat(mem_align, mem_offset);
                            break;
                        }
                        case 8U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load16Splat(mem_align, mem_offset);
                            break;
                        }
                        case 9U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load32Splat(mem_align, mem_offset);
                            break;
                        }
                        case 10U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load64Splat(mem_align, mem_offset);
                            break;
                        }
                        case 92U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load32Zero(mem_align, mem_offset);
                            break;
                        }
                        case 93U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Load64Zero(mem_align, mem_offset);
                            break;
                        }
                        case 11U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            self().visit_V128Store(mem_align, mem_offset);
                            break;
                        }
                        case 84U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Load8Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 85U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Load16Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 86U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Load32Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 87U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Load64Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 88U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Store8Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 89U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Store16Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 90U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Store32Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 91U: {
                            uint32_t mem_align = read_unsigned_num(offset);
                            uint32_t mem_offset = read_unsigned_num(offset);
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_V128Store64Lane(mem_align, mem_offset, laneidx);
                            break;
                        }
                        case 12U: {
                            std::vector<uint8_t> immediate_bytes16;
                            for (int i = 0; i < 16; i++) {
                                immediate_bytes16.push_back(wasm_bytes[offset++]);
                            }
                            self().visit_V128Const(immediate_bytes16);
                            break;
                        }
                        case 13U: {
                            std::vector<uint8_t> immediate_lane_indices16;
                            for (int i = 0; i < 16; i++) {
                                immediate_lane_indices16.push_back(wasm_bytes[offset++]);
                            }
                            self().visit_I8x16Shuffle(immediate_lane_indices16);
                            break;
                        }
                        case 21U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I8x16ExtractLaneS(laneidx);
                            break;
                        }
                        case 22U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I8x16ExtractLaneU(laneidx);
                            break;
                        }
                        case 23U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I8x16ReplaceLane(laneidx);
                            break;
                        }
                        case 24U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I16x8ExtractLaneS(laneidx);
                            break;
                        }
                        case 25U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I16x8ExtractLaneU(laneidx);
                            break;
                        }
                        case 26U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I16x8ReplaceLane(laneidx);
                            break;
                        }
                        case 27U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I32x4ExtractLane(laneidx);
                            break;
                        }
                        case 28U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I32x4ReplaceLane(laneidx);
                            break;
                        }
                        case 29U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I64x2ExtractLane(laneidx);
                            break;
                        }
                        case 30U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_I64x2ReplaceLane(laneidx);
                            break;
                        }
                        case 31U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_F32x4ExtractLane(laneidx);
                            break;
                        }
                        case 32U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_F32x4ReplaceLane(laneidx);
                            break;
                        }
                        case 33U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_F64x2ExtractLane(laneidx);
                            break;
                        }
                        case 34U: {
                            uint8_t laneidx = wasm_bytes[offset++];
                            self().visit_F64x2ReplaceLane(laneidx);
                            break;
                        }
                        case 14U: {
                            self().visit_I8x16Swizzle();
                            break;
                        }
                        case 15U: {
                            self().visit_I8x16Splat();
                            break;
                        }
                        case 16U: {
                            self().visit_I16x8Splat();
                            break;
                        }
                        case 17U: {
                            self().visit_I32x4Splat();
                            break;
                        }
                        case 18U: {
                            self().visit_I64x2Splat();
                            break;
                        }
                        case 19U: {
                            self().visit_F32x4Splat();
                            break;
                        }
                        case 20U: {
                            self().visit_F64x2Splat();
                            break;
                        }
                        case 35U: {
                            self().visit_I8x16Eq();
                            break;
                        }
                        case 36U: {
                            self().visit_I8x16Ne();
                            break;
                        }
                        case 37U: {
                            self().visit_I8x16LtS();
                            break;
                        }
                        case 38U: {
                            self().visit_I8x16LtU();
                            break;
                        }
                        case 39U: {
                            self().visit_I8x16GtS();
                            break;
                        }
                        case 40U: {
                            self().visit_I8x16GtU();
                            break;
                        }
                        case 41U: {
                            self().visit_I8x16LeS();
                            break;
                        }
                        case 42U: {
                            self().visit_I8x16LeU();
                            break;
                        }
                        case 43U: {
                            self().visit_I8x16GeS();
                            break;
                        }
                        case 44U: {
                            self().visit_I8x16GeU();
                            break;
                        }
                        case 45U: {
                            self().visit_I16x8Eq();
                            break;
                        }
                        case 46U: {
                            self().visit_I16x8Ne();
                            break;
                        }
                        case 47U: {
                            self().visit_I16x8LtS();
                            break;
                        }
                        case 48U: {
                            self().visit_I16x8LtU();
                            break;
                        }
                        case 49U: {
                            self().visit_I16x8GtS();
                            break;
                        }
                        case 50U: {
                            self().visit_I16x8GtU();
                            break;
                        }
                        case 51U: {
                            self().visit_I16x8LeS();
                            break;
                        }
                        case 52U: {
                            self().visit_I16x8LeU();
                            break;
                        }
                        case 53U: {
                            self().visit_I16x8GeS();
                            break;
                        }
                        case 54U: {
                            self().visit_I16x8GeU();
                            break;
                        }
                        case 55U: {
                            self().visit_I32x4Eq();
                            break;
                        }
                        case 56U: {
                            self().visit_I32x4Ne();
                            break;
                        }
                        case 57U: {
                            self().visit_I32x4LtS();
                            break;
                        }
                        case 58U: {
                            self().visit_I32x4LtU();
                            break;
                        }
                        case 59U: {
                            self().visit_I32x4GtS();
                            break;
                        }
                        case 60U: {
                            self().visit_I32x4GtU();
                            break;
                        }
                        case 61U: {
                            self().visit_I32x4LeS();
                            break;
                        }
                        case 62U: {
                            self().visit_I32x4LeU();
                            break;
                        }
                        case 63U: {
                            self().visit_I32x4GeS();
                            break;
                        }
                        case 64U: {
                            self().visit_I32x4GeU();
                            break;
                        }
                        case 214U: {
                            self().visit_I64x2Eq();
                            break;
                        }
                        case 215U: {
                            self().visit_I64x2Ne();
                            break;
                        }
                        case 216U: {
                            self().visit_I64x2LtS();
                            break;
                        }
                        case 217U: {
                            self().visit_I64x2GtS();
                            break;
                        }
                        case 218U: {
                            self().visit_I64x2LeS();
                            break;
                        }
                        case 219U: {
                            self().visit_I64x2GeS();
                            break;
                        }
                        case 65U: {
                            self().visit_F32x4Eq();
                            break;
                        }
                        case 66U: {
                            self().visit_F32x4Ne();
                            break;
                        }
                        case 67U: {
                            self().visit_F32x4Lt();
                            break;
                        }
                        case 68U: {
                            self().visit_F32x4Gt();
                            break;
                        }
                        case 69U: {
                            self().visit_F32x4Le();
                            break;
                        }
                        case 70U: {
                            self().visit_F32x4Ge();
                            break;
                        }
                        case 71U: {
                            self().visit_F64x2Eq();
                            break;
                        }
                        case 72U: {
                            self().visit_F64x2Ne();
                            break;
                        }
                        case 73U: {
                            self().visit_F64x2Lt();
                            break;
                        }
                        case 74U: {
                            self().visit_F64x2Gt();
                            break;
                        }
                        case 75U: {
                            self().visit_F64x2Le();
                            break;
                        }
                        case 76U: {
                            self().visit_F64x2Ge();
                            break;
                        }
                        case 77U: {
                            self().visit_V128Not();
                            break;
                        }
                        case 78U: {
                            self().visit_V128And();
                            break;
                        }
                        case 79U: {
                            self().visit_V128Andnot();
                            break;
                        }
                        case 80U: {
                            self().visit_V128Or();
                            break;
                        }
                        case 81U: {
                            self().visit_V128Xor();
                            break;
                        }
                        case 82U: {
                            self().visit_V128Bitselect();
                            break;
                        }
                        case 83U: {
                            self().visit_V128AnyTrue();
                            break;
                        }
                        case 96U: {
                            self().visit_I8x16Abs();
                            break;
                        }
                        case 97U: {
                            self().visit_I8x16Neg();
                            break;
                        }
                        case 98U: {
                            self().visit_I8x16Popcnt();
                            break;
                        }
                        case 99U: {
                            self().visit_I8x16AllTrue();
                            break;
                        }
                        case 100U: {
                            self().visit_I8x16Bitmask();
                            break;
                        }
                        case 101U: {
                            self().visit_I8x16NarrowI16x8S();
                            break;
                        }
                        case 102U: {
                            self().visit_I8x16NarrowI16x8U();
                            break;
                        }
                        case 107U: {
                            self().visit_I8x16Shl();
                            break;
                        }
                        case 108U: {
                            self().visit_I8x16ShrS();
                            break;
                        }
                        case 109U: {
                            self().visit_I8x16ShrU();
                            break;
                        }
                        case 110U: {
                            self().visit_I8x16Add();
                            break;
                        }
                        case 111U: {
                            self().visit_I8x16AddSatS();
                            break;
                        }
                        case 112U: {
                            self().visit_I8x16AddSatU();
                            break;
                        }
                        case 113U: {
                            self().visit_I8x16Sub();
                            break;
                        }
                        case 114U: {
                            self().visit_I8x16SubSatS();
                            break;
                        }
                        case 115U: {
                            self().visit_I8x16SubSatU();
                            break;
                        }
                        case 118U: {
                            self().visit_I8x16MinS();
                            break;
                        }
                        case 119U: {
                            self().visit_I8x16MinU();
                            break;
                        }
                        case 120U: {
                            self().visit_I8x16MaxS();
                            break;
                        }
                        case 121U: {
                            self().visit_I8x16MaxU();
                            break;
                        }
                        case 123U: {
                            self().visit_I8x16AvgrU();
                            break;
                        }
                        case 124U: {
                            self().visit_I16x8ExtaddPairwiseI8x16S();
                            break;
                        }
                        case 125U: {
                            self().visit_I16x8ExtaddPairwiseI8x16U();
                            break;
                        }
                        case 128U: {
                            self().visit_I16x8Abs();
                            break;
                        }
                        case 129U: {
                            self().visit_I16x8Neg();
                            break;
                        }
                        case 130U: {
                            self().visit_I16x8Q15mulrSatS();
                            break;
                        }
                        case 131U: {
                            self().visit_I16x8AllTrue();
                            break;
                        }
                        case 132U: {
                            self().visit_I16x8Bitmask();
                            break;
                        }
                        case 133U: {
                            self().visit_I16x8NarrowI32x4S();
                            break;
                        }
                        case 134U: {
                            self().visit_I16x8NarrowI32x4U();
                            break;
                        }
                        case 135U: {
                            self().visit_I16x8ExtendLowI8x16S();
                            break;
                        }
                        case 136U: {
                            self().visit_I16x8ExtendHighI8x16S();
                            break;
                        }
                        case 137U: {
                            self().visit_I16x8ExtendLowI8x16U();
                            break;
                        }
                        case 138U: {
                            self().visit_I16x8ExtendHighI8x16U();
                            break;
                        }
                        case 139U: {
                            self().visit_I16x8Shl();
                            break;
                        }
                        case 140U: {
                            self().visit_I16x8ShrS();
                            break;
                        }
                        case 141U: {
                            self().visit_I16x8ShrU();
                            break;
                        }
                        case 142U: {
                            self().visit_I16x8Add();
                            break;
                        }
                        case 143U: {
                            self().visit_I16x8AddSatS();
                            break;
                        }
                        case 144U: {
                            self().visit_I16x8AddSatU();
                            break;
                        }
                        case 145U: {
                            self().visit_I16x8Sub();
                            break;
                        }
                        case 146U: {
                            self().visit_I16x8SubSatS();
                            break;
                        }
                        case 147U: {
                            self().visit_I16x8SubSatU();
                            break;
                        }
                        case 149U: {
                            self().visit_I16x8Mul();
                            break;
                        }
                        case 150U: {
                            self().visit_I16x8MinS();
                            break;
                        }
                        case 151U: {
                            self().visit_I16x8MinU();
                            break;
                        }
                        case 152U: {
                            self().visit_I16x8MaxS();
                            break;
                        }
                        case 153U: {
                            self().visit_I16x8MaxU();
                            break;
                        }
                        case 155U: {
                            self().visit_I16x8AvgrU();
                            break;
                        }
                        case 156U: {
                            self().visit_I16x8ExtmulLowI8x16S();
                            break;
                        }
                        case 157U: {
                            self().visit_I16x8ExtmulHighI8x16S();
                            break;
                        }
                        case 158U: {
                            self().visit_I16x8ExtmulLowI8x16U();
                            break;
                        }
                        case 159U: {
                            self().visit_I16x8ExtmulHighI8x16U();
                            break;
                        }
                        case 126U: {
                            self().visit_I32x4ExtaddPairwiseI16x8S();
                            break;
                        }
                        case 127U: {
                            self().visit_I32x4ExtaddPairwiseI16x8U();
                            break;
                        }
                        case 160U: {
                            self().visit_I32x4Abs();
                            break;
                        }
                        case 161U: {
                            self().visit_I32x4Neg();
                            break;
                        }
                        case 163U: {
                            self().visit_I32x4AllTrue();
                            break;
                        }
                        case 164U: {
                            self().visit_I32x4Bitmask();
                            break;
                        }
                        case 167U: {
                            self().visit_I32x4ExtendLowI16x8S();
                            break;
                        }
                        case 168U: {
                            self().visit_I32x4ExtendHighI16x8S();
                            break;
                        }
                        case 169U: {
                            self().visit_I32x4ExtendLowI16x8U();
                            break;
                        }
                        case 170U: {
                            self().visit_I32x4ExtendHighI16x8U();
                            break;
                        }
                        case 171U: {
                            self().visit_I32x4Shl();
                            break;
                        }
                        case 172U: {
                            self().visit_I32x4ShrS();
                            break;
                        }
                        case 173U: {
                            self().visit_I32x4ShrU();
                            break;
                        }
                        case 174U: {
                            self().visit_I32x4Add();
                            break;
                        }
                        case 177U: {
                            self().visit_I32x4Sub();
                            break;
                        }
                        case 181U: {
                            self().visit_I32x4Mul();
                            break;
                        }
                        case 182U: {
                            self().visit_I32x4MinS();
                            break;
                        }
                        case 183U: {
                            self().visit_I32x4MinU();
                            break;
                        }
                        case 184U: {
                            self().visit_I32x4MaxS();
                            break;
                        }
                        case 185U: {
                            self().visit_I32x4MaxU();
                            break;
                        }
                        case 186U: {
                            self().visit_I32x4DotI16x8S();
                            break;
                        }
                        case 188U: {
                            self().visit_I32x4ExtmulLowI16x8S();
                            break;
                        }
                        case 189U: {
                            self().visit_I32x4ExtmulHighI16x8S();
                            break;
                        }
                        case 190U: {
                            self().visit_I32x4ExtmulLowI16x8U();
                            break;
                        }
                        case 191U: {
                            self().visit_I32x4ExtmulHighI16x8U();
                            break;
                        }
                        case 192U: {
                            self().visit_I64x2Abs();
                            break;
                        }
                        case 193U: {
                            self().visit_I64x2Neg();
                            break;
                        }
                        case 195U: {
                            self().visit_I64x2AllTrue();
                            break;
                        }
                        case 196U: {
                            self().visit_I64x2Bitmask();
                            break;
                        }
                        case 199U: {
                            self().visit_I64x2ExtendLowI32x4S();
                            break;
                        }
                        case 200U: {
                            self().visit_I64x2ExtendHighI32x4S();
                            break;
                        }
                        case 201U: {
                            self().visit_I64x2ExtendLowI32x4U();
                            break;
                        }
                        case 202U: {
                            self().visit_I64x2ExtendHighI32x4U();
                            break;
                        }
                        case 203U: {
                            self().visit_I64x2Shl();
                            break;
                        }
                        case 204U: {
                            self().visit_I64x2ShrS();
                            break;
                        }
                        case 205U: {
                            self().visit_I64x2ShrU();
                            break;
                        }
                        case 206U: {
                            self().visit_I64x2Add();
                            break;
                        }
                        case 209U: {
                            self().visit_I64x2Sub();
                            break;
                        }
                        case 213U: {
                            self().visit_I64x2Mul();
                            break;
                        }
                        case 220U: {
                            self().visit_I64x2ExtmulLowI32x4S();
                            break;
                        }
                        case 221U: {
                            self().visit_I64x2ExtmulHighI32x4S();
                            break;
                        }
                        case 222U: {
                            self().visit_I64x2ExtmulLowI32x4U();
                            break;
                        }
                        case 223U: {
                            self().visit_I64x2ExtmulHighI32x4U();
                            break;
                        }
                        case 103U: {
                            self().visit_F32x4Ceil();
                            break;
                        }
                        case 104U: {
                            self().visit_F32x4Floor();
                            break;
                        }
                        case 105U: {
                            self().visit_F32x4Trunc();
                            break;
                        }
                        case 106U: {
                            self().visit_F32x4Nearest();
                            break;
                        }
                        case 224U: {
                            self().visit_F32x4Abs();
                            break;
                        }
                        case 225U: {
                            self().visit_F32x4Neg();
                            break;
                        }
                        case 227U: {
                            self().visit_F32x4Sqrt();
                            break;
                        }
                        case 228U: {
                            self().visit_F32x4Add();
                            break;
                        }
                        case 229U: {
                            self().visit_F32x4Sub();
                            break;
                        }
                        case 230U: {
                            self().visit_F32x4Mul();
                            break;
                        }
                        case 231U: {
                            self().visit_F32x4Div();
                            break;
                        }
                        case 232U: {
                            self().visit_F32x4Min();
                            break;
                        }
                        case 233U: {
                            self().visit_F32x4Max();
                            break;
                        }
                        case 234U: {
                            self().visit_F32x4Pmin();
                            break;
                        }
                        case 235U: {
                            self().visit_F32x4Pmax();
                            break;
                        }
                        case 116U: {
                            self().visit_F64x2Ceil();
                            break;
                        }
                        case 117U: {
                            self().visit_F64x2Floor();
                            break;
                        }
                        case 122U: {
                            self().visit_F64x2Trunc();
                            break;
                        }
                        case 148U: {
                            self().visit_F64x2Nearest();
                            break;
                        }
                        case 236U: {
                            self().visit_F64x2Abs();
                            break;
                        }
                        case 237U: {
                            self().visit_F64x2Neg();
                            break;
                        }
                        case 239U: {
                            self().visit_F64x2Sqrt();
                            break;
                        }
                        case 240U: {
                            self().visit_F64x2Add();
                            break;
                        }
                        case 241U: {
                            self().visit_F64x2Sub();
                            break;
                        }
                        case 242U: {
                            self().visit_F64x2Mul();
                            break;
                        }
                        case 243U: {
                            self().visit_F64x2Div();
                            break;
                        }
                        case 244U: {
                            self().visit_F64x2Min();
                            break;
                        }
                        case 245U: {
                            self().visit_F64x2Max();
                            break;
                        }
                        case 246U: {
                            self().visit_F64x2Pmin();
                            break;
                        }
                        case 247U: {
                            self().visit_F64x2Pmax();
                            break;
                        }
                        case 248U: {
                            self().visit_I32x4TruncSatF32x4S();
                            break;
                        }
                        case 249U: {
                            self().visit_I32x4TruncSatF32x4U();
                            break;
                        }
                        case 250U: {
                            self().visit_F32x4ConvertI32x4S();
                            break;
                        }
                        case 251U: {
                            self().visit_F32x4ConvertI32x4U();
                            break;
                        }
                        case 252U: {
                            self().visit_I32x4TruncSatF64x2SZero();
                            break;
                        }
                        case 253U: {
                            self().visit_I32x4TruncSatF64x2UZero();
                            break;
                        }
                        case 254U: {
                            self().visit_F64x2ConvertLowI32x4S();
                            break;
                        }
                        case 255U: {
                            self().visit_F64x2ConvertLowI32x4U();
                            break;
                        }
                        case 94U: {
                            self().visit_F32x4DemoteF64x2Zero();
                            break;
                        }
                        case 95U: {
                            self().visit_F64x2PromoteLowF32x4();
                            break;
                        }
                        default: {
                            throw LFortran::LFortranException("Incorrect num found for intruction with byte 0xFD");
                        }
                    }
                    break;
                }
                default: {
                    throw LFortran::LFortranException("Incorrect instruction byte found");
                }
            }
            cur_byte = wasm_bytes[offset++];
        }
    }
};

class WATVisitor : public BaseWASMVisitor<WATVisitor> {
    public:

    std::string src, indent;
    
    WATVisitor() : src(""), indent("") {}

    void visit_Return() { src += indent + "return"; }

    void visit_Call(uint32_t func_index) { src += indent + "call " + std::to_string(func_index); }

    void visit_LocalGet(uint32_t localidx) { src += indent + "local.get " + std::to_string(localidx); }

    void visit_LocalSet(uint32_t localidx) { src += indent + "local.set " + std::to_string(localidx); }

    void visit_I32Const(int value) { src += indent + "i32.const " + std::to_string(value); }

    void visit_I32Add() { src += indent + "i32.add"; }

    void visit_I32Sub() { src += indent + "i32.sub"; }

    void visit_I32Mul() { src += indent + "i32.mul"; }

    void visit_I32DivS() { src += indent + "i32.div_s"; }
};
