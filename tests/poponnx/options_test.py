# Test for basic importing

import pytest

# the core library
import poponnx


def test_create_empty_options():

    opts = poponnx.SessionOptions()
    assert (opts is not None)
    assert (opts.exportDot == False)
    assert (len(opts.engineOptions) == 0)
    assert (len(opts.convolutionOptions) == 0)
    assert (len(opts.reportOptions) == 0)


def test_set_exportDot_flag():

    opts = poponnx.SessionOptions()
    opts.exportDot = True

    assert (opts.exportDot == True)


def test_set_engineOptions():

    opts = poponnx.SessionOptions()
    opts.engineOptions = {'option': 'value'}

    assert (len(opts.engineOptions) == 1)
    assert (len(opts.convolutionOptions) == 0)
    assert (len(opts.reportOptions) == 0)
    assert (opts.engineOptions['option'] == 'value')


def test_set_convolutionOptions():

    opts = poponnx.SessionOptions()
    opts.convolutionOptions = {'option': 'value'}

    assert (len(opts.engineOptions) == 0)
    assert (len(opts.convolutionOptions) == 1)
    assert (len(opts.reportOptions) == 0)
    assert (opts.convolutionOptions['option'] == 'value')


def test_set_reportOptions():

    opts = poponnx.SessionOptions()
    opts.reportOptions = {'option': 'value'}

    assert (len(opts.engineOptions) == 0)
    assert (len(opts.convolutionOptions) == 0)
    assert (len(opts.reportOptions) == 1)
    assert (opts.reportOptions['option'] == 'value')


def test_engine_options_passed_to_engine():
    builder = poponnx.Builder()

    i1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    i2 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))

    o = builder.add(i1, i2)

    builder.addOutputTensor(o)

    proto = builder.getModelProto()

    earlyInfo = poponnx.EarlyInfo()
    earlyInfo.add(i1, poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    earlyInfo.add(i2, poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))

    dataFlow = poponnx.DataFlow(1, 1, [], poponnx.AnchorReturnType.ALL)
    optimizer = poponnx.SGD(0.01)
    losses = [poponnx.L1Loss(o, "l1LossVal", 0.1)]

    opts = poponnx.SessionOptions()
    opts.engineOptions = {'option': 'value'}
    opts.logging = {'all': 'DEGBUG'}

    session = poponnx.Session(
        fnModel=proto,
        earlyInfo=earlyInfo,
        dataFeed=dataFlow,
        losses=losses,
        optimizer=optimizer,
        outputdir="/tmp",
        userOptions=opts)

    session.initAnchorArrays()
    session.setDevice("IPU")

    with pytest.raises(poponnx.exception) as e_info:
        session.prepareDevice()

    assert (e_info.value.args[0].endswith("Unrecognised option 'option'"))
