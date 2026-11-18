"""lwIP 2.2.0 for libwup's networking (src/): where it lives and which files are built. fetch() clones it once."""
import os, subprocess
import paths
DIR = os.path.join(paths.THIRD, 'lwip')                 # git-ignored checkout
SRC = os.path.join(DIR, 'src')
SRCS = (['core/' + f for f in ('def.c', 'inet_chksum.c', 'init.c', 'ip.c', 'mem.c', 'memp.c', 'netif.c', 'pbuf.c', 'raw.c',
                               'stats.c', 'sys.c', 'tcp.c', 'tcp_in.c', 'tcp_out.c', 'timeouts.c', 'udp.c', 'dns.c')]
        + ['core/ipv4/' + f for f in ('dhcp.c', 'etharp.c', 'icmp.c', 'ip4.c', 'ip4_addr.c', 'ip4_frag.c')]
        + ['netif/ethernet.c', 'apps/sntp/sntp.c'])


SOCKETS = ['api/' + f for f in ('api_lib.c', 'api_msg.c', 'err.c', 'netbuf.c', 'netdb.c', 'sockets.c', 'tcpip.c')]   # net = yes


def fetch(sockets=False):
    import deps; deps.fetch('lwip')
    return [os.path.join(SRC, s) for s in SRCS + (SOCKETS if sockets else [])]
