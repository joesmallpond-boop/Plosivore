#include "../Source/PlosiveEngine.h"
#include <cstdio>
#include <cstdlib>
int main(int argc,char**argv){
  // args: in out sens freq depth rel look
  FILE*f=fopen(argv[1],"rb"); std::vector<float> il; float b[2];
  while(fread(b,4,2,f)==2){il.push_back(b[0]);il.push_back(b[1]);} fclose(f);
  int n=il.size()/2; std::vector<float> L(n),R(n);
  for(int i=0;i<n;i++){L[i]=il[2*i];R[i]=il[2*i+1];}
  plosivore::Engine e; plosivore::Params p; p.sensitivity=atof(argv[3]); p.frequency=atof(argv[4]);
  p.depthDb=atof(argv[5]); p.releaseMs=atof(argv[6]); p.lookaheadMs=atof(argv[7]);
  if(argc>8){e.kSlope=atof(argv[8]);e.kLowRel=atof(argv[9]);e.kHold=atof(argv[10]); if(argc>11) e.kChestAmount=atof(argv[11]);}
  e.prepare(48000,2); e.setParams(p); e.updateCrossover(p.frequency); e.updateCrossover(p.frequency);
  for(int k=0;k<40;k++) e.updateCrossover(p.frequency);
  int lat=e.getLatencySamples(); int frames=0, ducked=0, deep=0;
  for(int i=0;i<n;i+=256){int m=std::min(256,n-i); float*d[2]={&L[i],&R[i]}; e.process(d,2,m,[&](const plosivore::Frame& fr){frames++; if(fr.grDb<-3)ducked++; if(fr.grDb<-10)deep++;});}
  FILE*o=fopen(argv[2],"wb"); for(int i=lat;i<n;i++){fwrite(&L[i],4,1,o);fwrite(&R[i],4,1,o);} for(int i=0;i<lat;i++){float z[2]={0,0};fwrite(z,4,2,o);} fclose(o);
  printf("latency %d samples, frames %d, ducked>3dB %.2f%%, >10dB %.2f%%\n",lat,frames,100.0*ducked/frames,100.0*deep/frames);
}
