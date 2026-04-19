from m5.objects.MemCtrl import MemCtrl
from m5.params import *


class XerxesWrapper(MemCtrl):
    type = "XerxesWrapper"
    cxx_header = "mem/xerxes_wrapper.hh"
    cxx_class = "gem5::memory::XerxesWrapper"

    cxlLatency = Param.Latency(
        "150000t",
        "Wrapper edge latency applied on request ingress and response egress",
    )
    output_dir = Param.String("output", "Output directory for Xerxes")
    log_name = Param.String(
        "xerxes_log.csv",
        "Xerxes log file name; relative paths are resolved under output_dir",
    )
    log_level = Param.String("INFO", "Xerxes log_level TOML field")
    bus_is_full = Param.Bool(
        True, "Whether the embedded Xerxes DuplexBus is full duplex"
    )
    bus_half_rev_time = Param.Latency(
        "100t", "Xerxes DuplexBus half-duplex reverse penalty"
    )
    bus_delay_per_t = Param.Latency(
        "1t", "Xerxes DuplexBus delay_per_T field"
    )
    bus_width = Param.Unsigned(
        32, "Xerxes DuplexBus width TOML field, in bits"
    )
    bus_framing_time = Param.Latency(
        "20t", "Xerxes DuplexBus framing_time field"
    )
    bus_frame_size = Param.Unsigned(
        256, "Xerxes DuplexBus frame_size TOML field, in bytes"
    )
    coh_interface = Param.CohInterface(
        NULL, "Optional CohInterface used for Xerxes back-invalidation"
    )
    use_snoop = Param.Bool(False, "Enable Xerxes snoop stage")
    snoop_line_num = Param.Int(1024, "Number of lines in snoop cache")
    snoop_assoc = Param.Int(8, "Associativity of snoop cache")
    snoop_max_burst_inv = Param.Unsigned(
        8, "Xerxes Snoop max_burst_inv TOML field"
    )
    snoop_eviction = Param.String(
        "LRU", "Xerxes Snoop eviction TOML field"
    )
    snoopPort = ResponsePort(
        "Legacy reserved port; active coherence path uses coh_interface"
    )
