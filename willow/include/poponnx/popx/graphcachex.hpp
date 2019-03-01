#ifndef GUARD_NEURALNET_GRAPHCACHE_HPP
#define GUARD_NEURALNET_GRAPHCACHE_HPP

#include <poplin/Convolution.hpp>
#include <poplin/MatMul.hpp>
#include <poputil/GraphFunction.hpp>
#include <poponnx/popx/poplaroptionsx.hpp>

#include <map>

namespace poponnx {
namespace popx {

class GraphCachex {
public:
  GraphCachex();
  /** Wrapper for poplin::convolution, which tries to cache parts of the poplar
   * graph.
   *
   * This is for a 2D convolution.
   *
   * The input tensor is in the form [B x inChans x H x W], and can be allocated
   * using createInput().  The weights tensor is in the form
   * [convGroups x outChans x inChans x H x W], and can be allocated using
   * createWeights().
   *
   * Padding and striding are specified in the ConvParams structure.
   *
   * \param graph                   The operation will be added to this graph
   * \param in                      Input data tensor
   * \param weights                 Weights tensor
   * \param params                  Parameters for the form of the convolution
   * \param transposeAndFlipWeights For the weight update pass
   * \param prog                    Poplar program sequence to append to op onto
   * \param cacheOperation              Whether to use caching of the Poplar
   * Graph for this operation. \param debugPrefix             Name of the
   * operation, for debugging \param options                 Options that
   * control the implementation \param cache                   Optional pointer
   * to planning cache to use \return                        The convolved
   * output tensor
   */
  poplar::Tensor convolution(poplar::Graph &graph,
                             const poplar::Tensor &in,
                             const poplar::Tensor &weights,
                             const poplin::ConvParams &params,
                             const bool transposeAndFlipWeights,
                             poplar::program::Sequence &prog,
                             bool cacheOperation,
                             const std::string &debugPrefix = "",
                             const PoplarOptions &options   = {},
                             poplin::PlanningCache *cache   = nullptr,
                             const int64_t &virtualGraphId  = 0);

  /** Wrapper for poplin::convolution, which tries to cache parts of the poplar
   *  graph.
   *
   * Padding and striding are specified in the ConvParams structure.
   *
   * \param graph                   The operation will be added to this graph
   * \param zDeltas                 zDeltas tensor
   * \param activations             Activations tensor
   * \param params                  Parameters for the form of the convolution
   * \param prog                    Poplar program sequence to append to op onto
   * \param cacheOperation              Whether to use caching of the Poplar
   * Graph for this operation. \param debugPrefix             Name of the
   * operation, for debugging \param options                 Options that
   * control the implementation \param cache                   Optional pointer
   * to planning cache to use \return                        The weight deltas
   */
  poplar::Tensor calculateWeightDeltas(poplar::Graph &graph,
                                       const poplar::Tensor &zDeltas,
                                       const poplar::Tensor &activations,
                                       const poplin::ConvParams &params,
                                       poplar::program::Sequence &prog,
                                       bool cacheOperation,
                                       const std::string &debugPrefix = "",
                                       const PoplarOptions &options   = {},
                                       poplin::PlanningCache *cache   = nullptr,
                                       const int64_t &virtualGraphId  = 0);

  poplar::Tensor matMulGrouped(poplar::Graph &graph,
                               const poplar::Tensor &zDeltas,
                               const poplar::Tensor &activations,
                               poplar::program::Sequence &prog,
                               bool cacheOperation,
                               const std::string &debugPrefix,
                               const PoplarOptions &options,
                               poplin::matmul::PlanningCache *cache,
                               const int64_t &virtualGraphId);

private:
  poplar::Tensor createCachedConvolution(poplar::Graph &graph,
                                         const poplar::Tensor &in,
                                         const poplar::Tensor &weights,
                                         const poplin::ConvParams &params,
                                         bool transposeAndFlipWeights,
                                         poplar::program::Sequence &prog,
                                         bool cacheOperation,
                                         const std::string &debugPrefix,
                                         const PoplarOptions &options,
                                         poplin::PlanningCache *cache,
                                         const int64_t &virtualGraphId);

  poplar::Tensor cachedCalculateWeightDeltas(poplar::Graph &graph,
                                             const poplar::Tensor &zDeltas,
                                             const poplar::Tensor &activations,
                                             const poplin::ConvParams &params,
                                             poplar::program::Sequence &prog,
                                             bool cacheOperation,
                                             const std::string &debugPrefix,
                                             const PoplarOptions &options,
                                             poplin::PlanningCache *cache,
                                             const int64_t &virtualGraphId);

  void createCachedBwdWeights(poplar::Graph &,
                              const poplar::Tensor &,
                              const poplar::Tensor &,
                              poplar::program::Sequence &,
                              const std::string &,
                              const int64_t &virtualGraphId);

  // Definitions for the cache:
  // TODO T5541 - Take poponnx node placement into account for cached graphs
  // Signature of a poplar Tensor
  using PoplarTensorSignature =
      std::pair<poplar::Type, std::vector<std::size_t>>;
  // Key used for convolutions
  using ConvolutionCacheKey = std::tuple<PoplarTensorSignature,
                                         PoplarTensorSignature,
                                         poplin::ConvParams,
                                         std::map<std::string, std::string>,
                                         bool,
                                         int64_t>;
  // Key used for caching weight deltas calculations
  using CalculateWeightDeltasCacheKey =
      std::tuple<PoplarTensorSignature,
                 PoplarTensorSignature,
                 poplin::ConvParams,
                 std::map<std::string, std::string>,
                 int64_t>;
  // Key used for weightsTransposeChansFlipXY
  using BwdWeightCacheKey =
      std::tuple<PoplarTensorSignature, PoplarTensorSignature, int64_t>;

  // Key used for matmul
  using MatMulCacheKey = std::tuple<PoplarTensorSignature,
                                    PoplarTensorSignature,
                                    std::map<std::string, std::string>,
                                    int64_t>;

  // Cache maps definitions
  using ConvolutionGraphCache =
      std::map<ConvolutionCacheKey, poputil::graphfn::TensorFunction>;
  using CalculateWeightDeltasGraphCache =
      std::map<CalculateWeightDeltasCacheKey, poputil::graphfn::TensorFunction>;
  using BwdWeightGraphCache =
      std::map<BwdWeightCacheKey, poputil::graphfn::VoidFunction>;
  using MatMulGraphCache =
      std::map<MatMulCacheKey, poputil::graphfn::TensorFunction>;

  // Caches
  ConvolutionGraphCache convolutionGraphCache;
  CalculateWeightDeltasGraphCache calculateWeightDeltasGraphCache;
  BwdWeightGraphCache bwdWeightGraphCache;
  MatMulGraphCache matmulGraphCache;

  static PoplarTensorSignature getPoplarTensorSignature(const poplar::Tensor &);

  static ConvolutionCacheKey getConvolutionCacheKey(const poplin::ConvParams &,
                                                    const PoplarOptions &,
                                                    const bool &,
                                                    const int64_t &);

  static CalculateWeightDeltasCacheKey
  getCalculateWeightDeltasKey(const poplar::Tensor &,
                              const poplar::Tensor &,
                              const poplin::ConvParams &,
                              const PoplarOptions &,
                              const int64_t &);

  static BwdWeightCacheKey getBwdWeightCacheKey(const poplar::Tensor &,
                                                const poplar::Tensor &,
                                                const int64_t &);

  static MatMulCacheKey getMatMulCacheKey(const poplar::Tensor &,
                                          const poplar::Tensor &,
                                          const PoplarOptions &,
                                          const int64_t &);
};

} // namespace popx
} // namespace poponnx

#endif
