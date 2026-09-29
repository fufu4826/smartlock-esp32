"""Compare private NVS snapshots without emitting values, hashes, salts or secrets."""
import argparse
import json
from pathlib import Path
import struct

def logical(path):
    data=Path(path).read_bytes()
    assert len(data)==20480
    pages=[]
    for offset in range(0,len(data),4096):
        page=data[offset:offset+4096]
        if page[:4]!=b'\xff'*4:pages.append((struct.unpack_from('<I',page,4)[0],page))
    raw=[];names={}
    for _,page in sorted(pages):
        entry=0
        while entry<126:
            state=(page[32+entry//4]>>((entry%4)*2))&3
            record=page[64+entry*32:96+entry*32]
            if state!=2:entry+=1;continue
            ns,kind,span,chunk=record[:4]
            if not span or entry+span>126:raise ValueError('Invalid entry span')
            key=record[8:24].split(b'\0',1)[0].decode('ascii')
            if ns==0 and kind==1:names[record[24]]=key
            elif kind in (0x21,0x42):
                size=struct.unpack_from('<H',record,24)[0]
                raw.append((ns,key,kind,chunk,page[96+entry*32:96+entry*32+size]))
            elif kind!=0x48:raw.append((ns,key,kind,0,record[24:32]))
            entry+=span
    result={}
    for ns,key,kind,chunk,value in raw:
        result[(names.get(ns,str(ns)),key,kind,chunk)]=value
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('before');p.add_argument('after');p.add_argument('output');a=p.parse_args()
    before,after=logical(a.before),logical(a.after)
    namespaces=['sl-config','sl-pin','sl-net','sl-sta','lock-touch-v2']
    rows=[]
    for namespace in namespaces:
        first={k:v for k,v in before.items() if k[0]==namespace}
        second={k:v for k,v in after.items() if k[0]==namespace}
        rows.append({'namespace':namespace,'entries_before':len(first),'entries_after':len(second),
                     'byte_identical':first==second,'present':bool(first)})
    assert any(k[0]=='sl-pin' and k[1]=='verifier' for k in before)
    report={'compared':'logical NVS values only; no values or hashes exported','namespaces':rows,
            'all_unchanged':all(row['byte_identical'] for row in rows)}
    Path(a.output).write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))
    assert report['all_unchanged'],'Persistent production configuration changed'

if __name__=='__main__':main()
