import sys, re, difflib
def w(path, drv):
    out=[]
    for l in open(path):
        m=re.search(r"WRITE\s+(\w\w):(\w\w) = (\w\w)", l)
        if m: out.append("%s:%s=%s"%(m.group(1),m.group(2).upper(),m.group(3).upper()))
    return out
a=w(sys.argv[1],0); b=w(sys.argv[2],1)
print("windows %d, driver %d"%(len(a),len(b)))
sm=difflib.SequenceMatcher(None,a,b,autojunk=False)
for op,i1,i2,j1,j2 in sm.get_opcodes():
    if op=="equal": print("  = %d same (%s .. %s)"%(i2-i1,a[i1],a[i2-1])); continue
    print("  %s W[%d:%d] %s"%(op,i1,i2," ".join(a[i1:i2])))
    print("  %s D[%d:%d] %s"%(" "*len(op),j1,j2," ".join(b[j1:j2])))
