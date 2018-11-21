# see model0.py for a more detailed
# description of what's going on.

import sys
import os

import c10driver
import poponnx
from poponnx.torch import torchwriter
#we require torch in this file to create the torch Module
import torch

if (len(sys.argv) != 2):
    raise RuntimeError("onnx_net.py <log directory>")

outputdir = sys.argv[1]
if not os.path.exists(outputdir):
    print("Making %s" % (outputdir, ))
    os.mkdir(outputdir)

if (len(sys.argv) != 2):
    raise RuntimeError("onnx_net.py <log directory>")

outputdir = sys.argv[1]
if not os.path.exists(outputdir):
    print("Making %s" % (outputdir, ))
    os.mkdir(outputdir)

nInChans = 3
nOutChans = 10
batchSize = 2
batchesPerStep = 3
anchors = ["l1LossVal"]
art = poponnx.AnchorReturnType.ALL
dataFeed = poponnx.DataFlow(batchesPerStep, batchSize, anchors, art)
earlyInfo = poponnx.EarlyInfo()
earlyInfo.add("image0",
              poponnx.TensorInfo("FLOAT", [batchSize, nInChans, 32, 32]))
earlyInfo.add("image1",
              poponnx.TensorInfo("FLOAT", [batchSize, nInChans, 32, 32]))
earlyInfo.add("label", poponnx.TensorInfo("INT32", [batchSize]))
inNames = ["image0", "image1"]
cifarInIndices = {"image0": 0, "image1": 0, "label": 1}
outNames = ["out"]
losses = [poponnx.L1Loss("out", "l1LossVal", 0.01)]
willowOptPasses = [
    "PreUniRepl", "PostNRepl", "SoftmaxGradDirect", "SubtractArg1GradOp"
]


class Module0(torch.nn.Module):
    def __init__(self):
        torch.nn.Module.__init__(self)
        self.conv1 = torchwriter.conv3x3(nInChans, nOutChans)
        self.conv2 = torchwriter.conv3x3(nInChans, nOutChans)

    def forward(self, inputs):
        x = self.conv1(inputs[0])
        y = self.conv2(inputs[1])
        y = torch.sum(y, dim=(0, 1))
        x = x - y
        y = torch.sum(y, dim=0)
        return x + y


torchWriter = torchwriter.PytorchNetWriter(
    inNames=inNames,
    outNames=outNames,
    losses=losses,
    optimizer=poponnx.ConstSGD(0.001),
    earlyInfo=earlyInfo,
    dataFeed=dataFeed,
    ### Torch specific:
    module=Module0())

c10driver.run(torchWriter, willowOptPasses, outputdir, cifarInIndices)