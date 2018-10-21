#ifndef GUARD_NEURALNET_POPDEVICE_HPP
#define GUARD_NEURALNET_POPDEVICE_HPP

#pragma clang diagnostic push // start ignoring warnings
#pragma clang diagnostic ignored "-Weverything"
#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplin/Convolution.hpp>
#include <poputil/TileMapping.hpp>
#pragma clang diagnostic pop // stop ignoring warnings

#include <willow/device.hpp>
#include <willow/popx/enigma.hpp>
#include <willow/pritask.hpp>

namespace willow {
namespace popx {

class Opx;

poplar::Type getPopType(const TensorInfo &);

// A bundle class for an int and an Opx.
class OpxAndInIndex {
public:
  OpxAndInIndex(int, Opx *);
  OpxAndInIndex() = default;
  int index;
  Opx *opx;
};

class Devicex : public willow::Device {

public:
  Devicex(const Ir *);
  virtual void prepare() override final;
  Opx *getOpx(OpId);
  poplar::Graph &graph();

  // enigma has a PlanningCache for matmul and conv
  poplin::PlanningCache convCache;
  poplin::PlanningCache matmulCache;

  // completed in Devicex constructor.
  enigma::ConvOptions fwdConvOptions, bwdConvOptions, wuConvOptions;
  poplar::OptionFlags engineOptions;

  // return the name of the task which creates a poplar::Tensor
  // This function is pure string manipulation
  TaskId taskWhichCreates(TensorId);

private:
  std::unique_ptr<poplar::Graph> pGraph{nullptr};
  std::unique_ptr<poplar::Engine> pEngine{nullptr};
  std::unique_ptr<poplar::Target> pTarget{nullptr};
  poplar::Device popDevice;

  poplar::program::Sequence weightsToHost;
  poplar::program::Sequence optimizerToHost;
  poplar::program::Sequence weightsFromHost;
  poplar::program::Sequence step;

  PriTask createPopTensorTask(Tensor *tensor);
  std::unique_ptr<Opx> createOpx(Op *);

  // 1-to-1 mapping between Ops and Opxs
  std::map<OpId, std::unique_ptr<Opx>> opxs;
  std::map<TensorId, poplar::Tensor> pop_tensors;
};

} // namespace popx
} // namespace willow

#endif
