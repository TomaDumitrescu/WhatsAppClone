#!/usr/bin/env python

"""
The example topology creates a router and multiple IP subnets:

    - 192.168.0.0/24 (router-eth0, IP: 192.168.0.1)
    - 192.168.1.0/24 to 192.168.N.0/24 (router-eth1 to router-ethN, IP: 192.168.1.1 to 192.168.N.1)
"""

import argparse
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import Node
from mininet.log import setLogLevel
from mininet.cli import CLI
from mininet.link import TCLink

class LinuxRouter(Node):
    "A Node with IP forwarding enabled."

    def config(self, **params):
        super(LinuxRouter, self).config(**params)
        # Enable forwarding on the router
        self.cmd('sysctl net.ipv4.ip_forward=1')

    def terminate(self):
        self.cmd('sysctl net.ipv4.ip_forward=0')
        super(LinuxRouter, self).terminate()

class NetworkTopo(Topo):
    "A LinuxRouter connecting multiple IP subnets"

    def build(self, num_clients=10, **_opts):
        # Set up the router
        defaultIP = '192.168.0.1/24'  # IP address for router-eth0
        router = self.addNode('router', cls=LinuxRouter, ip=defaultIP)

        # Add a server connected to router-eth0
        server = self.addHost('server', ip='192.168.0.2/24',
                              defaultRoute='via 192.168.0.1')
        self.addLink(server, router, intfName1='router-eth0', params1={'ip': '192.168.0.1/24'})

        # Dynamically add clients connected to the router
        for i in range(1, num_clients + 1):
            client_ip = f'192.168.{i}.2/24'
            router_ip = f'192.168.{i}.1/24'
            client_name = f'client{i}'

            client = self.addHost(client_name, ip=client_ip, defaultRoute=f'via 192.168.{i}.1')
            self.addLink(client, router, intfName2=f'router-eth{i}', params2={'ip': router_ip})

def run(num_clients):
    topo = NetworkTopo(num_clients=num_clients)  # Use the number of clients from CLI
    net = Mininet(topo=topo, xterms=True, link=TCLink, waitConnected=True, controller=None)
    net.start()

    CLI(net)
    net.stop()

if __name__ == '__main__':
    # Set up command-line argument parsing
    parser = argparse.ArgumentParser(description="Mininet Topology with a configurable number of clients.")
    parser.add_argument('--clients', type=int, default=2, help='Number of clients to add to the topology (default is 2)')
    args = parser.parse_args()

    setLogLevel('info')  # Set Mininet log level to info
    run(num_clients=args.clients)  # Pass the number of clients to the run function
