#pragma once

#include "NNFuncs.hpp"
#include "NNLayer.hpp"
#include "algorithms/util/FluidEigenMappings.hpp"
#include "data/FluidDataSet.hpp"
#include "data/FluidIndex.hpp"
#include "data/FluidTensor.hpp"
#include "data/TensorTypes.hpp"
#include <Eigen/Core>
#include <random>

namespace fluid {
namespace algorithm {

class MLP {
  using ArrayXd = Eigen::ArrayXd;
  using ArrayXXd = Eigen::ArrayXXd;

public:
  explicit MLP() = default;
  ~MLP() = default;

  void init(index inputSize, index outputSize,
            FluidTensor<index, 1> hiddenSizes, index hiddenAct, index outputAct) {
    mLayers.clear();
    std::vector<index> sizes = {inputSize};
    std::vector<index> activations = {};
    for (auto &&s : hiddenSizes){
      sizes.push_back(s);
      activations.push_back(hiddenAct);
    }
    sizes.push_back(outputSize);
    activations.push_back(outputAct);
    for (index i = 0; i < sizes.size() - 1; i++) {
      mLayers.push_back(NNLayer(sizes[i], sizes[i + 1], activations[i]));
    }
    for (auto &&l : mLayers)
      l.init();
    mInitialized = true;
    mTrained = false;
  }

  void getParameters(index layer, RealMatrixView W, RealVectorView b, index& layerType) const {
    using namespace _impl;
    W = asFluid(mLayers[layer].getWeights());
    b = asFluid(mLayers[layer].getBiases());
    layerType = mLayers[layer].getActType();
  }

  void setParameters(index layer, RealMatrixView W, RealVectorView b, index layerType) {
    using namespace Eigen;
    using namespace std;
    using namespace _impl;
    MatrixXd weights = asEigen<Matrix>(W);
    VectorXd biases = asEigen<Matrix>(b);
    mLayers[layer].init(
      weights, biases, layerType
    );
  }

  void clear() {
    for (auto &&l : mLayers)
      l.init();
    mInitialized = true;
    mTrained = false;
  }

  double loss(ArrayXXd pred, ArrayXXd out) {
    assert(pred.rows() == out.rows());
    return (pred - out).square().sum() / out.rows();
  }

  void process(RealMatrixView in, RealMatrixView out, index startLayer, index endLayer) {
    using namespace _impl;
    using namespace Eigen;
    ArrayXXd input = asEigen<Eigen::Array>(in);
    ArrayXXd output = ArrayXXd::Zero(out.rows(), out.cols());
    forward(input, output, startLayer, endLayer);
    out = asFluid(output);
  }

  void processFrame(RealVectorView in, RealVectorView out, index startLayer, index endLayer) {
    using namespace _impl;
    using namespace Eigen;
    ArrayXd tmpIn = asEigen<Eigen::Array>(in);
    ArrayXXd input(1, tmpIn.size());
    input.row(0) = tmpIn;
    ArrayXXd output = ArrayXXd::Zero(1, out.size());
    forward(input, output, startLayer, endLayer);
    ArrayXd tmpOut = output.row(0);
    out = asFluid(tmpOut);
  }

  void forward(Eigen::Ref<ArrayXXd> in, Eigen::Ref<ArrayXXd> out) {
    forward(in, out, 0, mLayers.size() - 1);
  }

  void forward(Eigen::Ref<ArrayXXd> in, Eigen::Ref<ArrayXXd> out, index startLayer, index endLayer) {
    if(startLayer >= mLayers.size() || endLayer >= mLayers.size()) return;
    if(startLayer < 0 || endLayer <= 0) return;
    ArrayXXd input = in;
    ArrayXXd output;
    index nRows = input.rows();
    for (index i = 0; i <= endLayer; i++) {
      auto &&l = mLayers[i];
      output = ArrayXXd::Zero(input.rows(), l.outputSize());
      l.forward(input, output);
      input = output;
    }
    out = output;
  }

  void backward(Eigen::Ref<ArrayXXd> out) {
    index nRows = out.rows();
    ArrayXXd chain =
        ArrayXXd::Zero(nRows, mLayers[mLayers.size() - 1].inputSize());
    mLayers[mLayers.size() - 1].backward(out, chain);
    for (index i = mLayers.size() - 2; i >= 0; i--) {
      ArrayXXd tmp = ArrayXXd::Zero(nRows, mLayers[i].inputSize());
      mLayers[i].backward(chain, tmp);
      chain = tmp;
    }
  }

  void update(double learningRate, double momentum) {
    for (auto &&l : mLayers)
      l.update(learningRate, momentum);
  }

  index size() const { return mLayers.size(); }
  bool trained() const { return mTrained; }
  void setTrained(bool val) { mTrained = val; }
  index initialized() const { return mInitialized; }
  index outputSize(index layer) const {
    return (layer >= mLayers.size() || layer < 0)?
      0:mLayers[layer].outputSize();
  }

  index inputSize(index layer) const {
    return (layer >= mLayers.size() || layer < 0)?
      0:mLayers[layer].inputSize();
  }

  index dims() const {
    return mLayers.size() == 0 ? 0 : mLayers[0].inputSize();
  }

  std::vector<NNLayer> mLayers;
  bool mInitialized{false};
  bool mTrained{false};
};
} // namespace algorithm
} // namespace fluid
