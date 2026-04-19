#include "mem/xerxes_wrapper.hh"

#include "../../ext/xerxes/bus.hh"
#include "../../ext/xerxes/device.hh"
#include "../../ext/xerxes/simulation.hh"
#include "../../ext/xerxes/snoop.hh"
#include "../../ext/xerxes/system.hh"
#include "../../ext/xerxes/topology.hh"
#include "../../ext/xerxes/utils.hh"
#include "../../ext/xerxes/xerxes_basic.cc"

#include <fstream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/logging.hh"
#include "mem/coh_interface.hh"
#include "sim/cur_tick.hh"
#include "sim/eventq.hh"

namespace
{

std::unordered_map<const xerxes::Device *,
                   std::vector<gem5::EventFunctionWrapper *>> scheduledEvents;

void
cleanupFinishedEvents(const xerxes::Device* dev)
{
    auto it = scheduledEvents.find(dev);
    if (it == scheduledEvents.end()) {
        return;
    }

    auto& events = it->second;
    for (auto event_it = events.begin(); event_it != events.end();) {
        auto* event = *event_it;
        if (!event->scheduled() && event->when() <= gem5::curTick()) {
            delete event;
            event_it = events.erase(event_it);
        } else {
            ++event_it;
        }
    }
}

void
registerXerxesDev(xerxes::Simulation* sim, xerxes::Device* dev)
{
    sim->system()->add_dev(dev);
}

} // anonymous namespace

namespace xerxes
{

void
Device::sched_transit(Tick tick)
{
    cleanupFinishedEvents(this);

    auto* event = new gem5::EventFunctionWrapper(
        this->get_transit_func(), name() + "::transit()");
    tick = std::max<Tick>(tick, gem5::curTick() + 1);
    gem5::curEventQueue()->schedule(event, tick);
    scheduledEvents[this].push_back(event);
}

class UpInterface : public Device
{
  private:
    struct InterfacePacket
    {
        gem5::PacketPtr pkt;
        gem5::memory::MemInterface* memIntr;
        gem5::Tick staticLatency;
    };

    std::unordered_map<PktID, InterfacePacket> pendingPkts;
    gem5::memory::XerxesWrapper* memCtrl;

  public:
    UpInterface(Simulation* sim, gem5::memory::XerxesWrapper* mem_ctrl,
                std::string name = "UpInterface")
        : Device(sim, std::move(name)), memCtrl(mem_ctrl)
    {
    }

    void
    request(gem5::PacketPtr pkt, gem5::Tick static_latency,
            gem5::memory::MemInterface* mem_intr, TopoID dst_dev)
    {
        auto xerxesPkt = PktBuilder()
                             .type(pkt->isRead() ? PacketType::RD
                                                 : PacketType::WT)
                             .addr(pkt->getAddr())
                             .sent(gem5::curTick())
                             .arrive(gem5::curTick() + static_latency +
                                     memCtrl->cxlLatency)
                             .burst(pkt->getSize())
                             .payload(pkt->isRead() ? 0 : pkt->getSize())
                             .src(self)
                             .dst(dst_dev)
                             .build();
        pendingPkts[xerxesPkt.id] = {pkt, mem_intr, static_latency};
        send_pkt(xerxesPkt);
    }

    void
    transit() override
    {
        auto pkt = receive_pkt();
        if (pkt.dst != self) {
            send_pkt(pkt);
            return;
        }

        if (pkt.type == PacketType::INV) {
            if (pkt.is_rsp) {
                warn("%s received an INV response packet %lld unexpectedly.",
                     name().c_str(), static_cast<long long>(pkt.id));
                return;
            }

            fatal_if(memCtrl->cohInterface == nullptr,
                     "XerxesWrapper received Xerxes INV packet %lld but "
                     "coh_interface is not connected.",
                     static_cast<long long>(pkt.id));
            fatal_if(pkt.burst != 1,
                     "XerxesWrapper phase-1 invalidation supports only one "
                     "line per INV packet, got burst %llu for packet %lld.",
                     static_cast<unsigned long long>(pkt.burst),
                     static_cast<long long>(pkt.id));

            const auto wait_start = gem5::curTick();
            inform("XerxesWrapper %s forwarding Xerxes INV packet %lld for "
                   "addr %#llx into CohInterface.",
                   memCtrl->name().c_str(),
                   static_cast<long long>(pkt.id),
                   static_cast<unsigned long long>(pkt.addr));
            const auto issued = memCtrl->cohInterface->issueInvalidate(
                static_cast<gem5::Addr>(pkt.addr),
                static_cast<uint64_t>(pkt.id),
                [this, pkt, wait_start](
                    uint64_t request_id, gem5::Addr line_addr,
                    bool success) mutable {
                    fatal_if(!success,
                             "CohInterface failed Xerxes INV packet %lld.",
                             static_cast<long long>(pkt.id));
                    fatal_if(request_id != static_cast<uint64_t>(pkt.id),
                             "CohInterface completed request %llu for "
                             "Xerxes INV packet %lld.",
                             request_id, static_cast<long long>(pkt.id));
                    fatal_if(
                        line_addr != memCtrl->cohInterface->backendLineAddress(
                                         static_cast<gem5::Addr>(pkt.addr)),
                        "CohInterface completed line %#llx for Xerxes INV "
                        "packet %lld at address %#llx.",
                        static_cast<unsigned long long>(line_addr),
                        static_cast<long long>(pkt.id),
                        static_cast<unsigned long long>(pkt.addr));

                    const auto done_tick = std::max<gem5::Tick>(
                        gem5::curTick(), static_cast<gem5::Tick>(pkt.arrive));
                    pkt.delta_stat(
                        HOST_INV_DELAY,
                        static_cast<double>(done_tick - wait_start));
                    inform("XerxesWrapper %s completed Xerxes INV packet %lld "
                           "for line %#llx after Ruby completion.",
                           memCtrl->name().c_str(),
                           static_cast<long long>(pkt.id),
                           static_cast<unsigned long long>(line_addr));
                    pkt.arrive = done_tick;
                    std::swap(pkt.src, pkt.dst);
                    pkt.is_rsp = true;
                    pkt.payload = memCtrl->cohInterface->lineSize() *
                                  pkt.burst;
                    send_pkt(pkt);
                });
            fatal_if(!issued,
                     "CohInterface refused Xerxes INV packet %lld.",
                     static_cast<long long>(pkt.id));
            return;
        }

        if (!pkt.is_rsp) {
            warn("%s received a non-response packet %lld unexpectedly.",
                 name().c_str(), static_cast<long long>(pkt.id));
            return;
        }

        auto pending_it = pendingPkts.find(pkt.id);
        if (pending_it == pendingPkts.end()) {
            warn("%s received an unknown packet id %lld.",
                 name().c_str(), static_cast<long long>(pkt.id));
            return;
        }

        auto pending = pending_it->second;
        pendingPkts.erase(pending_it);

        pkt.log_stat();
        const auto latency = pkt.arrive - pkt.sent;
        memCtrl->actual_accessAndRespond(pending.pkt, latency, pending.memIntr);
    }
};

class DownInterface : public Device
{
  private:
    gem5::memory::XerxesWrapper* memCtrl;

  public:
    DownInterface(Simulation* sim, gem5::memory::XerxesWrapper* mem_ctrl,
                  std::string name = "DownInterface")
        : Device(sim, std::move(name)), memCtrl(mem_ctrl)
    {
    }

    void
    transit() override
    {
        auto pkt = receive_pkt();
        std::swap(pkt.src, pkt.dst);
        pkt.is_rsp = true;
        pkt.payload = pkt.is_read() ? pkt.burst : 0;
        if (pkt.arrive < gem5::curTick()) {
            pkt.arrive = gem5::curTick() + 1;
        }
        send_pkt(pkt);
    }
};

} // namespace xerxes

namespace gem5
{

namespace memory
{

XerxesWrapper::XerxesWrapper(const XerxesWrapperParams& p)
    : MemCtrl(p),
      xerxesSim(nullptr),
      xerxesBus(nullptr),
      xerxesSnoop(nullptr),
      upIf(nullptr),
      downIf(nullptr),
      useSnoop(p.use_snoop),
      snoopLineNum(p.snoop_line_num),
      snoopAssoc(p.snoop_assoc),
      snoopMaxBurstInv(p.snoop_max_burst_inv),
      snoopEviction(p.snoop_eviction),
      busIsFull(p.bus_is_full),
      busHalfRevTime(p.bus_half_rev_time),
      busDelayPerT(p.bus_delay_per_t),
      busWidthBits(p.bus_width),
      busFramingTime(p.bus_framing_time),
      busFrameSize(p.bus_frame_size),
      cxlLatency(p.cxlLatency),
      outputDir(p.output_dir),
      logName(p.log_name),
      logLevel(p.log_level),
      logger(nullptr),
      cohInterface(p.coh_interface),
      snoopPort(name() + ".snoopPort", *this)
{
}

XerxesWrapper::~XerxesWrapper()
{
    xerxes::Packet::pkt_logger(true, [](const xerxes::Packet&) {});

    delete upIf;
    delete downIf;
    delete xerxesSnoop;
    delete xerxesBus;
    delete xerxesSim;

    if (logger != nullptr) {
        logger->close();
        delete logger;
    }
}

void
XerxesWrapper::init()
{
    MemCtrl::init();

    if (logger == nullptr) {
        std::filesystem::path logPath(logName);
        if (logPath.is_relative()) {
            logPath = std::filesystem::path(outputDir) / logPath;
        }
        if (logPath.has_parent_path()) {
            std::filesystem::create_directories(logPath.parent_path());
        }
        logger = new std::ofstream(logPath);
        fatal_if(!logger->is_open(),
                 "Failed to open Xerxes log file %s.",
                 logPath.string().c_str());
        xerxes::XerxesLogger::set(
            *logger, xerxes::str_to_log_level(logLevel));
        *logger << "srcname,dstname,type,addr,sent,arrive,lat,payload"
                << std::endl;
    }

    if (xerxesSim != nullptr) {
        return;
    }

    xerxesSim = new xerxes::Simulation();

    xerxes::DuplexBusConfig busConfig;
    busConfig.is_full = busIsFull;
    busConfig.half_rev_time = busHalfRevTime;
    busConfig.delay_per_T = busDelayPerT;
    busConfig.width = busWidthBits;
    busConfig.framing_time = busFramingTime;
    busConfig.frame_size = busFrameSize;
    xerxesBus = new xerxes::DuplexBus(xerxesSim, busConfig, "bus");

    upIf = new xerxes::UpInterface(xerxesSim, this);
    downIf = new xerxes::DownInterface(xerxesSim, this);
    registerXerxesDev(xerxesSim, upIf);
    registerXerxesDev(xerxesSim, downIf);
    registerXerxesDev(xerxesSim, xerxesBus);

    xerxesSim->topology()->add_edge(upIf->id(), xerxesBus->id());

    if (useSnoop) {
        warn_if(cohInterface == nullptr,
                "XerxesWrapper use_snoop is enabled without coh_interface; "
                "the first Xerxes INV packet will fatal.");

        xerxes::SnoopConfig snoopConfig;
        snoopConfig.line_num = snoopLineNum;
        snoopConfig.assoc = snoopAssoc;
        snoopConfig.max_burst_inv = snoopMaxBurstInv;
        snoopConfig.eviction = snoopEviction;

        for (const auto& range : getAddrRanges()) {
            if (range.size() > 0) {
                snoopConfig.ranges.push_back(
                    {range.start(), range.end() - 1});
            }
        }

        fatal_if(snoopConfig.ranges.empty(),
                 "Xerxes snoop enabled but no address ranges were found.");

        xerxesSnoop = new xerxes::Snoop(xerxesSim, snoopConfig, "snoop");
        registerXerxesDev(xerxesSim, xerxesSnoop);
        xerxesSim->topology()
            ->add_edge(xerxesBus->id(), xerxesSnoop->id())
            ->add_edge(xerxesSnoop->id(), downIf->id());
    } else {
        xerxesSim->topology()->add_edge(xerxesBus->id(), downIf->id());
    }

    xerxesSim->topology()->build_route();

    xerxes::Packet::pkt_logger(true, [this](const xerxes::Packet& pkt) {
        auto* srcDev = xerxesSim->system()->find_dev(pkt.src);
        auto* dstDev = xerxesSim->system()->find_dev(pkt.dst);
        const auto srcName = srcDev ? srcDev->name() : std::string("unknown");
        const auto dstName = dstDev ? dstDev->name() : std::string("unknown");
        *logger << srcName << "," << dstName << ","
                << xerxes::TypeName::of(pkt.type) << "," << std::hex
                << pkt.addr << std::dec << "," << pkt.sent << ","
                << pkt.arrive << "," << pkt.arrive - pkt.sent << ","
                << pkt.payload << std::endl;
    });

    inform("XerxesWrapper %s initialized with bus{is_full=%d, "
           "half_rev_time=%llu, delay_per_T=%llu, width=%u, "
           "framing_time=%llu, frame_size=%u} snoop{enabled=%d, line_num=%d, "
           "assoc=%d, max_burst_inv=%u, eviction=%s} log{name=%s, level=%s}.",
           name().c_str(), busIsFull,
           static_cast<unsigned long long>(busHalfRevTime),
           static_cast<unsigned long long>(busDelayPerT), busWidthBits,
           static_cast<unsigned long long>(busFramingTime), busFrameSize,
           useSnoop, snoopLineNum, snoopAssoc, snoopMaxBurstInv,
           snoopEviction.c_str(), logName.c_str(), logLevel.c_str());
}

Port&
XerxesWrapper::getPort(const std::string& if_name, PortID idx)
{
    if (if_name == "snoopPort") {
        return snoopPort;
    }
    return MemCtrl::getPort(if_name, idx);
}

Tick
XerxesWrapper::doBurstAccess(MemPacket* mem_pkt, MemInterface* mem_intr)
{
    return MemCtrl::doBurstAccess(mem_pkt, mem_intr);
}

void
XerxesWrapper::accessAndRespond(PacketPtr pkt, Tick static_latency,
                                MemInterface* mem_intr)
{
    panic_if(upIf == nullptr || downIf == nullptr,
             "XerxesWrapper is not initialized before accessAndRespond().");
    upIf->request(pkt, static_latency, mem_intr, downIf->id());
}

void
XerxesWrapper::actual_accessAndRespond(PacketPtr pkt, Tick static_latency,
                                       MemInterface* mem_intr)
{
    MemCtrl::accessAndRespond(pkt, static_latency + cxlLatency, mem_intr);
}

XerxesWrapper::XerxesSnoopPort::XerxesSnoopPort(
    const std::string& name, MemCtrl& wrapper)
    : MemoryPort(name, wrapper)
{
}

bool
XerxesWrapper::XerxesSnoopPort::recvTimingSnoopResp(PacketPtr pkt)
{
    warn("Ignoring snoop response for XerxesWrapper packet %#llx.",
         static_cast<unsigned long long>(pkt->getAddr()));
    return true;
}

} // namespace memory
} // namespace gem5
