#include <iostream>

template <class Derived>
class BaseWASMVisitor {
   private:
    Derived &self() { return static_cast<Derived &>(*this); }

   public:
    void visit_Return() { throw LFortran::LFortranException("visit_Return() not implemented"); }

    void visit_FunctionCall(uint32_t /*func_index*/) { throw LFortran::LFortranException("visit_FunctionCall() not implemented"); }

    void decode_function_instructions(uint32_t offset, Vec<uint8_t> wasm_bytes) {
        uint8_t cur_byte = wasm_bytes[offset++];
        while (cur_byte != 0x0B) {
            switch (cur_byte) {
                case 0x21 : {  // local.set
                    uint32_t index = read_unsinged_num(offset);
                    self().visit_LocalSet(index);
                    break;
                }
                case 0x6A: {  // i32.add
                    self().visit_I32Add();
                    break;
                }
                case 0x6B: {  // i32.sub
                    self().visit_I32Sub();
                    break;
                }
                case 0x6C: {  // i32.mul
                    self().visit_I32Mul();
                    break;
                }
                case 0x6D: {  // i32.div_s
                    self().visit_I32Div();
                    break;
                }
                case 0x10: {  // call func
                    uint32_t index = read_unsinged_num(offset);
                    self().visit_FunctionCall(index);
                    break;
                }
                case 0x0F: {
                    self().visit_Return();
                    break;
                }
                default: {
                    std::cout << "Error: Instruction " << std::to_string(cur_byte) << " not supported" << std::endl;
                    exit(1);
                }
            }
            cur_byte = wasm_bytes[offset++];
        }
    }
};

class WATVisitor : public BaseWASMVisitor<WATVisitor>
{
    std::string src;

    void visit_Return() {
        src += indent + "return";
    }
    
    void visit_FunctionCall(uint32_t func_index) {
        src += indent + "call " + std::to_string(func_index);
    }

    void visit_LocalSet(uint32_t local_index) {
        src += indent + "local.set " + std::to_string(local_index);
    }
}
