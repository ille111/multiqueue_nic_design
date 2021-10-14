import argparse
import csv

from simpy import Interrupt
from tqdm import tqdm
import json
from typing import Tuple, List, Dict, Optional

import simpy

# Interrupt trace entry: (<interrupt time>, <IPs>, <reason>)
# where IPs is the list of packet IPs that triggered the interrupt
InterruptTrace = List[Tuple[int, List[str], str]]

LOG = False
RUNTIME = 1000000000  # in us
PRINT_PROGRESS_AFTER = 1000000  # in ms (printing progress impacts performance)


class Packet:
    irq_time: int = None

    def __init__(self, seq_no: int, ip: str, arrival_time: int):
        self.seq_no = seq_no
        self.ip = ip
        self.arrival_time = arrival_time


class Buffer(simpy.Store):
    def __init__(self,
                 env: simpy.Environment,
                 name: str,
                 interrupt_trace: InterruptTrace,
                 packet_limit: int = None,
                 absolute_time_limit: int = None,
                 absolute_time_limit_offset: int = 0,
                 packet_time_limit: int = None,
                 *args, **kwargs):
        """NIC Buffer.

        Args:
            env: Simpy environment for registering processes
            name: Name of the buffer (only used for logging)
            interrupt_trace: Interrupts caused by this buffer will be appended to this list
            packet_limit: Maximum number of packets the buffer can hold before flushing
            absolute_time_limit: Time between flushes
            absolute_time_limit_offset: Initial offset of the first flush caused by `absolute_time_limit`.
                Can only be used in conjunction with `absolute_time_limit`.
        """
        super().__init__(env, *args, **kwargs)
        self.env = env
        self.name = name
        self.interrupt_trace = interrupt_trace
        self.packet_limit = packet_limit
        self.absolute_time_limit = absolute_time_limit
        if absolute_time_limit_offset != 0 and absolute_time_limit is None:
            raise ValueError("`absolute_time_limit_offset` can only be used in conjunction with `absolute_time_limit`")
        self.absolute_time_limit_offset = absolute_time_limit_offset
        if absolute_time_limit is not None:
            self.env.process(self._absolute_timer())
        self.packet_time_limit = packet_time_limit
        self.packet_timer = None

    def put(self, packet: Packet):
        """Overwrite: After putting we check if the buffers packet limit was reached and flush if so"""
        super().put(packet)
        if self._is_packet_limit_reached():
            self.flush("packet_limit")
        if self.packet_time_limit is not None:
            self._reset_packet_timer()
            self.packet_timer = self.env.process(self._packet_timer())

    def flush(self, reason):
        _log(self.env.now, self.name, f"Flushing {len(self.items)} packets")
        self.interrupt_trace.append((self.env.now, [packet.ip for packet in self.items], reason))
        for packet in self.items:
            packet.irq_time = self.env.now
        self.items = []

    def _absolute_timer(self):
        """Periodically flushes the buffer"""
        yield self.env.timeout(self.absolute_time_limit_offset)
        while True:
            yield self.env.timeout(self.absolute_time_limit)
            self._reset_packet_timer()
            if self.items:
                self.flush("absolute_timer")
            else:
                _log(self.env.now, self.name, f"Flushing {len(self.items)} packets (no interrupt)")

    def _packet_timer(self):
        try:
            yield self.env.timeout(self.packet_time_limit)
        except Interrupt:
            pass
        else:
            self.flush("packet_timer")

    def _reset_packet_timer(self):
        if self.packet_timer is not None and not self.packet_timer.processed:
            self.packet_timer.interrupt()
            self.packet_timer = None

    def _is_packet_limit_reached(self) -> bool:
        return self.packet_limit is not None and len(self.items) >= self.packet_limit


def main(packet_trace_csv: str, config_json: str, irqout: str, seqout: str):
    with open(packet_trace_csv) as f:
        reader = csv.reader(f)
        packet_trace = [(int(time_str), ip) for time_str, ip in reader]
    with open(config_json) as f:
        config = json.load(f)

    env = simpy.Environment()
    interrupt_trace = []
    seqout_trace: List[Packet] = []

    ip_buffer_mapping = {}
    for ip in config["pass_through_ips"]:
        ip_buffer_mapping[ip] = None
        print(f"Assigned {ip} to pass through directly.")
    for buf in config["buffers"]:
        buffer = Buffer(env, name=buf["name"],
                        interrupt_trace=interrupt_trace,
                        absolute_time_limit=buf.get("absolute_time_limit"),
                        absolute_time_limit_offset=buf.get("absolute_time_limit_offset", 0),
                        packet_time_limit=buf.get("packet_time_limit"),
                        packet_limit=buf.get("packet_limit"),
                        capacity=buf.get("capacity"))
        for ip in buf["ips"]:
            assert ip not in ip_buffer_mapping, f"IP {ip} already in mapping"
            ip_buffer_mapping[ip] = buffer
            print(f"Assigned {ip} to buffer '{buffer.name}'.")

    env.process(nic(env, packet_trace, ip_buffer_mapping, interrupt_trace, seqout_trace))
    for i in tqdm(range(1, RUNTIME, 1000000)):
        env.run(until=i)
    env.run(until=RUNTIME)
    write_interrupt_trace(interrupt_trace, irqout)
    write_seqout_trace(seqout_trace, seqout)


def nic(env: simpy.Environment, packet_trace: List[Tuple[int, str]], ip_buffer_mapping: Dict[str, Optional[Buffer]],
        interrupt_trace: InterruptTrace, seqout_trace: List[Packet]):
    _log(env.now, "NIC", "Starting packet generator")
    for i, (time, ip) in enumerate(packet_trace):
        yield env.timeout(time - env.now)  # Wait for new packet to arrive
        packet = Packet(seq_no=i, ip=ip, arrival_time=env.now)
        seqout_trace.append(packet)
        try:
            buffer = ip_buffer_mapping[ip]
        except KeyError:
            _log(env.now, "NIC", f"Dropped packet with IP {ip}")
            continue
        if buffer is None:
            # No buffer, directly trigger interrupt
            interrupt_trace.append((env.now, [ip]))
            packet.irq_time = env.now
            continue
        buffer.put(packet)
    _log(env.now, "NIC", "Packet generator finished")


def write_interrupt_trace(interrupt_trace: InterruptTrace, outfile: str):
    with open(outfile, "w") as csv_file:
        writer = csv.writer(csv_file)
        for interrupt in interrupt_trace:
            writer.writerow(interrupt[:2])
    with open(outfile[:-4] + ".stats.csv", "w") as csv_file:
        writer = csv.writer(csv_file)
        for interrupt in interrupt_trace:
            writer.writerow(interrupt)


def write_seqout_trace(seqout_trace: List[Packet], outfile: str):
    with open(outfile, "w") as csv_file:
        writer = csv.writer(csv_file)
        for packet in seqout_trace:
            # [seq no], [packet arrival time], [irq time], [last byte of ip]
            if packet.irq_time:
                writer.writerow((packet.seq_no, packet.arrival_time, packet.irq_time, packet.ip.split(".")[-1]))


def _log(time: int, name: str, message: str):
    if LOG:
        print(f"{time:<5}{name:<20}{message}")


if __name__ == '__main__':
    # EXAMPLE: python main.py example_packet_trace.csv --config example_config.json --irqout example_interrupt_trace.csv --seqout example_seq_trace.csv
    parser = argparse.ArgumentParser(
        usage="%(prog)s [packet_trace_csv] --config [config_json] --irqout [irqout_csv] --seqout [seqout_csv]",
        description="This script generates an interrupt trace file from an ingress network trace file. "
                    "The input can be generated by the net_trace_generator in this repository."
    )
    parser.add_argument("packet_trace_csv", help="Input packet trace CSV")
    parser.add_argument("--config", help="Configuration JSON")
    parser.add_argument("--irqout", help="Interrupt CSV file name")
    parser.add_argument("--seqout", help="Packets with sequence numbers CSV file name")
    args = parser.parse_args()
    main(packet_trace_csv=args.packet_trace_csv, config_json=args.config, irqout=args.irqout, seqout=args.seqout)
