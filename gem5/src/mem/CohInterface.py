from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import Parent


class CohInterface(ClockedObject):
    type = "CohInterface"
    cxx_header = "mem/coh_interface.hh"
    cxx_class = "gem5::memory::CohInterface"

    system = Param.System(Parent.any, "System this CohInterface belongs to")
    ruby_port = RequestPort(
        "Request port connected to a Ruby DMASequencer in_ports endpoint"
    )

    protocol = Param.String(
        "MI_example_Xerxes",
        "Ruby protocol backend for CohInterface scaffolding",
    )
    ruby_addr_offset = Param.Addr(
        0,
        "Offset subtracted from incoming invalidate addresses before issuing the Ruby request",
    )
    line_size = Param.Unsigned(64, "Cache line size used for address normalization")
    max_outstanding_per_line = Param.Unsigned(
        1,
        "Maximum outstanding invalidations per line in phase-1 scaffolding",
    )
    require_completion = Param.Bool(
        True,
        "Whether each request is expected to produce a completion",
    )
    front_end_only = Param.Bool(
        True,
        "Do not issue Ruby requests yet; instantiate the front-end scaffold only",
    )
    test_invalidate_on_startup = Param.Bool(
        False,
        "Issue one test invalidation request after startup for adapter bring-up",
    )
    test_invalidate_addr = Param.Addr(
        0,
        "Physical address used when test_invalidate_on_startup is enabled",
    )
    test_invalidate_delay_cycles = Param.Unsigned(
        1,
        "Cycles to wait after startup before issuing the test invalidation",
    )
