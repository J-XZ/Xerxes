#ifndef __MEM_XERXES_WRAPPER_HH__
#define __MEM_XERXES_WRAPPER_HH__

#include <iosfwd>
#include <string>

#include "mem/mem_ctrl.hh"
#include "params/XerxesWrapper.hh"

namespace xerxes
{

class Simulation;
class DuplexBus;
class Snoop;
class UpInterface;
class DownInterface;

} // namespace xerxes

namespace gem5
{

namespace memory
{

class CohInterface;

class XerxesWrapper : public MemCtrl
{
    friend class xerxes::UpInterface;
    friend class xerxes::DownInterface;

  protected:
    class XerxesSnoopPort : public MemoryPort
    {
      public:
        XerxesSnoopPort(const std::string& name, MemCtrl& wrapper);

      protected:
        bool recvTimingSnoopResp(PacketPtr pkt) override;
    };

    xerxes::Simulation* xerxesSim;
    xerxes::DuplexBus* xerxesBus;
    xerxes::Snoop* xerxesSnoop;
    xerxes::UpInterface* upIf;
    xerxes::DownInterface* downIf;

    bool useSnoop;
    int snoopLineNum;
    int snoopAssoc;
    unsigned snoopMaxBurstInv;
    std::string snoopEviction;
    bool busIsFull;
    Tick busHalfRevTime;
    Tick busDelayPerT;
    unsigned busWidthBits;
    Tick busFramingTime;
    unsigned busFrameSize;
    Tick cxlLatency;
    std::string outputDir;
    std::string logName;
    std::string logLevel;
    std::ofstream* logger;
    CohInterface* cohInterface;

    XerxesSnoopPort snoopPort;

  public:
    XerxesWrapper(const XerxesWrapperParams& p);
    ~XerxesWrapper() override;

    void init() override;
    Port& getPort(const std::string& if_name,
                  PortID idx=InvalidPortID) override;
    Tick doBurstAccess(MemPacket* mem_pkt, MemInterface* mem_intr) override;
    void accessAndRespond(PacketPtr pkt, Tick static_latency,
                          MemInterface* mem_intr) override;

    void actual_accessAndRespond(PacketPtr pkt, Tick static_latency,
                                 MemInterface* mem_intr);
};

} // namespace memory
} // namespace gem5

#endif // __MEM_XERXES_WRAPPER_HH__
