#include "wasm_visitor.h"

namespace LFortran::WASM_INSTS_VISITOR {
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
} // namespace LFortran::WASM_INSTRUCTIONS_VISITOR
