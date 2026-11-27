import sys,re
# map (addr,lsb,bits)->name from the header
names={}
for l in open(sys.argv[1]):
    m=re.match(r'#define GCHD_TC_(\w+)\s+0x([0-9a-f]+),\s*(\d+),\s*(\d+)',l)
    if m: names[(int(m[2],16),int(m[3]),int(m[4]))]=m[1].lower()
def drv(fn):
    d={}
    for l in open(fn):
        m=re.match(r'SOCF\s+([0-9a-f]+)/(\d+)/(\d+) = (\d+)',l)
        if m:
            k=names.get((int(m[1],16),int(m[2]),int(m[3])),'%s/%s/%s'%(m[1],m[2],m[3]))
            d[k]=m[4]
    return d
def rec(fn,a,b):
    d={}
    for l in open(fn):
        m=re.match(r'\s*([\d.]+)\s+SOC\s+\w+\s+(\w+)=(\S+)',l)
        if m and a<=float(m[1])<=b: d[m[2].lower()]=m[3]
    return d
D=drv(sys.argv[2]); R=rec(sys.argv[3],0,1e9)
diff=[(k,R[k],D.get(k)) for k in sorted(R) if D.get(k)!=R[k]]
print('recorded fields %d, differing %d'%(len(R),len(diff)))
for k,r,dv in diff: print('  %-28s rec=%-8s drv=%s'%(k,r,dv))
