import argparse
import os

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal

addToPath("../")

from common import Options
from ruby import Ruby


config_path = os.path.dirname(os.path.abspath(__file__))
config_root = os.path.dirname(config_path)


def attach_directed_tester(system):
    for ruby_port in system.ruby._cpu_ports:
        system.directed_tester.cpuPort = ruby_port.in_ports


def build_coh_only_system(args, invalidate_addr):
    system = System(
        cache_line_size=args.cacheline_size,
        mem_ranges=[AddrRange(args.mem_size)],
    )
    system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)
    system.clk_domain = SrcClockDomain(
        clock=args.sys_clock, voltage_domain=system.voltage_domain
    )
    system.mem_mode = "timing"

    generator = SeriesRequestGenerator(
        num_cpus=args.num_cpus,
        percent_writes=0,
        addr_increment_size=0,
    )
    system.directed_tester = RubyDirectedTester(
        requests_to_complete=args.ruby_directed_requests,
        generator=generator,
    )

    system.coh_interface = CohInterface()
    system.coh_interface.protocol = "MESI_Two_Level_Xerxes"
    system.coh_interface.line_size = args.cacheline_size
    system.coh_interface.max_outstanding_per_line = 1
    system.coh_interface.require_completion = True
    system.coh_interface.front_end_only = False
    system.coh_interface.test_invalidate_on_startup = True
    system.coh_interface.test_invalidate_addr = invalidate_addr
    system.coh_interface.test_invalidate_delay_cycles = (
        args.invalidate_delay_cycles
    )

    cpu_list = [system.directed_tester] * args.num_cpus
    Ruby.create_system(
        args,
        False,
        system,
        cpus=cpu_list,
        dma_ports=[system.coh_interface.ruby_port],
    )

    system.ruby.clk_domain = SrcClockDomain(
        clock=args.ruby_clock, voltage_domain=system.voltage_domain
    )
    attach_directed_tester(system)
    return system


def build_runtime_inv_system(args):
    system = System(
        cache_line_size=args.cacheline_size,
        mem_ranges=[AddrRange(args.mem_size)],
    )
    system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)
    system.clk_domain = SrcClockDomain(
        clock=args.sys_clock, voltage_domain=system.voltage_domain
    )
    system.mem_mode = "timing"
    system.mmap_using_noreserve = True
    system.membus = SystemXBar()

    system.tgen = PyTrafficGen()

    system.coh_interface = CohInterface()
    system.coh_interface.protocol = "MESI_Two_Level_Xerxes"
    system.coh_interface.ruby_addr_offset = 0x100000000
    system.coh_interface.line_size = args.cacheline_size
    system.coh_interface.max_outstanding_per_line = 1
    system.coh_interface.require_completion = True
    system.coh_interface.front_end_only = False
    system.coh_interface.test_invalidate_on_startup = False

    system.mem_ctrl = XerxesWrapper()
    system.mem_ctrl.dram = DDR3_1600_8x8(
        range=AddrRange(0x100000000, size=args.mem_size)
    )
    system.mem_ctrl.output_dir = os.path.join(m5.options.outdir, "xerxes")
    system.mem_ctrl.use_snoop = True
    system.mem_ctrl.snoop_line_num = 1
    system.mem_ctrl.snoop_assoc = 1
    system.mem_ctrl.snoop_max_burst_inv = 1
    system.mem_ctrl.snoop_eviction = "LIFO"
    system.mem_ctrl.bus_is_full = True
    system.mem_ctrl.bus_half_rev_time = "0t"
    system.mem_ctrl.bus_delay_per_t = "20t"
    system.mem_ctrl.bus_width = 32
    system.mem_ctrl.bus_framing_time = "10000t"
    system.mem_ctrl.bus_frame_size = 64
    system.mem_ctrl.coh_interface = system.coh_interface
    system.tgen.port = system.mem_ctrl.port

    generator = SeriesRequestGenerator(
        num_cpus=args.num_cpus,
        percent_writes=100,
        addr_increment_size=0,
    )
    system.directed_tester = RubyDirectedTester(
        requests_to_complete=args.ruby_directed_requests,
        generator=generator,
    )

    cpu_list = [system.directed_tester] * args.num_cpus
    Ruby.create_system(
        args,
        False,
        system,
        piobus=system.membus,
        cpus=cpu_list,
        dma_ports=[system.coh_interface.ruby_port],
    )

    system.ruby.clk_domain = SrcClockDomain(
        clock=args.ruby_clock, voltage_domain=system.voltage_domain
    )
    attach_directed_tester(system)
    return system


parser = argparse.ArgumentParser()
Options.addNoISAOptions(parser)
Ruby.define_options(parser)
parser.add_argument(
    "--scenario",
    choices=("i_path", "runtime_inv"),
    default="runtime_inv",
    help="Select a MESI_Two_Level_Xerxes coherence evidence scenario",
)
parser.add_argument(
    "--traffic-lines",
    type=int,
    default=2,
    help="How many cache lines PyTrafficGen should stream through Xerxes",
)
parser.add_argument(
    "--traffic-period",
    type=int,
    default=1000,
    help="Min/max period, in ticks, between generated Xerxes requests",
)
parser.add_argument(
    "--ruby-directed-requests",
    type=int,
    default=None,
    help="How many directed Ruby requests to allow before tester exits",
)
parser.add_argument(
    "--invalidate-delay-cycles",
    type=int,
    default=None,
    help="Cycles to wait before CohInterface injects the test invalidation",
)

exec(
    compile(
        open(os.path.join(config_root, "common", "Options.py")).read(),
        os.path.join(config_root, "common", "Options.py"),
        "exec",
    )
)

args = parser.parse_args()

if buildEnv["PROTOCOL"] != "MESI_Two_Level_Xerxes":
    fatal("This script requires build/X86_MESI_Two_Level_Xerxes/gem5.opt.")

args.l1d_size = "256B"
args.l1i_size = "256B"
args.l2_size = "512B"
args.l1d_assoc = 2
args.l1i_assoc = 2
args.l2_assoc = 2
args.mem_size = "16MiB"

if args.scenario == "runtime_inv":
    args.num_cpus = 1
    if args.ruby_directed_requests is None:
        args.ruby_directed_requests = 100000
    if args.invalidate_delay_cycles is None:
        args.invalidate_delay_cycles = 1000
    system = build_runtime_inv_system(args)
else:
    args.num_cpus = 1
    if args.ruby_directed_requests is None:
        args.ruby_directed_requests = 10
    if args.invalidate_delay_cycles is None:
        args.invalidate_delay_cycles = 1
    system = build_coh_only_system(args, 0x1000)

root = Root(full_system=False, system=system)

m5.ticks.setGlobalFrequency("1ns")
m5.instantiate()

if args.scenario == "runtime_inv":
    traffic_bytes = args.traffic_lines * args.cacheline_size
    linear = system.tgen.createLinear(
        1000000,
        0x100000000,
        0x100000000 + traffic_bytes,
        args.cacheline_size,
        args.traffic_period,
        args.traffic_period,
        100,
        traffic_bytes,
    )
    exit_gen = system.tgen.createExit(0)
    system.tgen.start([linear, exit_gen])

exit_event = m5.simulate(args.abs_max_tick)
print("Exiting @ tick", m5.curTick(), "because", exit_event.getCause())
