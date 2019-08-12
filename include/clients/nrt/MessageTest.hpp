#pragma once

#include "../common/AudioClient.hpp"
#include "../common/FluidBaseClient.hpp"
#include "../common/ParameterConstraints.hpp"
#include "../common/ParameterSet.hpp"
#include "../common/ParameterTypes.hpp"
#include "../nrt/FluidNRTClientWrapper.hpp"
#include "../../algorithms/public/OnsetSegmentation.hpp"
#include "../../data/TensorTypes.hpp"

#include "../common/MessageSet.hpp"

#include <string>
#include <tuple>

namespace fluid {
namespace client {

class MessageTest : public FluidBaseClient, public OfflineIn, public OfflineOut
{
public:

  template<typename T>
  MessageTest(T&){}

  template <typename T>
  Result process(FluidContext&) { return {}; }
  
  MessageResult<FluidTensor<std::string,1>> doStrings()
  {
    return FluidTensor<std::string,1>{"Hello","I","Love","you"};
  }

  MessageResult<FluidTensor<double,1>> doNumbers()
  {
    auto result = FluidTensor<double,1>(100);
    std::iota(result.begin(),result.end(), 0.0);
    return result;
  }
  
  MessageResult<std::string> doOneString()
  {
    return std::string("Hello I Love you");
  }

  MessageResult<intptr_t> doOneNumber()
  {
    return 12345;
  }

  MessageResult<intptr_t> doBuffer(std::shared_ptr<BufferAdaptor> b)
  {
    if(!b) return  {Result::Status::kError, "Null passed"};
    auto buf = BufferAdaptor::Access(b.get());
    if(!buf.exists()) return {Result::Status::kError, "No buffer found"};
    return buf.numFrames();
  }
 
  MessageResult<void> doTakeString(std::string s, double a, double b, double c)
  {
    std::cout << "Received " << s << ' ' << a << ' ' << b << ' ' << c << '\n';
    return {};
  }
  
  FLUID_DECLARE_MESSAGES(
    makeMessage("testReturnStrings", &MessageTest::doStrings),
    makeMessage("testReturnNumbers", &MessageTest::doNumbers),
    makeMessage("testReturnOneString", &MessageTest::doOneString),
    makeMessage("testReturnOneNumber", &MessageTest::doOneNumber),
    makeMessage("testAccessBuffer", &MessageTest::doBuffer),
    makeMessage("testPassString", &MessageTest::doTakeString)
  );
};

using NRTThreadingMessageTest = NRTThreadingAdaptor<ClientWrapper<MessageTest>>;

} // namespace client
} // namespace fluid
