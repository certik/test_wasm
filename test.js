const fs = require('fs');
const wasmBuffer = fs.readFileSync('./test.wasm');

WebAssembly.instantiate(wasmBuffer).then(wasmModule => {
    // Exported function live under instance.exports
    const get_const_val = wasmModule.instance.exports.get_const_val;
    const add_two_nums = wasmModule.instance.exports.add_two_nums;

    console.log(get_const_val());
    console.log(add_two_nums(5, 4));
    console.log("Success!")
});