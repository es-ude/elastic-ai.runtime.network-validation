import os
from io import BytesIO
from pathlib import Path

from flask import Flask, send_file

bytes_per_request = 512  # should be matching with the page size of the flash

app = Flask(__name__)

path_to_binfile = "../../../build_dir_output/output/env5_top_reconfig.bin"
def print_bytearray(arr: bytes):
    print("0x" + " 0x".join("%02X" % b for b in arr))


def read_slice(position: int, filename: str) -> bytes:
    with open(filename, "rb") as file:
        file.seek(position * bytes_per_request, 0)
        chunk: bytes = file.read(bytes_per_request)
        # print(f"Chunk {position}:")
        # print_bytearray(chunk)
    return chunk

@app.route("/binfile_length")
def get_binfile_length():
    size = os.path.getsize(path_to_binfile)
    return str(size)


@app.route("/binfile/<position>")
def get_binfile(position: str):
    """
    Using positional arguments as parameter did not work!
    """
    buffer = BytesIO()
    buffer.write(
        read_slice(int(position), path_to_binfile)
    )
    buffer.seek(0)
    return send_file(
        buffer,
        as_attachment=True,
        download_name="binfile.bin",
        mimetype="application/octet-stream",
    )

@app.route("/check")
def check_server_available():
    return "AVAILABLE\0"
@app.route("/data_to_compute_length")
def get_data_to_compute_length():
    return str(5)

@app.route("/data_to_compute")
def get_data_to_compute():
    data = [0, 1, 2, 3, 4]
    data_as_str = [str(x) for x in data]
    return ";".join(data_as_str)

@app.route("/result_length")
def get_size():
    return str(3)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=1234, debug=True)
