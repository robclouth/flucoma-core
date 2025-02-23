/*
Part of the Fluid Corpus Manipulation Project (http://www.flucoma.org/)
Copyright 2017-2019 University of Huddersfield.
Licensed under the BSD-3 License.
See license.md file in the project root for full license information.
This project has received funding from the European Research Council (ERC)
under the European Union’s Horizon 2020 research and innovation programme
(grant agreement No 725899).
*/

#pragma once
#include "DataSetClient.hpp"
#include "NRTClient.hpp"
#include "../../algorithms/public/PCA.hpp"

namespace fluid {
namespace client {
namespace pca {

enum { kNumDimensions, kInputBuffer, kOutputBuffer };

constexpr auto PCAParams = defineParameters(
    LongParam("numDimensions", "Target Number of Dimensions", 2, Min(1)),
    BufferParam("inputPointBuffer", "Input Point Buffer"),
    BufferParam("predictionBuffer", "Prediction Buffer"));

class PCAClient : public FluidBaseClient,
                  AudioIn,
                  ControlOut,
                  ModelObject,
                  public DataClient<algorithm::PCA>
{
public:
  using string = std::string;
  using BufferPtr = std::shared_ptr<BufferAdaptor>;
  using StringVector = FluidTensor<string, 1>;

  using ParamDescType = decltype(PCAParams);

  using ParamSetViewType = ParameterSetView<ParamDescType>;
  std::reference_wrapper<ParamSetViewType> mParams;

  void setParams(ParamSetViewType& p) { mParams = p; }

  template <size_t N>
  auto& get() const
  {
    return mParams.get().template get<N>();
  }

  static constexpr auto& getParameterDescriptors() { return PCAParams; }

  PCAClient(ParamSetViewType& p) : mParams(p)
  {
    audioChannelsIn(1);
    controlChannelsOut(1);
  }

  template <typename T>
  void process(std::vector<FluidTensorView<T, 1>>& input,
               std::vector<FluidTensorView<T, 1>>& output, FluidContext&)
  {
    if (!mAlgorithm.initialized()) return;
    index k = get<kNumDimensions>();
    if (k <= 0 || k > mAlgorithm.dims()) return;
    InOutBuffersCheck bufCheck(mAlgorithm.dims());
    if (!bufCheck.checkInputs(get<kInputBuffer>().get(),
                              get<kOutputBuffer>().get()))
      return;
    auto outBuf = BufferAdaptor::Access(get<kOutputBuffer>().get());
    if (outBuf.samps(0).size() < k) return;
    RealVector src(mAlgorithm.dims());
    RealVector dest(k);
    src = BufferAdaptor::ReadAccess(get<kInputBuffer>().get())
              .samps(0, mAlgorithm.dims(), 0);
    mTrigger.process(input, output, [&]() {
      mAlgorithm.processFrame(src, dest, k);
      outBuf.samps(0, k, 0) = dest;
    });
  }

  MessageResult<void> fit(DataSetClientRef datasetClient)
  {
    auto datasetClientPtr = datasetClient.get().lock();
    if (!datasetClientPtr) return Error(NoDataSet);
    auto dataSet = datasetClientPtr->getDataSet();
    if (dataSet.size() == 0) return Error(EmptyDataSet);
    mAlgorithm.init(dataSet.getData());
    return OK();
  }

  MessageResult<double> fitTransform(DataSetClientRef sourceClient,
                                     DataSetClientRef destClient)
  {
    auto fitResult = fit(sourceClient);
    if (!fitResult.ok()) return Error<double>(fitResult.message());
    auto result = transform(sourceClient, destClient);
    auto destPtr = destClient.get().lock();
    return result;
  }

  MessageResult<double> transform(DataSetClientRef sourceClient,
                                  DataSetClientRef destClient) const
  {
    using namespace std;
    index k = get<kNumDimensions>();
    if (k <= 0) return Error<double>(SmallDim);
    if (k > mAlgorithm.dims()) return Error<double>(LargeDim);
    auto   srcPtr = sourceClient.get().lock();
    auto   destPtr = destClient.get().lock();
    double result = 0;
    if (srcPtr && destPtr)
    {
      auto srcDataSet = srcPtr->getDataSet();
      if (srcDataSet.size() == 0) return Error<double>(EmptyDataSet);
      if (!mAlgorithm.initialized()) return Error<double>(NoDataFitted);
      if (srcDataSet.pointSize() != mAlgorithm.dims())
        return Error<double>(WrongPointSize);
      if (srcDataSet.pointSize() < k) return Error<double>(LargeDim);

      StringVector ids{srcDataSet.getIds()};
      RealMatrix   output(srcDataSet.size(), k);
      result = mAlgorithm.process(srcDataSet.getData(), output, k);
      FluidDataSet<string, double, 1> result(ids, output);
      destPtr->setDataSet(result);
    }
    else
    {
      return Error<double>(NoDataSet);
    }
    return result;
  }

  MessageResult<void> transformPoint(BufferPtr in, BufferPtr out) const
  {
    index k = get<kNumDimensions>();
    if (k <= 0) return Error(SmallDim);
    if (k > mAlgorithm.dims()) return Error(LargeDim);
    if (!mAlgorithm.initialized()) return Error(NoDataFitted);
    InOutBuffersCheck bufCheck(mAlgorithm.dims());
    if (!bufCheck.checkInputs(in.get(), out.get()))
      return Error(bufCheck.error());
    BufferAdaptor::Access outBuf(out.get());
    Result resizeResult = outBuf.resize(k, 1, outBuf.sampleRate());
    if (!resizeResult.ok()) return Error(BufferAlloc);
    FluidTensor<double, 1> src(mAlgorithm.dims());
    FluidTensor<double, 1> dest(k);
    src = BufferAdaptor::ReadAccess(in.get()).samps(0, mAlgorithm.dims(), 0);
    mAlgorithm.processFrame(src, dest, k);
    outBuf.samps(0, k, 0) = dest;
    return OK();
  }

  index latency() { return 0; }

  static auto getMessageDescriptors()
  {
    return defineMessages(
        makeMessage("fit", &PCAClient::fit),
        makeMessage("transform", &PCAClient::transform),
        makeMessage("fitTransform", &PCAClient::fitTransform),
        makeMessage("transformPoint", &PCAClient::transformPoint),
        makeMessage("cols", &PCAClient::dims),
        makeMessage("size", &PCAClient::size),
        makeMessage("clear", &PCAClient::clear),
        makeMessage("load", &PCAClient::load),
        makeMessage("dump", &PCAClient::dump),
        makeMessage("read", &PCAClient::read),
        makeMessage("write", &PCAClient::write));
  }

private:
  FluidInputTrigger mTrigger;
};
} // namespace pca

using RTPCAClient = ClientWrapper<pca::PCAClient>;
} // namespace client
} // namespace fluid
