import numpy as np
import poponnx
import pytest


def test_auto_virtual_graph_subgraphs_2():

    ipus = 2

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))
    input2 = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    # Subgraph 0
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x0 = builder.aiOnnx.matmul([input1, w])
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x0 = builder.aiOnnx.matmul([x0, w])

    # Subgraph 1
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x1 = builder.aiOnnx.matmul([input2, w])

    # Subgraph 2
    x2 = builder.aiOnnx.add([x0, x1])
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x2 = builder.aiOnnx.matmul([x2, w])

    output = x2
    builder.addOutputTensor(output)

    # Desired split is:
    # ipu1: 0. ipu2: 1,2

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(1, {output: poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    poponnx.Session(fnModel=proto, dataFeed=dataFlow, userOptions=opts)


def test_auto_virtual_graph_subgraphs_4():

    ipus = 4

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))
    input2 = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    # Subgraph 0
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x0 = builder.aiOnnx.matmul([input1, w])
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x0 = builder.aiOnnx.matmul([x0, w])

    # Subgraph 1
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x1 = builder.aiOnnx.matmul([input2, w])

    # Subgraph 2
    x2 = builder.aiOnnx.add([x0, x1])
    w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
    x2 = builder.aiOnnx.matmul([x2, w])

    output = x2
    builder.addOutputTensor(output)

    # Desired split is:
    # ipu1: 0, ipu2: 0.5, ipu3: 1, ipu4: 2

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(1, {output: poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    poponnx.Session(fnModel=proto, dataFeed=dataFlow, userOptions=opts)


def test_auto_virtual_graph_inf_2():

    ipus = 2

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    x = input
    for i in range(ipus):
        w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
        x = builder.aiOnnx.matmul([x, w])
    output = x
    builder.addOutputTensor(output)

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(1, {output: poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    poponnx.Session(fnModel=proto, dataFeed=dataFlow, userOptions=opts)


def test_auto_virtual_graph_inf_many():

    ipus = 4

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    x = input
    for i in range(16):
        w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
        x = builder.aiOnnx.matmul([x, w])
    output = x
    builder.addOutputTensor(output)

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(1, {output: poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    poponnx.Session(fnModel=proto, dataFeed=dataFlow, userOptions=opts)


def test_auto_virtual_graph_train():

    ipus = 2

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    x = input
    for i in range(ipus):
        w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
        x = builder.aiOnnx.matmul([x, w])
    output = x
    builder.addOutputTensor(output)

    label = builder.addInputTensor(poponnx.TensorInfo("INT32", [1]))

    loss = poponnx.NllLoss(output, label, "nllLossVal")

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(
        1, {loss.output(0): poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    poponnx.Session(
        fnModel=proto,
        dataFeed=dataFlow,
        userOptions=opts,
        losses=[loss],
        optimizer=poponnx.SGD(0.01))


def test_auto_virtual_graph_not_enough_splits():
    ipus = 4

    poponnx.getLogger().setLevel("TRACE")

    builder = poponnx.Builder()

    input_shape = [1, 64]
    input = builder.addInputTensor(poponnx.TensorInfo("FLOAT16", input_shape))

    x = input
    for i in range(2):
        w = builder.addInitializedInputTensor(np.zeros([64, 64], np.float16))
        x = builder.aiOnnx.matmul([x, w])
    output = x
    builder.addOutputTensor(output)

    proto = builder.getModelProto()

    dataFlow = poponnx.DataFlow(1, {output: poponnx.AnchorReturnType("FINAL")})

    opts = poponnx.SessionOptions()
    opts.enableVirtualGraphs = True
    opts.autoVirtualGraph = True
    opts.minimumVirtualGraphCount = ipus

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        poponnx.Session(fnModel=proto, dataFeed=dataFlow, userOptions=opts)

    assert (e_info.value.args[0].startswith(
        "[AutoVirtualGraph] Couldn't find enough splits"))