#ifndef __MEM_COH_INTERFACE_HH__
#define __MEM_COH_INTERFACE_HH__

#include <cstdint>
#include <functional>
#include <string>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/port.hh"
#include "params/CohInterface.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace memory
{

class CohInterface : public ClockedObject
{
  public:
    using Params = CohInterfaceParams;
    using CompletionCallback =
        std::function<void(uint64_t request_id, Addr line_addr, bool success)>;

    explicit CohInterface(const Params &p);

    void init() override;
    void startup() override;
    DrainState drain() override;
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    bool issueInvalidate(Addr addr, uint64_t request_id=0);
    bool issueInvalidate(Addr addr, uint64_t request_id,
                         CompletionCallback done);
    Addr lineAddress(Addr addr) const;
    Addr backendLineAddress(Addr addr) const;
    const std::string &protocolName() const;
    unsigned lineSize() const;
    bool requiresCompletion() const;
    bool frontEndOnly() const;

  private:
    class RubyRequestPort : public RequestPort
    {
      public:
        RubyRequestPort(const std::string &name, CohInterface &owner);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;

      private:
        CohInterface &owner;
    };

    PacketPtr buildInvalidatePacket(Addr line_addr);
    void issueStartupInvalidate();
    void completeInvalidate(PacketPtr pkt);
    void retryInvalidate();
    bool trySendPending();
    void releasePacket(PacketPtr pkt);

    RubyRequestPort rubyPort;

    System *system;
    RequestorID requestorId;
    std::string protocol;
    Addr rubyAddrOffset;
    unsigned lineSizeBytes;
    unsigned maxOutstandingPerLine;
    bool requireCompletion;
    bool frontEndOnlyMode;
    bool testInvalidateOnStartup;
    Addr testInvalidateAddr;
    unsigned testInvalidateDelayCycles;

    EventFunctionWrapper startupInvalidateEvent;
    PacketPtr pendingPkt;
    bool waitingForRetry;
    bool outstanding;
    uint64_t nextRequestId;
    uint64_t outstandingRequestId;
    Addr outstandingLineAddr;
    CompletionCallback completionCallback;
};

} // namespace memory
} // namespace gem5

#endif // __MEM_COH_INTERFACE_HH__
