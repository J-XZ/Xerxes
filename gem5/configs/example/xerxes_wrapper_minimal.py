import argparse

import m5
from m5.objects import *


parser = argparse.ArgumentParser()
parser.add_argument(
    "--max-loads",
    type=int,
    default=8,
    help="Number of MemTest loads before the smoke test exits",
)
parser.add_argument(
    "--progress-interval",
    type=int,
    default=4,
    help="MemTest progress print interval",
)
args = parser.parse_args()

system = System(cache_line_size=64)
system.clk_domain = SrcClockDomain(
    clock="1.5GHz", voltage_domain=VoltageDomain(voltage="1V")
)
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("16MiB")]
system.mmap_using_noreserve = True

system.membus = SystemXBar()
system.system_port = system.membus.cpu_side_ports

system.mem_ctrl = XerxesWrapper()
system.mem_ctrl.dram = DDR3_1600_8x8(range=system.mem_ranges[0])
system.mem_ctrl.output_dir = m5.options.outdir
system.mem_ctrl.use_snoop = False

system.tester = MemTest(
    interval=5000,
    size=0x10000,
    base_addr_1=0x0,
    base_addr_2=0x100000,
    max_loads=args.max_loads,
    percent_reads=50,
    percent_functional=0,
    percent_uncacheable=0,
    percent_atomic=0,
    progress_interval=args.progress_interval,
    progress_check=5000000,
)
system.tester.port = system.membus.cpu_side_ports

system.membus.mem_side_ports = system.mem_ctrl.port

root = Root(full_system=False, system=system)

m5.instantiate()

exit_event = m5.simulate(0)
print(exit_event.getCause())
