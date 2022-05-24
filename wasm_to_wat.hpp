#include <iostream>
#include <vector>
#include <unordered_map>

std::unordered_map<uint8_t, std::string> type_to_string = {
        {0x7F, "i32"}, {0x7E, "i64"}, {0x7D, "f32"}, {0x7C, "f64"}};

std::unordered_map<uint8_t, std::string> kind_to_string = {
        {0x00, "func"}, {0x01, "table"}, {0x02, "mem"}, {0x03, "global"}};
        
struct FuncType {
    std::vector<uint8_t> param_types;
    std::vector<uint8_t> result_types;

    // std::string to_string() {
    //     std::string result = "(param";
    //     for (auto &type : param_types) {
    //         result += " " + type_to_string[type];
    //     }
    //     result += ") (result";
    //     for (auto &type : result_types) {
    //         result += " " + type_to_string[type];
    //     }
    //     result += ")";
    //     return result;
    // }
};

struct Export {
    std::string name;
    uint8_t kind;
    uint32_t index;

    // std::string to_string() {
    //     std::string result =
    //         "(export " + name + " (" + kind_to_string[kind] + " $" + std::to_string(index) + "))";
    //     return result;
    // }
};

class Instruction {
   public:
    uint8_t inst_code;
    virtual std::string to_string() { return "null"; }
    Instruction(uint8_t inst_code) : inst_code(inst_code) {}
};

class I32ConstInst : public Instruction {
   public:
    int val;
    I32ConstInst(uint8_t inst_code, int val)
        : Instruction(inst_code), val(val) {}

    std::string to_string() { return "i32.const " + std::to_string(val); }
};

class I64ConstInst : public Instruction {
   public:
    long long int val;
    I64ConstInst(uint8_t inst_code, long long int val)
        : Instruction(inst_code), val(val) {}

    std::string to_string() { return "i64.const " + std::to_string(val); }
};

class I32AddInst : public Instruction {
   public:
    I32AddInst(uint8_t inst_code) : Instruction(inst_code) {}

    std::string to_string() { return "i32.add"; }
};

class I32SubInst : public Instruction {
   public:
    I32SubInst(uint8_t inst_code) : Instruction(inst_code) {}

    std::string to_string() { return "i32.sub"; }
};

class I32MulInst : public Instruction {
   public:
    I32MulInst(uint8_t inst_code) : Instruction(inst_code) {}

    std::string to_string() { return "i32.mul"; }
};

class I32DivInst : public Instruction {
   public:
    I32DivInst(uint8_t inst_code) : Instruction(inst_code) {}

    std::string to_string() { return "i32.div_s"; }
};

class LocalGetInst : public Instruction {
   public:
    uint32_t local_index;
    LocalGetInst(uint8_t inst_code, uint32_t local_index)
        : Instruction(inst_code), local_index(local_index) {}

    std::string to_string() {
        return "local.get " + std::to_string(local_index);
    }
};

class LocalSetInst : public Instruction {
   public:
    uint32_t local_index;
    LocalSetInst(uint8_t inst_code, uint32_t local_index)
        : Instruction(inst_code), local_index(local_index) {}

    std::string to_string() {
        return "local.set " + std::to_string(local_index);
    }
};

class CallInst : public Instruction {
   public:
    uint32_t func_index;
    CallInst(uint8_t inst_code, uint32_t func_index)
        : Instruction(inst_code), func_index(func_index) {}

    std::string to_string() { return "call " + std::to_string(func_index); }
};

class ReturnInst: public Instruction {
   public:
    ReturnInst(uint8_t inst_code) : Instruction(inst_code) {}

    std::string to_string() { return "return"; }
};

struct Local {
    uint32_t count;
    uint8_t type;
};

struct Code {
    int size;
    std::vector<Local> locals;
    std::vector<std::unique_ptr<Instruction>> instructions;

    // std::string to_string() {
    //     std::string result = "(local";
    //     for (auto &local : locals) {
    //         for(uint32_t i = 0; i < local.count; i++) {
    //             result += " " + type_to_string[local.type];
    //         }
    //     }
    //     result += ")(";
    //     for (auto &inst : instructions) {
    //         result += "(" + inst.to_string() + ")";
    //     }
    //     result += ")";
    //     return result;
    // }
};
