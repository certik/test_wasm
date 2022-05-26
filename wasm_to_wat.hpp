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
    std::vector<std::unique_ptr<Instruction>> instructions;
};
