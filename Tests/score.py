import numpy as np, json, sys, scipy.signal as ss
d=sys.argv[1]; m=json.load(open(d+'/meta.json')); fs=m['fs']
ld=lambda f: np.fromfile(d+'/'+f,np.float32).reshape(-1,2)[:,0]
x,c,y=ld('ref.f32'),ld('cleanref.f32'),ld(sys.argv[2] if len(sys.argv)>2 else 'out.f32')  # ref = engine at depth 0 (same allpass phase)
lp=ss.butter(4,200,'lp',fs=fs,output='sos'); db=lambda a:10*np.log10(np.sum(a**2)+1e-12)
mask=np.zeros(len(x),bool); red=[]
for p in m['pops']:
    i=int(p['start']*fs); L=int(p['len']*fs); mask[i:i+L+int(.01*fs)]=True
    a=ss.sosfilt(lp,(x-c)[i-480:i+L+480]); b=ss.sosfilt(lp,(y-c)[i-480:i+L+480]); red.append(db(a)-db(b))
red=np.array(red)
print(f"pop LF residue removed: median {np.median(red):.1f} dB, worst {red.min():.1f} dB, best {red.max():.1f} dB")
# collateral on clean material away from pops
t0=0
for name,dur in m['parts']:
    s=slice(int(t0*fs),int((t0+dur)*fs)); mm=~mask[s]
    err=(y-x)[s][mm]; print(f"  {name:9s} collateral (change to non-pop audio): {db(err)-db(x[s][mm]):6.1f} dB rel signal")
    t0+=dur
peak=lambda a:20*np.log10(np.max(np.abs(a)))
print(f"peak in {peak(x):.1f} dBFS -> out {peak(y):.1f} dBFS (clean ref {peak(c):.1f})")
