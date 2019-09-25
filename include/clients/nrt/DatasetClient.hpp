#pragma once
#include "DatasetErrorStrings.hpp"
#include "FluidSharedInstanceAdaptor.hpp"
#include "clients/common/SharedClientUtils.hpp"
#include "data/FluidDataset.hpp"
#include <clients/common/FluidBaseClient.hpp>
#include <clients/common/MessageSet.hpp>
#include <clients/common/OfflineClient.hpp>
#include <clients/common/ParameterSet.hpp>
#include <clients/common/ParameterTypes.hpp>
#include <clients/common/Result.hpp>
#include <clients/nrt/FluidNRTClientWrapper.hpp>
#include <data/FluidTensor.hpp>
#include <data/TensorTypes.hpp>
#include <data/FluidFile.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace fluid {
namespace client {

class DatasetClient : public FluidBaseClient, OfflineIn, OfflineOut {
  enum { kName, kNDims };

public:
  using string = std::string;
  using BufferPtr = std::shared_ptr<BufferAdaptor>;
  using LabelledDataset = FluidDataset<string, double, string, 1>;

  template <typename T> Result process(FluidContext &) { return {}; }

  FLUID_DECLARE_PARAMS(StringParam<Fixed<true>>("name", "Dataset"),
                       LongParam<Fixed<true>>("nDims", "Dimension size", 1,
                                              Min(1)));

  DatasetClient(ParamSetViewType &p) : mParams(p), mDataset(get<kNDims>()) {
    mDims = get<kNDims>();
  }

  MessageResult<void> addPoint(string id, BufferPtr data) {
    if (!data)
      return mNoBufferError;
    BufferAdaptor::Access buf(data.get());
    if (buf.numFrames() != mDims)
      return mWrongSizeError;
    FluidTensor<double, 1> point(mDims);
    point = buf.samps(0, mDims, 0);
    return mDataset.add(id, point, id) ? mOKResult : mDuplicateError;
  }

  // TODO: refactor with addPoint
  MessageResult<void> addPointLabel(string id, BufferPtr data, string label) {
    if (!data)
      return mNoBufferError;
    BufferAdaptor::Access buf(data.get());
    if (buf.numFrames() != mDims)
      return mWrongSizeError;
    FluidTensor<double, 1> point(mDims);
    point = buf.samps(0, mDims, 0);
    return mDataset.add(id, point, label) ? mOKResult : mDuplicateError;
  }

  MessageResult<void> getPoint(string label, BufferPtr data) const {
    if (!data)
      return mNoBufferError;
    BufferAdaptor::Access buf(data.get());
    if (buf.numFrames() < mDims)
      return mWrongSizeError;
    FluidTensor<double, 1> point(mDims);
    point = buf.samps(0, mDims, 0);
    bool result = mDataset.get(label, point);
    mDataset.print();
    if (result) {
      buf.samps(0, mDims, 0) = point;
      return {Result::Status::kOk};
    } else {
      return mNotFoundError;
    }
  }

  MessageResult<void> updatePoint(string label, BufferPtr data) {
    if (!data)
      return mNoBufferError;
    BufferAdaptor::Access buf(data.get());
    if (buf.numFrames() < mDims)
      return mWrongSizeError;
    FluidTensor<double, 1> point(mDims);
    point = buf.samps(0, mDims, 0);
    return mDataset.update(label, point) ? mOKResult : mNotFoundError;
  }

  MessageResult<void> deletePoint(string label) {
    return mDataset.remove(label) ? mOKResult : mNotFoundError;
  }

  MessageResult<int> size() { return mDataset.size(); }

  MessageResult<void> clear() {
    mDataset = LabelledDataset(get<kNDims>());
    return mOKResult;
  }

  MessageResult<void> write(string fileName) {
    auto file = FluidFile(fileName, "w");
    if(!file.valid()){return {Result::Status::kError, file.error()};}
    file.add("targets", mDataset.getTargets());
    file.add("ids", mDataset.getIds());
    file.add("data", mDataset.getData());
    file.add("cols", mDataset.pointSize());
    file.add("rows", mDataset.size());
    return file.write()? mOKResult:mWriteError;
  }

 MessageResult<void> read(string fileName) {
   auto file = FluidFile(fileName, "r");
   if(!file.valid()){return {Result::Status::kError, file.error()};}
   if(!file.read()){return {Result::Status::kError, ReadError};}
   if(!file.checkKeys({"targets","data","ids","rows","cols"})){
     return {Result::Status::kError, file.error()};
   }
   size_t cols, rows;
   file.get("cols", cols);
   file.get("rows", rows);
   FluidTensor<string, 1> ids(rows);
   FluidTensor<string, 1> targets(rows);
   FluidTensor<double, 2> data(rows,cols);
   file.get("ids", ids, rows);
   file.get("data", data, rows, cols);
   file.get("targets", targets, rows);
   mDataset = LabelledDataset(ids, data, targets);
   return mOKResult;
 }

  FLUID_DECLARE_MESSAGES(
      makeMessage("addPoint", &DatasetClient::addPoint),
      makeMessage("addPointLabel", &DatasetClient::addPointLabel),
      makeMessage("getPoint", &DatasetClient::getPoint),
      makeMessage("updatePoint", &DatasetClient::updatePoint),
      makeMessage("deletePoint", &DatasetClient::deletePoint),
      makeMessage("size", &DatasetClient::size),
      makeMessage("clear", &DatasetClient::clear),
      makeMessage("write", &DatasetClient::write),
      makeMessage("read", &DatasetClient::read)

  );

  const LabelledDataset getDataset() const { return mDataset; }

private:
  using result = MessageResult<void>;
  result mNoBufferError{Result::Status::kError, NoBufferError};
  result mWriteError{Result::Status::kError, WriteError};
  result mNotFoundError{Result::Status::kError, PointNotFoundError};
  result mWrongSizeError{Result::Status::kError, WrongPointSizeError};
  result mDuplicateError{Result::Status::kError,DuplicateError};
  result mOKResult{Result::Status::kOk};

  mutable LabelledDataset mDataset;
  size_t mDims;
};
using DatasetClientRef = SharedClientRef<DatasetClient>;
using NRTThreadedDatasetClient =
    NRTThreadingAdaptor<typename DatasetClientRef::SharedType>;

} // namespace client
} // namespace fluid
