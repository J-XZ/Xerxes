import math

import m5
from m5.defines import buildEnv
from m5.objects import *

from .MI_example import L1Cache
from .Ruby import (
    create_directories,
    create_topology,
    send_evicts,
)


def define_options(parser):
    return


def create_system(
    options, full_system, system, dma_ports, bootmem, ruby_system, cpus
):
    if buildEnv["PROTOCOL"] != "MI_example_Xerxes":
        panic("This script requires the MI_example_Xerxes protocol to be built.")

    cpu_sequencers = []
    l1_cntrl_nodes = []
    dma_cntrl_nodes = []
    block_size_bits = int(math.log(options.cacheline_size, 2))

    for i in range(options.num_cpus):
        cache = L1Cache(
            size=options.l1d_size,
            assoc=options.l1d_assoc,
            start_index_bit=block_size_bits,
        )

        clk_domain = cpus[i].clk_domain

        l1_cntrl = MI_example_Xerxes_L1Cache_Controller(
            version=i,
            cacheMemory=cache,
            send_evictions=send_evicts(options),
            transitions_per_cycle=options.ports,
            clk_domain=clk_domain,
            ruby_system=ruby_system,
        )

        cpu_seq = RubySequencer(
            version=i,
            dcache=cache,
            clk_domain=clk_domain,
            ruby_system=ruby_system,
        )

        l1_cntrl.sequencer = cpu_seq
        exec("ruby_system.l1_cntrl%d = l1_cntrl" % i)
        cpu_sequencers.append(cpu_seq)
        l1_cntrl_nodes.append(l1_cntrl)

        l1_cntrl.mandatoryQueue = MessageBuffer()
        l1_cntrl.requestFromCache = MessageBuffer(ordered=True)
        l1_cntrl.requestFromCache.out_port = ruby_system.network.in_port
        l1_cntrl.responseFromCache = MessageBuffer(ordered=True)
        l1_cntrl.responseFromCache.out_port = ruby_system.network.in_port
        l1_cntrl.forwardToCache = MessageBuffer(ordered=True)
        l1_cntrl.forwardToCache.in_port = ruby_system.network.out_port
        l1_cntrl.responseToCache = MessageBuffer(ordered=True)
        l1_cntrl.responseToCache.in_port = ruby_system.network.out_port

    phys_mem_size = sum([r.size() for r in system.mem_ranges])
    assert phys_mem_size % options.num_dirs == 0

    ruby_system.memctrl_clk_domain = DerivedClockDomain(
        clk_domain=ruby_system.clk_domain, clk_divider=3
    )

    mem_dir_cntrl_nodes, rom_dir_cntrl_node = create_directories(
        options, bootmem, ruby_system, system
    )
    dir_cntrl_nodes = mem_dir_cntrl_nodes[:]
    if rom_dir_cntrl_node is not None:
        dir_cntrl_nodes.append(rom_dir_cntrl_node)

    for dir_cntrl in dir_cntrl_nodes:
        dir_cntrl.requestToDir = MessageBuffer(ordered=True)
        dir_cntrl.requestToDir.in_port = ruby_system.network.out_port
        dir_cntrl.dmaRequestToDir = MessageBuffer(ordered=True)
        dir_cntrl.dmaRequestToDir.in_port = ruby_system.network.out_port

        dir_cntrl.responseFromDir = MessageBuffer()
        dir_cntrl.responseFromDir.out_port = ruby_system.network.in_port
        dir_cntrl.dmaResponseFromDir = MessageBuffer(ordered=True)
        dir_cntrl.dmaResponseFromDir.out_port = ruby_system.network.in_port
        dir_cntrl.forwardFromDir = MessageBuffer()
        dir_cntrl.forwardFromDir.out_port = ruby_system.network.in_port
        dir_cntrl.requestToMemory = MessageBuffer()
        dir_cntrl.responseFromMemory = MessageBuffer()

    for i, dma_port in enumerate(dma_ports):
        dma_seq = DMASequencer(version=i, ruby_system=ruby_system)

        dma_cntrl = MI_example_Xerxes_DMA_Controller(
            version=i,
            dma_sequencer=dma_seq,
            transitions_per_cycle=options.ports,
            ruby_system=ruby_system,
        )

        exec("ruby_system.dma_cntrl%d = dma_cntrl" % i)
        exec("ruby_system.dma_cntrl%d.dma_sequencer.in_ports = dma_port" % i)
        dma_cntrl_nodes.append(dma_cntrl)

        dma_cntrl.mandatoryQueue = MessageBuffer()
        dma_cntrl.requestToDir = MessageBuffer()
        dma_cntrl.requestToDir.out_port = ruby_system.network.in_port
        dma_cntrl.responseFromDir = MessageBuffer(ordered=True)
        dma_cntrl.responseFromDir.in_port = ruby_system.network.out_port

    all_cntrls = l1_cntrl_nodes + dir_cntrl_nodes + dma_cntrl_nodes

    if full_system:
        io_seq = DMASequencer(version=len(dma_ports), ruby_system=ruby_system)
        ruby_system._io_port = io_seq
        io_controller = MI_example_Xerxes_DMA_Controller(
            version=len(dma_ports),
            dma_sequencer=io_seq,
            ruby_system=ruby_system,
        )
        ruby_system.io_controller = io_controller

        io_controller.mandatoryQueue = MessageBuffer()
        io_controller.requestToDir = MessageBuffer()
        io_controller.requestToDir.out_port = ruby_system.network.in_port
        io_controller.responseFromDir = MessageBuffer(ordered=True)
        io_controller.responseFromDir.in_port = ruby_system.network.out_port

        all_cntrls = all_cntrls + [io_controller]

    ruby_system.network.number_of_virtual_networks = 5
    topology = create_topology(all_cntrls, options)
    return (cpu_sequencers, mem_dir_cntrl_nodes, topology)
