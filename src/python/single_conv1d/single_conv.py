import subprocess

from elasticai.creator.file_generation.on_disk_path import OnDiskPath
import torch
import os
from elasticai.creator.nn.sequential import Sequential
from elasticai.creator.nn.fixed_point.conv1d import Conv1d
from elasticai.creator.vhdl.system_integrations.firmware_env5 import FirmwareENv5

def delete_report_lines_in_dir(dir: str):
    for (root,dirs,files) in os.walk(dir, topdown=True):
        for file in files:
            file_path = os.path.join(root, file)
            out = subprocess.run(["sed", '-i', '', "/report/d", file_path],  capture_output=True)
            #print(out)

if __name__ == '__main__':
    torch.manual_seed(1)
    model = Sequential(Conv1d(total_bits=8, frac_bits=0, in_channels=1, out_channels=1, signal_length=5, kernel_size=3))
    #model = Sequential(Conv1d(total_bits=6, frac_bits=1, in_channels=1, out_channels=2, signal_length=5, kernel_size=3),
    #                   Conv1d(total_bits=6, frac_bits=1, in_channels=2, out_channels=1, signal_length=3, kernel_size=3))
    #print(f"conv.weight: {model[0].weight}")
    #print(f"conv.bias: {model[0].bias}")
    for module in model:
        ...
        module.weight = torch.nn.Parameter(data=torch.Tensor([[[-2, 1, 1]]]), requires_grad=True)
        module.bias = torch.nn.Parameter(data=torch.Tensor([1.]), requires_grad=True)
    #print(f"conv.weight: {model[0].weight}")
    #print(f"conv.bias: {model[0].bias}")
    x = torch.Tensor([[[0, 1, 2, 3, 4]]])


    y = model(x)
    #print(f"x: {x}")
    print(f"y: {y}")
    print(x.shape[2])
    print(y.shape[2])

    output_dir = "build_dir"
    destination = OnDiskPath(output_dir)
    network = model.create_design("network")
    network.save_to(destination.create_subpath("srcs"))
    ENv5 = FirmwareENv5(network, x_num_values=x.shape[2], y_num_values=y.shape[2], id=list(range(16)), skeleton_version="v2")
    ENv5.save_to(destination)

    print("Network created")
    delete_report_lines_in_dir(output_dir)

    print("run_Vivado")
    out = subprocess.run(["bash", "src/vivado_build/autobuild_binfile_vivado2021.1.sh", "es-admin", "65.108.38.237", "./build_dir", "./build_dir_output"], capture_output=True)
    print(out)

