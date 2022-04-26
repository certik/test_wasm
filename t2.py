import leb128

module = bytearray([0x00, 0x61, 0x73, 0x6d])
version = bytearray([0x01, 0x00, 0x00, 0x00])

i32 = 0x7F
i64 = 0x7E
f32 = 0x7D
f64 = 0x7C

def emit_u32(code, i):
    code += leb128.u.encode(i)

def emit_fn_type(code, param_types, return_types):
    code += bytearray([0x60])
    emit_u32(code, len(param_types))
    code += bytearray(param_types)
    emit_u32(code, len(return_types))
    code += bytearray(return_types)

def emit_type_section(code):
    section = bytearray()
    emit_u32(section, 2) # number of functions
    emit_fn_type(section, [], [i32])
    emit_fn_type(section, [i32, i32], [i32])

    emit_u32(code, 1)
    emit_u32(code, len(section))
    code += section

def emit_function_section(code):
    section = bytearray()

    type_ids = bytearray([0, 1])
    emit_u32(section, len(type_ids))
    section += type_ids

    emit_u32(code, 3)
    emit_u32(code, len(section))
    code += section

def emit_export_fn(code, name, idx):
    name_bin = bytearray(name.encode(encoding="utf-8"))
    emit_u32(code, len(name_bin))
    code += name_bin
    code += bytearray([0x00])
    emit_u32(code, idx)

def emit_export_section(code):
    section = bytearray()
    emit_u32(section, 2) # number of functions
    emit_export_fn(section, "get_const_val", 0)
    emit_export_fn(section, "add_two_nums", 1)

    emit_u32(code, 7)
    emit_u32(code, len(section))
    code += section

def emit_var_list(code, vars):
    emit_u32(code, len(vars))
    code += bytearray(vars)

def emit_fn_code(code, local_vars, code1):
    fn_code = bytearray()
    emit_var_list(fn_code, local_vars)
    fn_code += code1
    emit_u32(code, len(fn_code))
    code += fn_code

def emit_code_section(code):
    section = bytearray()

    emit_u32(section, 2) # two functions

    instructions1 = bytearray([0x41]) + leb128.i.encode(-10)
    expr1 = instructions1 + bytearray([0x0b])
    emit_fn_code(section, [], expr1)

    get_inst2_0 = bytearray([0x20]) + leb128.u.encode(0)
    get_inst2_1 = bytearray([0x20]) + leb128.u.encode(1)
    add_inst2 = bytearray([0x6a])
    call_func2_0 = bytearray([0x10]) + leb128.u.encode(0)
    instructions2 = get_inst2_0 + get_inst2_1 + add_inst2 + call_func2_0 + add_inst2
    expr2 = instructions2 + bytearray([0x0b])
    emit_fn_code(section, [], expr2)

    emit_u32(code, 10)
    emit_u32(code, len(section))
    code += section


all_code = bytearray()
all_code += module
all_code += version
emit_type_section(all_code)
emit_function_section(all_code)
emit_export_section(all_code)
emit_code_section(all_code)

with open("test2.wasm", "wb") as wasm_file:
    wasm_file.write(bytes(all_code))
