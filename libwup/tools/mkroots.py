#!/usr/bin/env python3
"""The roots libwup's HTTPS client trusts by default (include/wup_roots.h, kept in git): from Mozilla's list as curl
publishes it (https://curl.se/ca/cacert.pem), the certificate authorities behind most of the web, turned into C by
BearSSL's own tool (brssl ta). Run it now and then to follow Mozilla's changes.

    python3 tools/mkroots.py                    -> include/wup_roots.h
    python3 tools/mkroots.py --test CA.pem      -> build/https/wup_roots_test.h: the same and a test CA (test servers;
                                                   build with WUP_DEFINES=WUP_HTTPS_TEST_ROOTS)"""
import os, re, subprocess, sys, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths, bearssl

ROOTS = [
    'ISRG Root X1', 'ISRG Root X2',                                           # Let's Encrypt
    'USERTrust ECC Certification Authority', 'USERTrust RSA Certification Authority',   # Sectigo (GitHub's API)
    'Sectigo Public Server Authentication Root E46', 'Sectigo Public Server Authentication Root R46',
    'DigiCert Global Root G2', 'DigiCert Global Root G3',
    'GlobalSign Root CA - R3', 'GlobalSign Root CA - R6', 'GlobalSign Root E46', 'GlobalSign Root R46',
    'Amazon Root CA 1', 'Amazon Root CA 2', 'Amazon Root CA 3', 'Amazon Root CA 4',
    'GTS Root R1', 'GTS Root R3', 'GTS Root R4',                              # Google
    'Microsoft RSA Root Certificate Authority 2017', 'Microsoft ECC Root Certificate Authority 2017',
    'Starfield Root Certificate Authority - G2',
]
PEM = os.path.join(paths.ROOT, 'include', 'wup_roots.pem')


def to_c(pem, out, note):
    c = subprocess.run([bearssl.brssl(), 'ta', '-q', pem], check=True, capture_output=True, text=True).stdout
    c = c.replace('TAs_NUM', 'WUP_ROOTS_NUM').replace('TAs[', 'wup_roots[')
    open(out, 'w').write('/* %s. Made by tools/mkroots.py (brssl ta): do not edit. */\n#pragma once\n#include <bearssl.h>\n%s' % (note, c))


def main():
    if len(sys.argv) > 2 and sys.argv[1] == '--test':
        d = paths.out('https'); p = os.path.join(d, 'wup_roots_test.pem')
        open(p, 'w').write(open(PEM).read() + open(sys.argv[2]).read())
        to_c(p, os.path.join(d, 'wup_roots_test.h'), 'TEST BUILD: the default roots and a test CA')
        print(os.path.join(d, 'wup_roots_test.h')); return
    bundle = urllib.request.urlopen('https://curl.se/ca/cacert.pem', timeout=30).read().decode()
    date = re.search(r'Certificate data from Mozilla as of: (.*)', bundle)
    found = dict(re.findall(r'\n([^\n#=][^\n]*)\n=+\n(?:#[^\n]*\n)*(-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)', bundle, re.S))
    missing = [n for n in ROOTS if n not in found]
    if missing: sys.exit("not in Mozilla's list any more (take them out of ROOTS): %s" % ', '.join(missing))
    note = "Mozilla's roots (curl's cacert.pem, %s), %d of them" % (date.group(1).strip() if date else '?', len(ROOTS))
    open(PEM, 'w').write('# %s\n' % note + ''.join('\n# %s\n%s\n' % (n, found[n]) for n in ROOTS))
    to_c(PEM, os.path.join(paths.ROOT, 'include', 'wup_roots.h'), note)
    print('%d roots -> include/wup_roots.h' % len(ROOTS))


if __name__ == '__main__':
    main()
