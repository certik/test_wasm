import leb128

module = bytearray(bytearray([0x00, 0x61, 0x73, 0x6d]))
version = bytearray([0x01, 0x00, 0x00, 0x00])

# type section
param_types_list1 = bytearray([])
param_types1 = leb128.u.encode(len(param_types_list1)) + param_types_list1

return_types_list1 = bytearray([0x7f])
return_types1 = leb128.u.encode(len(return_types_list1)) + return_types_list1

func_type1 = bytearray([0x60]) + param_types1 + return_types1

param_types_list2 = bytearray([0x7f, 0x7f])
param_types2 = leb128.u.encode(len(param_types_list2)) + param_types_list2

return_types_list2 = bytearray([0x7f])
return_types2 = leb128.u.encode(len(return_types_list2)) + return_types_list2

func_type2 = bytearray([0x60]) + param_types2 + return_types2

func_types = [func_type1, func_type2]

type_section_id = leb128.u.encode(1)
type_section_content = leb128.u.encode(
    len(func_types))  # length of vector of functype
for func_type in func_types:
    type_section_content.extend(func_type)
type_section_body = leb128.u.encode(
    len(type_section_content)) + type_section_content
type_section = type_section_id + type_section_body

# function section
type_ids = bytearray([0, 1])

func_section_id = leb128.u.encode(3)
func_section_content = leb128.u.encode(
    len(type_ids))  # length of vector of functype
func_section_content += type_ids
func_section_body = leb128.u.encode(
    len(func_section_content)) + func_section_content
func_section = func_section_id + func_section_body

# code section
local_vars_list1 = bytearray([])
local_vars1 = leb128.u.encode(len(local_vars_list1)) + local_vars_list1

instructions1 = bytearray([0x41]) + leb128.i.encode(-10)
expr1 = instructions1 + bytearray([0x0b])

func1 = local_vars1 + expr1

code1 = leb128.u.encode(len(func1)) + func1

local_vars_list2 = bytearray([])
local_vars2 = leb128.u.encode(len(local_vars_list2)) + local_vars_list2
get_inst2_0 = bytearray([0x20]) + leb128.u.encode(0)
get_inst2_1 = bytearray([0x20]) + leb128.u.encode(1)
add_inst2 = bytearray([0x6a])
call_func2_0 = bytearray([0x10]) + leb128.u.encode(0)
instructions2 = get_inst2_0 + get_inst2_1 + add_inst2 + call_func2_0 + add_inst2
expr2 = instructions2 + bytearray([0x0b])

func2 = local_vars2 + expr2

code2 = leb128.u.encode(len(func2)) + func2

codes = [code1, code2]

code_section_id = leb128.u.encode(10)
code_section_content = leb128.u.encode(len(codes))  # length of vector of codes
for code in codes:
    code_section_content.extend(code)
code_section_body = leb128.u.encode(
    len(code_section_content)) + code_section_content
code_section = code_section_id + code_section_body


# export section
actual_name1 = "get_const_val"
# should encode the len of bytearray instead of direct actual_name
name1 = leb128.u.encode(len(actual_name1)) + \
    bytearray(actual_name1.encode(encoding="utf-8"))
export_desc1 = bytearray([0x00]) + leb128.u.encode(0)  # encoding function index
export1 = name1 + export_desc1

actual_name2 = "add_two_nums"
# should encode the len of bytearray instead of direct actual_name
name2 = leb128.u.encode(len(actual_name2)) + \
    bytearray(actual_name2.encode(encoding="utf-8"))
export_desc2 = bytearray([0x00]) + leb128.u.encode(1)  # encoding function index
export2 = name2 + export_desc2

exports = [export1, export2]

export_section_id = leb128.u.encode(7)
export_section_content = leb128.u.encode(
    len(exports))  # length of vector of exports
for export in exports:
    export_section_content.extend(export)
export_section_body = leb128.u.encode(
    len(export_section_content)) + export_section_content
export_section = export_section_id + export_section_body

all_code = module + version + type_section + \
    func_section + export_section + code_section

with open("test.wasm", "wb") as wasm_file:
    wasm_file.write(bytes(all_code))
