#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""List the import address table of a PE32 file as CSV-like lines: <IAT address> <dll>!<function>.

  re/scripts/pe_imports.py 1602.exe > re/symbols/imports.txt
"""
import struct,sys
d=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=pe+24
base=struct.unpack_from('<I',d,opt+28)[0]; imp_rva=struct.unpack_from('<I',d,opt+104)[0]
secs=[struct.unpack_from('<8sIIII',d,opt+224+i*40) for i in range(nsec)]
def off(rva):
    for n,vs,va,rs,ro in secs:
        if va<=rva<va+max(vs,rs): return rva-va+ro
def cstr(o): return d[o:d.index(b'\0',o)].decode()
p=off(imp_rva)
while True:
    ilt,ts,fc,name,iat=struct.unpack_from('<5I',d,p); p+=20
    if not name: break
    dll=cstr(off(name)); t=off(ilt or iat); i=0
    while True:
        e=struct.unpack_from('<I',d,t+i*4)[0]
        if not e: break
        fn= ('#%d'%(e&0xffff)) if e&0x80000000 else cstr(off(e)+2)
        print('%08x %s!%s'%(base+iat+i*4,dll,fn)); i+=1
