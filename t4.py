import leb128

class WASMAssembler:
    i32 = 0x7F
    i64 = 0x7E
    f32 = 0x7D
    f64 = 0x7C

    def __init__(self):
        self.code = bytearray()

    def save_bin(self, filename):
        with open(filename, "wb") as wasm_file:
            wasm_file.write(bytes(self.code))

    def emit_header(self):
        module = bytearray([0x00, 0x61, 0x73, 0x6d])
        version = bytearray([0x01, 0x00, 0x00, 0x00])
        self.code += module
        self.code += version

    def emit_u32(self, i):
        self.code += leb128.u.encode(i)

    def emit_i32(self, i):
        self.code += leb128.i.encode(-10)

    def emit_b8(self, i):
        self.code += bytearray([i])

    def emit_u32_b32_idx(self, idx, i):
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
            self.code[idx+i] = num_4b[i]

    def fixup_len(self, len_idx):
        section_len = len(self.code)-len_idx-4
        self.emit_u32_b32_idx(len_idx, section_len)

    def emit_len_placeholder(self):
        len_idx = len(self.code)
        self.code += bytearray([0x00, 0x00, 0x00, 0x00]) # Length placeholder
        return len_idx


# -----------------------------------------------------------------------------
# Functions to emit WASM sections

def emit_fn_type(a, param_types, return_types):
    a.emit_b8(0x60)
    a.emit_u32(len(param_types))
    a.code += bytearray(param_types)
    a.emit_u32(len(return_types))
    a.code += bytearray(return_types)

def emit_type_section(a):
    a.emit_u32(1)
    len_idx = a.emit_len_placeholder()
    a.emit_u32(2) # number of functions
    emit_fn_type(a, [], [a.i32])
    emit_fn_type(a, [a.i32, a.i32], [a.i32])
    a.fixup_len(len_idx)

def emit_function_section(a):
    a.emit_u32(3)
    len_idx = a.emit_len_placeholder()
    a.emit_u32(2) # Number of functions
    # Function indices:
    a.emit_u32(0)
    a.emit_u32(1)
    a.fixup_len(len_idx)

def emit_export_fn(a, name, idx):
    name_bin = bytearray(name.encode(encoding="utf-8"))
    a.emit_u32(len(name_bin))
    a.code += name_bin
    a.emit_b8(0x00)
    a.emit_u32(idx)

def emit_export_section(a):
    a.emit_u32(7)
    len_idx = a.emit_len_placeholder()
    a.emit_u32(2) # number of functions
    emit_export_fn(a, "get_const_val", 0)
    emit_export_fn(a, "add_two_nums", 1)
    a.fixup_len(len_idx)

def emit_function_1(a):
    len_idx = a.emit_len_placeholder()
    # Local vars
    a.emit_u32(0)
    # Instructions
    a.emit_b8(0x41)
    a.emit_i32(-10)
    a.emit_b8(0x0b)
    a.fixup_len(len_idx)

def emit_function_2(a):
    len_idx = a.emit_len_placeholder()
    # Local vars
    a.emit_u32(0)
    # Instructions
    a.emit_b8(0x20)
    a.emit_u32(0)
    a.emit_b8(0x20)
    a.emit_u32(1)
    a.emit_b8(0x6a)
    a.emit_b8(0x10)
    a.emit_u32(0)
    a.emit_b8(0x6a)
    a.emit_b8(0x0b)
    a.fixup_len(len_idx)

def emit_code_section(a):
    a.emit_u32(10)
    len_idx = a.emit_len_placeholder()
    a.emit_u32(2) # two functions
    emit_function_1(a)
    emit_function_2(a)
    a.fixup_len(len_idx)

# -----------------------------------------------------------------------------
# Emit the WASM file

a = WASMAssembler()
a.emit_header()
emit_type_section(a)
emit_function_section(a)
emit_export_section(a)
emit_code_section(a)
a.save_bin("test4.wasm")
