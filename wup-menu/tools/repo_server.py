#!/usr/bin/env python3
"""The catalogue on this computer, over HTTPS with a test CA, until the public one is up: for test builds of the
App Store and Software Update.

    python3 tools/mkrepo.py && python3 tools/repo_server.py [--port 8443]

It makes build/test-ca/ca.pem (once) and a certificate for this computer's address, and serves build/repo. Test builds:
    python3 ../libwup/tools/mkroots.py --test build/test-ca/ca.pem
    WUP_DEFINES='WUP_HTTPS_TEST_ROOTS CATALOG_URL="https://<this computer>:8443"' python3 ../libwup/tools/wupbuild.py apps/app_store"""
import functools, http.server, os, socket, ssl, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths

D = paths.local_out('test-ca')


def my_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: s.connect(('192.0.2.1', 9)); return s.getsockname()[0]
    finally: s.close()


def certs(ip):
    ca, cakey, key, crt = (os.path.join(D, f) for f in ('ca.pem', 'ca.key', 'server.key', 'server.pem'))
    run = lambda *a: subprocess.run(['openssl'] + list(a), check=True, capture_output=True)
    if not os.path.exists(ca):
        run('req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256', '-nodes', '-days', '3650',
            '-subj', '/CN=wup-homebrew test CA', '-keyout', cakey, '-out', ca)
    ext = os.path.join(D, 'server.ext'); open(ext, 'w').write('subjectAltName=IP:%s,DNS:%s\nbasicConstraints=CA:FALSE\n' % (ip, ip))   # (BearSSL matches DNS names only)
    csr = os.path.join(D, 'server.csr')
    run('req', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256', '-nodes', '-subj', '/CN=%s' % ip, '-keyout', key, '-out', csr)
    run('x509', '-req', '-in', csr, '-CA', ca, '-CAkey', cakey, '-CAcreateserial', '-days', '365', '-extfile', ext, '-out', crt)
    return ca, key, crt


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *a): print(self.command, self.path, a[1] if len(a) > 1 else '', flush=True)


def main():
    port = int(sys.argv[sys.argv.index('--port') + 1]) if '--port' in sys.argv else 8443
    repo = os.path.join(paths.local_out(), 'repo')
    if not os.path.exists(os.path.join(repo, 'repo.json')): sys.exit('no catalogue: python3 tools/mkrepo.py first')
    ip = my_ip(); ca, key, crt = certs(ip)
    srv = http.server.ThreadingHTTPServer(('0.0.0.0', port), functools.partial(Handler, directory=repo))
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(crt, key)
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    print('serving %s at https://%s:%d (test CA %s)' % (repo, ip, port, ca), flush=True)
    srv.serve_forever()


if __name__ == '__main__':
    main()
