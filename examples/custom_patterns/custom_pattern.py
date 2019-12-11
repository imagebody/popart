import popart
import ctypes
from pathlib import Path

so_path = Path('custom_pattern.so')
ctypes.cdll.LoadLibrary(so_path.resolve())

builder = popart.Builder()

i1 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2]))

x = builder.aiOnnx.relu([i1])

o = x
builder.addOutputTensor(o)

proto = builder.getModelProto()

dataFlow = popart.DataFlow(1, {o: popart.AnchorReturnType("ALL")})

opts = popart.SessionOptions()

popart.InferenceSession(fnModel=proto,
                        dataFeed=dataFlow,
                        userOptions=opts,
                        deviceInfo=popart.DeviceManager().createCpuDevice())