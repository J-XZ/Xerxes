#include "mem/coh_interface.hh"

#include <utility>

#include "base/logging.hh"
#include "mem/request.hh"
#include "sim/system.hh"

namespace gem5
{

namespace memory
{

namespace
{

bool
isPowerOfTwo(unsigned value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

} // anonymous namespace

CohInterface::RubyRequestPort::RubyRequestPort(
        const std::string &name, CohInterface &owner)
    : RequestPort(name), owner(owner)
{
}

bool
CohInterface::RubyRequestPort::recvTimingResp(PacketPtr pkt)
{
    owner.completeInvalidate(pkt);
    return true;
}

void
CohInterface::RubyRequestPort::recvReqRetry()
{
    owner.retryInvalidate();
}

CohInterface::CohInterface(const Params &p)
    : ClockedObject(p),
      rubyPort(name() + ".ruby_port", *this),
      system(p.system),
      requestorId(Request::invldRequestorId),
      protocol(p.protocol),
      rubyAddrOffset(p.ruby_addr_offset),
      lineSizeBytes(p.line_size),
      maxOutstandingPerLine(p.max_outstanding_per_line),
      requireCompletion(p.require_completion),
      frontEndOnlyMode(p.front_end_only),
      testInvalidateOnStartup(p.test_invalidate_on_startup),
      testInvalidateAddr(p.test_invalidate_addr),
      testInvalidateDelayCycles(p.test_invalidate_delay_cycles),
      startupInvalidateEvent(
          [this] { issueStartupInvalidate(); },
          name() + ".startup_invalidate"),
      pendingPkt(nullptr),
      waitingForRetry(false),
      outstanding(false),
      nextRequestId(1),
      outstandingRequestId(0),
      outstandingLineAddr(0),
      completionCallback()
{
}

void
CohInterface::init()
{
    ClockedObject::init();

    fatal_if(!isPowerOfTwo(lineSizeBytes),
             "CohInterface line_size must be a non-zero power of two.");
    fatal_if(maxOutstandingPerLine != 1,
             "CohInterface phase-1 scaffolding only supports "
             "max_outstanding_per_line == 1.");
    fatal_if(system == nullptr,
             "CohInterface requires a valid parent System.");

    const bool supported_protocol =
        protocol == "MI_example" || protocol == "mi_example" ||
        protocol == "MI_example_Xerxes" ||
        protocol == "mi_example_xerxes" ||
        protocol == "MESI_Two_Level" || protocol == "mesi_two_level" ||
        protocol == "MESI_Two_Level_Xerxes" ||
        protocol == "mesi_two_level_xerxes";
    fatal_if(!supported_protocol,
             "CohInterface only supports MI_example_Xerxes/MI_example or "
             "MESI_Two_Level_Xerxes/MESI_Two_Level in phase-1 scaffolding.");

    warn_if(frontEndOnlyMode,
            "CohInterface %s is instantiated in front-end-only mode for "
            "protocol '%s'; no Ruby invalidation traffic will be issued yet.",
            name().c_str(), protocol.c_str());

    fatal_if(!frontEndOnlyMode && !rubyPort.isConnected(),
             "CohInterface %s requires ruby_port to be connected when "
             "front_end_only is false.", name().c_str());
    fatal_if(testInvalidateOnStartup && frontEndOnlyMode,
             "CohInterface %s cannot issue test invalidation while "
             "front_end_only is true.", name().c_str());

    requestorId = system->getRequestorId(this);
}

void
CohInterface::startup()
{
    ClockedObject::startup();

    if (testInvalidateOnStartup) {
        schedule(startupInvalidateEvent,
                 clockEdge(Cycles(testInvalidateDelayCycles)));
    }
}

DrainState
CohInterface::drain()
{
    return pendingPkt || outstanding ? DrainState::Draining :
        DrainState::Drained;
}

Port &
CohInterface::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "ruby_port") {
        return rubyPort;
    }

    return ClockedObject::getPort(if_name, idx);
}

bool
CohInterface::issueInvalidate(Addr addr, uint64_t request_id)
{
    return issueInvalidate(addr, request_id, CompletionCallback{});
}

bool
CohInterface::issueInvalidate(Addr addr, uint64_t request_id,
                              CompletionCallback done)
{
    if (frontEndOnlyMode) {
        warn("CohInterface %s ignored invalidation for %#llx because "
             "front_end_only is true.",
             name().c_str(), addr);
        return false;
    }

    fatal_if(!rubyPort.isConnected(),
             "CohInterface %s cannot issue invalidation without ruby_port.",
             name().c_str());
    fatal_if(pendingPkt || outstanding,
             "CohInterface phase-1 supports only one outstanding "
             "invalidation.");

    const Addr line_addr = backendLineAddress(addr);
    fatal_if(!system->isMemAddr(line_addr),
             "CohInterface %s cannot invalidate non-memory address %#llx.",
             name().c_str(), line_addr);

    outstandingRequestId = request_id ? request_id : nextRequestId++;
    outstandingLineAddr = line_addr;
    completionCallback = std::move(done);
    pendingPkt = buildInvalidatePacket(line_addr);
    trySendPending();
    return true;
}

PacketPtr
CohInterface::buildInvalidatePacket(Addr line_addr)
{
    auto req = std::make_shared<Request>(line_addr, 1, 0, requestorId);
    auto pkt = new Packet(req, MemCmd::ReadReq);
    pkt->allocate();
    return pkt;
}

void
CohInterface::issueStartupInvalidate()
{
    inform("CohInterface %s issuing startup test invalidation for %#llx.",
           name().c_str(), testInvalidateAddr);
    issueInvalidate(testInvalidateAddr);
}

bool
CohInterface::trySendPending()
{
    assert(pendingPkt != nullptr);

    if (rubyPort.sendTimingReq(pendingPkt)) {
        outstanding = true;
        waitingForRetry = false;
        pendingPkt = nullptr;
        return true;
    }

    waitingForRetry = true;
    return false;
}

void
CohInterface::retryInvalidate()
{
    if (waitingForRetry) {
        trySendPending();
    }
}

void
CohInterface::completeInvalidate(PacketPtr pkt)
{
    fatal_if(!outstanding,
             "CohInterface %s received unexpected Ruby completion.",
             name().c_str());

    inform("CohInterface %s completed invalidation request %llu for line %#llx.",
           name().c_str(), outstandingRequestId, pkt->getAddr());

    const auto request_id = outstandingRequestId;
    const auto line_addr = outstandingLineAddr;
    auto done = std::move(completionCallback);

    releasePacket(pkt);
    outstanding = false;
    outstandingRequestId = 0;
    outstandingLineAddr = 0;

    if (drainState() == DrainState::Draining) {
        signalDrainDone();
    }

    if (done) {
        done(request_id, line_addr, true);
    }
}

void
CohInterface::releasePacket(PacketPtr pkt)
{
    delete pkt;
}

Addr
CohInterface::lineAddress(Addr addr) const
{
    return addr & ~static_cast<Addr>(lineSizeBytes - 1);
}

Addr
CohInterface::backendLineAddress(Addr addr) const
{
    const Addr line_addr = lineAddress(addr);
    fatal_if(line_addr < rubyAddrOffset,
             "CohInterface %s cannot subtract ruby_addr_offset %#llx from "
             "line address %#llx.",
             name().c_str(),
             static_cast<unsigned long long>(rubyAddrOffset),
             static_cast<unsigned long long>(line_addr));
    return line_addr - rubyAddrOffset;
}

const std::string &
CohInterface::protocolName() const
{
    return protocol;
}

unsigned
CohInterface::lineSize() const
{
    return lineSizeBytes;
}

bool
CohInterface::requiresCompletion() const
{
    return requireCompletion;
}

bool
CohInterface::frontEndOnly() const
{
    return frontEndOnlyMode;
}

} // namespace memory
} // namespace gem5
