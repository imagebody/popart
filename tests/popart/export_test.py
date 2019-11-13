import numpy as np
import popart
import test_util as tu

# model:
#
#  i1 --|
#       |-- Add --|
#  i2 --|         |
#                 |---> o2
#  c -------------|
#
#  where i1, i2 are streamed and c is constant


def test_constants_preserved():

    popart.getLogger().setLevel("TRACE")

    # Check that `session.modelToHost` can be called when using a
    # model with a constant node, without throwing an exceptions
    builder = popart.Builder()

    i1 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2, 2]))
    i2 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2, 2]))
    c = builder.aiOnnx.constant(np.array([[1, 2], [3, 4]], dtype=np.float32))
    o1 = builder.aiOnnx.add([i1, i2])
    o2 = builder.aiOnnx.add([o1, c])
    builder.addOutputTensor(o2)

    proto = builder.getModelProto()

    anchors = {o2: popart.AnchorReturnType("ALL")}

    dataFlow = popart.DataFlow(1, anchors)

    optimizer = popart.ConstSGD(0.01)

    losses = [popart.L1Loss(o2, "l1LossVal", 0.1)]

    opts = popart.SessionOptions()

    session = popart.TrainingSession(fnModel=proto,
                                     dataFeed=dataFlow,
                                     userOptions=opts,
                                     losses=losses,
                                     optimizer=optimizer,
                                     deviceInfo=tu.get_poplar_cpu_device())

    anchorArrays = session.initAnchorArrays()

    session.prepareDevice()

    inputs = {
        i1: np.array([[2, 2], [2, 2]]).astype(np.float32),
        i2: np.array([[4, 4], [4, 4]]).astype(np.float32),
    }
    pystepio = popart.PyStepIO(inputs, anchorArrays)
    session.run(pystepio)

    session.modelToHost('session_proto.onnx')

    # models should be the same after training
    # as there are no trainable parameters
    with open('session_proto.onnx', 'rb') as f:
        session_proto = f.read()
    assert proto == session_proto

    # confirm that the output is correct. See T6186, which this tests
    assert (np.sum(np.abs(anchorArrays[o2] - np.array([[7, 8], [9, 10]]))) <
            1e-8)
