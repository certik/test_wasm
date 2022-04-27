import leb128

# -----------------------------------------------------------------------------
# Helpers for encoding integers and section lengths

module = bytearray([0x00, 0x61, 0x73, 0x6d])
version = bytearray([0x01, 0x00, 0x00, 0x00])

i32 = 0x7F
i64 = 0x7E
f32 = 0x7D
f64 = 0x7C

def emit_u32(code, i):
    code += leb128.u.encode(i)

def emit_i32(code, i):
    code += leb128.i.encode(-10)

def emit_b8(code, i):
    code += bytearray([i])

def emit_u32_b32_idx(code, idx, i):
    """
    Encodes the integer `i` using LEB128 and adds trailing zeros to always
    occupy 4 bytes. Stores the int `i` at the index `idx` in `code`.
    """
    num = leb128.u.encode(i)
    num_4b = bytearray([0x80, 0x80, 0x80, 0x00])
    assert len(num) <= 4
    for i in range(len(num)):
        num_4b[i] |= num[i]
    for i in range(4):
        code[idx+i] = num_4b[i]

def fixup_len(code, len_idx):
    section_len = len(code)-len_idx-4
    emit_u32_b32_idx(code, len_idx, section_len)

def emit_len_placeholder(code):
    len_idx = len(code)
    code += bytearray([0x00, 0x00, 0x00, 0x00]) # Length placeholder
    return len_idx

# -----------------------------------------------------------------------------
# Functions to emit WASM sections

def emit_fn_type(code, param_types, return_types):
    emit_b8(code, 0x60)
    emit_u32(code, len(param_types))
    code += bytearray(param_types)
    emit_u32(code, len(return_types))
    code += bytearray(return_types)

def emit_type_section(code):
    emit_u32(code, 1)
    len_idx = emit_len_placeholder(code)
    emit_u32(code, 2) # number of functions
    emit_fn_type(code, [], [i32])
    emit_fn_type(code, [i32, i32], [i32])
    fixup_len(code, len_idx)

def emit_function_section(code):
    emit_u32(code, 3)
    len_idx = emit_len_placeholder(code)
    emit_u32(code, 2) # Number of functions
    # Function indices:
    emit_u32(code, 0)
    emit_u32(code, 1)
    fixup_len(code, len_idx)

def emit_export_fn(code, name, idx):
    name_bin = bytearray(name.encode(encoding="utf-8"))
    emit_u32(code, len(name_bin))
    code += name_bin
    emit_b8(code, 0x00)
    emit_u32(code, idx)

def emit_export_section(code):
    emit_u32(code, 7)
    len_idx = emit_len_placeholder(code)
    emit_u32(code, 2) # number of functions
    emit_export_fn(code, "get_const_val", 0)
    emit_export_fn(code, "add_two_nums", 1)
    fixup_len(code, len_idx)

def emit_function_1(code):
    len_idx = emit_len_placeholder(code)
    # Local vars
    emit_u32(code, 0)
    # Instructions
    emit_b8(code, 0x41)
    emit_i32(code, -10)
    emit_b8(code, 0x0b)
    fixup_len(code, len_idx)

def emit_function_2(code):
    len_idx = emit_len_placeholder(code)
    # Local vars
    emit_u32(code, 0)
    # Instructions
    emit_b8(code, 0x20)
    emit_u32(code, 0)
    emit_b8(code, 0x20)
    emit_u32(code, 1)
    emit_b8(code, 0x6a)
    emit_b8(code, 0x10)
    emit_u32(code, 0)
    emit_b8(code, 0x6a)
    emit_b8(code, 0x0b)
    fixup_len(code, len_idx)

def emit_code_section(code):
    emit_u32(code, 10)
    len_idx = emit_len_placeholder(code)
    emit_u32(code, 2) # two functions
    emit_function_1(code)
    emit_function_2(code)
    fixup_len(code, len_idx)

# -----------------------------------------------------------------------------
# Emit the WASM file

all_code = bytearray()
all_code += module
all_code += version
emit_type_section(all_code)
emit_function_section(all_code)
emit_export_section(all_code)
emit_code_section(all_code)

with open("test3.wasm", "wb") as wasm_file:
    wasm_file.write(bytes(all_code))
