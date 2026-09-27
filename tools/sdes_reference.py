P10=[3,5,2,7,4,10,1,9,8,6]; P8=[6,3,7,4,8,5,10,9]
IP=[2,6,3,1,4,8,5,7]; IPi=[4,1,3,5,7,2,8,6]
EP=[4,1,2,3,2,3,4,1]; P4=[2,4,3,1]
S0=[[1,0,3,2],[3,2,1,0],[0,3,2,1],[3,1,3,2]]
S1=[[0,1,2,3],[2,0,1,3],[3,0,1,0],[2,1,0,3]]
def perm(v,t,w):
    r=0
    for i,ti in enumerate(t):
        b=(v>>(w-ti))&1
        r|=b<<(len(t)-1-i)
    return r
def ls10(v,s):
    l=(v>>5)&31; r=v&31
    l=((l<<s)|(l>>(5-s)))&31; r=((r<<s)|(r>>(5-s)))&31
    return (l<<5)|r
def subkeys(k):
    p=perm(k,P10,10); s1=ls10(p,1); s3=ls10(s1,2)
    return perm(s1,P8,8,10) if False else (perm(s1,P8,10), perm(s3,P8,10))
def fk(d,sk):
    l=(d>>4)&15; r=d&15
    e=perm(r,EP,4)^sk
    lp=(e>>4)&15; rp=e&15
    r0=((lp>>3)&1)*2+(lp&1); c0=((lp>>2)&1)*2+((lp>>1)&1)
    r1=((rp>>3)&1)*2+(rp&1); c1=((rp>>2)&1)*2+((rp>>1)&1)
    so=(S0[r0][c0]<<2)|S1[r1][c1]
    return ((l^perm(so,P4,4))<<4)|r
def enc(p,k):
    k1,k2=subkeys(k)
    a=perm(p,IP,8); a=fk(a,k1); a=((a&15)<<4)|((a>>4)&15); a=fk(a,k2)
    return perm(a,IPi,8)
k1,k2=subkeys(0b1010011010)
print("K1=%s K2=%s"%(bin(k1)[2:].zfill(8),bin(k2)[2:].zfill(8)))
c=enc(0b10010111,0b1010011010)
print("enc(10010111,1010011010)=%s"%bin(c)[2:].zfill(8))
