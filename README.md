Run:

    conda create -n wasm python
    conda activate wasm
    pip install leb128
    python t1.py
    python -m http.server

Open: http://localhost:8000/a.html

Then open a JavaScript console and you should see:

    -10
    -1
    Success!

To test `t4.py`:

    python t4.py
    cp test4.wasm test.wasm
    python -m http.server
