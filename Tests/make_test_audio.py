import numpy as np, scipy.signal as ss, sys, json
fs=48000; rng=np.random.default_rng(7)
def voice(dur, f0s, syl_rate, seed):
    r=np.random.default_rng(seed); n=int(dur*fs); t=np.arange(n)/fs
    f0=np.interp(t, np.linspace(0,dur,len(f0s)), f0s)*(1+0.02*np.sin(2*np.pi*5*t))
    ph=np.cumsum(f0/fs); src=np.zeros(n); src[np.where(np.diff(np.floor(ph))>0)[0]]=1
    src=ss.lfilter([1],[1,-0.97],src)              # glottal-ish tilt
    out=np.zeros(n)
    for F,B,g in [(700,110,1),(1220,120,.6),(2600,160,.3),(3400,250,.15)]:
        b,a=ss.iirpeak(F,F/B,fs); out+=g*ss.lfilter(b,a,src)
    # syllable envelope + fricative noise
    env=np.zeros(n); onsets=[]
    k=0.05
    while k<dur-0.15:
        L=r.uniform(.08,.14); i0=int(k*fs); i1=min(n,int((k+L)*fs))
        w=np.hanning(i1-i0)**0.6; env[i0:i1]=np.maximum(env[i0:i1],w); onsets.append(k); k+=1/syl_rate*r.uniform(.8,1.2)
    hiss=ss.lfilter(*ss.butter(4,4000,'hp',fs=fs),r.standard_normal(n))*0.05
    v=out*env + hiss*np.roll(env,int(.02*fs))*(r.random(n)>0)
    return v/np.max(np.abs(v))*0.35, onsets
def pop(kind):
    L=int(fs*rng.uniform(.04,.09)); t=np.arange(L)/fs
    f=rng.uniform(25,70) if kind=='capsule' else rng.uniform(40,90)
    thump=np.sin(2*np.pi*f*t*(1-0.3*t/t[-1]))*np.exp(-t/(L/fs/2.5))*np.minimum(1,t/0.002)
    rumble=ss.lfilter(*ss.butter(2,120,fs=fs),rng.standard_normal(L))*np.exp(-t/0.02)
    p=thump+0.6*rumble/np.max(np.abs(rumble))
    return p/np.max(np.abs(p))*rng.uniform(.5,.95), L
segs=[];truth=[]
# rap: deep male, fast (8 syl/s), high-pitched fast, slow sung low notes (no pops in a sustained bit)
parts=[('rap_deep',4,[95,110,100,120],8,1),('rap_high',3,[200,240,210],9,2),('sung_low',3,[82,87,82],2,3),('rap_mid',4,[140,160,150],7,4)]
clean=[];dirty=[];t0=0;meta=[]
for name,d,f0s,rate,seed in parts:
    v,ons=voice(d,f0s,rate,seed); pv=v.copy()
    if name!='sung_low':
        for o in ons[::2]:   # every other syllable is a plosive (very p/b-heavy verse)
            kind='capsule' if rng.random()<.3 else 'breath'
            p,L=pop(kind); i=int(o*fs); i=min(i,len(pv)-L); pv[i:i+L]+=p
            meta.append(dict(start=(t0+i)/fs,len=L/fs,kind=kind,part=name))
    clean.append(v);dirty.append(pv);t0+=len(v)
clean=np.concatenate(clean).astype(np.float32);dirty=np.concatenate(dirty).astype(np.float32)
sig=np.clip(dirty,-1,1)
np.stack([sig,sig*0.97]).T.astype(np.float32).tofile(sys.argv[1]+'/in.f32')
np.stack([clean,clean*0.97]).T.astype(np.float32).tofile(sys.argv[1]+'/clean.f32')
json.dump(dict(fs=fs,pops=meta,parts=[(p[0],p[1]) for p in parts]),open(sys.argv[1]+'/meta.json','w'))
print(len(meta),'pops,',len(clean)/fs,'s')
