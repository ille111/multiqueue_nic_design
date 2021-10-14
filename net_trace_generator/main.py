# -*- coding: utf-8 -*-
""" This script generates a trace file representing incoming network packets. Traces are generated depending on the
number of IP flows, their individual loads, their scale (in min/max packets per second), and an optional DoS packet
stream.
"""

import csv
import argparse
from itertools import chain
from enum import Enum, auto

# time resolution is maximum packets per second
TIME_RESOLUTION = 1000000
DURATION = 30
NR_TICKS = DURATION * TIME_RESOLUTION


class LoadType(Enum):
    """ Types of load traces."""
    WAVE = auto()
    BURST = auto()
    CONSTANT = auto()
    TRACE = auto()


class IPFlow:
    """IPFlows are traces of incoming packets belonging to one receiving process.

    Attributes
    ----------
    load : LoadType
        Type of load as LoadType object. Ignored if load == TRACE.
    frequency : int
        Number of packets per second. Ignored if load == TRACE.
    source_ip : str
        Source IP address of packets. Used to identify flow in NIC simulator.
    offset : int
        Number of ticks to wait before packets are generated in flow. Useful to interleave flows and prevent regular
        bursts. Ignored if load == TRACE.
    trace_file : str
        Name of the file to read recorded trace from. File needs to contain one relative time entry per row and have
        equal TIME_RESOLUTION. Ignored if load != TRACE.

    """
    def __init__(self, load, frequency, source_ip, offset, trace_file=""):
        self.load = load
        self.frequency = frequency
        self.source_ip = source_ip
        self.offset = offset
        self.irq_trace = list()
        self.trace_file = trace_file

        if self.load == LoadType.WAVE:
            self.create_wave()
        elif self.load == LoadType.BURST:
            self.create_burst()
        elif self.load == LoadType.CONSTANT:
            self.create_const()
        elif self.load == LoadType.TRACE:
            self.create_from_trace()
        else:
            print("Error: " + self.load + " is not a valid load type.")

    def create_wave(self):
        print("NOT IMPLEMENTED")

    def create_burst(self):
        print("NOT IMPLEMENTED")

    def create_const(self):
        """Creates an IPFlow with constant traffic load. """
        delay = int((1/self.frequency) * TIME_RESOLUTION)
        t = self.offset
        while t <= NR_TICKS:
            self.irq_trace.append((t, self.source_ip))
            t += delay

    def create_from_trace(self):
        """Takes recorded network trace and transforms it into an IPFlow object. """
        with open(self.trace_file) as trace_file:
            trace_reader = csv.reader(trace_file)
            t = self.offset
            last_time = -1
            for entry in trace_reader:
                time = int(entry[0])
                if time >= NR_TICKS:
                    break
                if time > last_time:
                    self.irq_trace.append((time + self.offset, self.source_ip))
                last_time = time


def init_argparse() -> argparse.ArgumentParser:
    """Parses command line arguments. """
    parser = argparse.ArgumentParser(
        usage="%(prog)s  [FILE]",
        description="This script generates a network packet trace file for incoming packets. The output can be used by"
                    "the nic simulator in this repository."
    )
    parser.add_argument(
        "-v", "--version", action="version",
        version = f"{parser.prog} version 1.0.0"
    )
    parser.add_argument("outfile", help="Output file name", type=str)
    return parser


def join_and_sort(*argv):
    """ Takes variable number of traces and joins them to one sorted and keyed list."""
    joined_trace = list()
    for trace in argv:
        joined_trace += trace
    joined_trace.sort(key=lambda tup: tup[0])
    return joined_trace


def define_flows():
    """ This function holds the IP flow generation calls. During experimentation this is the only function that needs to
     be changed."""
    flows = list()
    flows.append(IPFlow(LoadType.TRACE, 0, "10.10.10.0", 1,
    trace_file="traces/modbus_clean_1min.csv"))
    flows.append(IPFlow(LoadType.TRACE, 0, "10.10.10.1", 250000,
    trace_file="traces/modbus_clean_1min.csv"))
    flows.append(IPFlow(LoadType.TRACE, 0, "10.10.10.2", 500000,
    trace_file="traces/modbus_clean_1min.csv"))
    flows.append(IPFlow(LoadType.TRACE, 0, "10.10.10.3", 750000,
    trace_file="traces/modbus_clean_1min.csv"))
    # flows.append(IPFlow(LoadType.TRACE, 0, "10.10.10.4", 1000000,
    # trace_file="traces/s7_1min.csv"))
    #flows.append(IPFlow(LoadType.CONSTANT, 25, "10.10.10.0", 225))
    #flows.append(IPFlow(LoadType.CONSTANT, 50, "10.10.10.1", 356))
    #flows.append(IPFlow(LoadType.CONSTANT, 75, "10.10.10.2", 469))
    #flows.append(IPFlow(LoadType.CONSTANT, 100, "10.10.10.3", 593))
    flows.append(IPFlow(LoadType.CONSTANT, 4800, "10.10.10.99", 2))

    return flows


if __name__ == '__main__':
    # parse arguments
    parser = init_argparse()
    args = parser.parse_args()
    traces = list()
    for ip_flow in define_flows():
        traces += ip_flow.irq_trace
    # join and sort traces
    wifi_trace = join_and_sort(traces)
    # open and write to output file
    with open(args.outfile, 'w') as output_file:
        writer = csv.writer(output_file)
        for row in wifi_trace:
            writer.writerow(row)
    output_file.close()
