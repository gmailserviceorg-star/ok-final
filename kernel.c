/* ClaudeOS stage-1 kernel: GRUB multiboot, VBE framebuffer, PS/2, PIT, RTC, ATA PIO, RAM filesystem with disk persistence */
#include "font.h"
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define NULL ((void*)0)
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8 inb(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outw(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void insw(u16 p,void*b,u32 n){__asm__ volatile("rep insw":"+D"(b),"+c"(n):"d"(p):"memory");}
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32 inl(u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outsw(u16 p,const void*b,u32 n){__asm__ volatile("rep outsw":"+S"(b),"+c"(n):"d"(p));}
void*memcpy(void*d,const void*s,u32 n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}
void*memmove(void*d,const void*s,u32 n){u8*a=d;const u8*b=s;if(a<b)while(n--)*a++=*b++;else{a+=n;b+=n;while(n--)*--a=*--b;}return d;}
void*memset(void*d,int c,u32 n){u8*a=d;while(n--)*a++=(u8)c;return d;}
static u32 strlen(const char*s){u32 n=0;while(s[n])n++;return n;}
static int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return (u8)*a-(u8)*b;}
static int strncmp(const char*a,const char*b,u32 n){while(n&&*a&&*a==*b){a++;b++;n--;}return n?(u8)*a-(u8)*b:0;}
static void scp(char*d,const char*s,int m){int i=0;while(s[i]&&i<m-1){d[i]=s[i];i++;}d[i]=0;}
static int lc(int c){return c>='A'&&c<='Z'?c+32:c;}
static const char*sstr(const char*h,const char*n){u32 l=strlen(n);for(;*h;h++)if(!strncmp(h,n,l))return h;return NULL;}
static const char*isstr(const char*h,const char*n){u32 l=strlen(n);for(;*h;h++){u32 i=0;while(i<l&&h[i]&&lc(h[i])==lc(n[i]))i++;if(i==l)return h;}return NULL;}
static void utoa(u32 v,char*b){char t[12];int i=0;if(!v)t[i++]='0';while(v){t[i++]='0'+v%10;v/=10;}int j=0;while(i)b[j++]=t[--i];b[j]=0;}
static void cat(char*d,const char*s){scp(d+strlen(d),s,1000);}
static void catn(char*d,u32 v){char t[12];utoa(v,t);cat(d,t);}

/* ---------- time: PIT + RTC ---------- */
static u32 pit_last,pit_acc,msec;
static u32 now(void){outb(0x43,0);u32 c=inb(0x40);c|=(u32)inb(0x40)<<8;pit_acc+=(pit_last-c)&0xFFFF;pit_last=c;while(pit_acc>=1193){pit_acc-=1193;msec++;}return msec;}
static void wait(u32 ms){u32 t=now();while(now()-t<ms);}
static u32 beep_off;static int SNDON=1;
static void beep(int f,int ms){if(!SNDON||f<=0)return;u32 d=1193180/f;outb(0x43,0xB6);outb(0x42,d&255);outb(0x42,d>>8);outb(0x61,inb(0x61)|3);beep_off=now()+ms;if(!beep_off)beep_off=1;}
static u8 rtc(u8 r){outb(0x70,r);return inb(0x71);}
static void rtc_time(int*h,int*m,int*s){int n=0;while((rtc(0x0A)&0x80)&&n++<100000);u8 a=rtc(0),b=rtc(2),c=rtc(4),f=rtc(0x0B);if(!(f&4)){a=(a&15)+(a>>4)*10;b=(b&15)+(b>>4)*10;c=(c&15)+((c&0x70)>>4)*10;}*s=a;*m=b;*h=c;}

/* ---------- graphics ---------- */
static u32*FB;static u32 BB[1280*1024];static int W=1024,H=768,FPITCH;
static int cx0,cy0,cx1,cy1;
static void clip(int a,int b,int c,int d){cx0=a<0?0:a;cy0=b<0?0:b;cx1=c>W?W:c;cy1=d>H?H:d;}
static u32 mix(u32 a,u32 b,int t){u32 r=(((a>>16)&255)*(256-t)+((b>>16)&255)*t)>>8,g=(((a>>8)&255)*(256-t)+((b>>8)&255)*t)>>8,l=((a&255)*(256-t)+(b&255)*t)>>8;return r<<16|g<<8|l;}
static void hline(int x,int y,int w,u32 c,int a){if(y<cy0||y>=cy1)return;int x1=x+w;if(x<cx0)x=cx0;if(x1>cx1)x1=cx1;u32*p=BB+y*W;if(a>=256)for(;x<x1;x++)p[x]=c;else if(a>0)for(;x<x1;x++)p[x]=mix(p[x],c,a);}
static void arect(int x,int y,int w,int h,u32 c,int a){for(int j=0;j<h;j++)hline(x,y+j,w,c,a);}
static void rect(int x,int y,int w,int h,u32 c){arect(x,y,w,h,c,256);}
static int isq(int v){int r=0;while((r+1)*(r+1)<=v)r++;return r;}
static void rrect(int x,int y,int w,int h,int r,u32 c,int a){if(r*2>h)r=h/2;for(int j=0;j<h;j++){int dy=j<r?r-j:(j>=h-r?j-(h-r)+1:0),in=dy?r-isq(r*r-dy*dy):0;hline(x+in,y+j,w-2*in,c,a);}}
static void px(int x,int y,u32 c){if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1)BB[y*W+x]=c;}
static void bpx(int x,int y,u32 c,int a){if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1)BB[y*W+x]=mix(BB[y*W+x],c,a+(a>>7));}
static void chr(int x,int y,int c,u32 col,int s){c&=127;if(s<=1){for(int j=0;j<16;j++)for(int i=0;i<8;i++){int a=FONT[c][j][i];if(a)bpx(x+i,y+j,col,a);}}else{for(int j=0;j<16*s;j++)for(int i=0;i<8*s;i++){int a=FONTB[c][j*32/(16*s)][i*16/(8*s)];if(a)bpx(x+i,y+j,col,a);}}}
static int text(int x,int y,const char*s,u32 col,int sc){while(*s){chr(x,y,*s++,col,sc);x+=8*sc;}return x;}
static u32 PREV[1280*1024];static int prev_ok;
static void present(void){for(int y=0;y<H;y++){u32*d=FB+y*(FPITCH/4),*sp=BB+y*W,*pp=PREV+y*W;int a=0,b=W-1;if(prev_ok){while(a<W&&sp[a]==pp[a])a++;if(a==W)continue;while(sp[b]==pp[b])b--;}else a=0;int n=b-a+1;u32*dd=d+a;const u32*ss=sp+a;int nn=n;__asm__ volatile("rep movsl":"+D"(dd),"+S"(ss),"+c"(nn)::"memory");memcpy(pp+a,sp+a,n*4);}prev_ok=1;}

enum{S_BOOT,S_LOGIN,S_DESK,S_OFF,S_SETUP};static int state,ov,startopen;
#define NSTEP 10
typedef struct{int step,kb,kb2,aL,aP,wall,acc,feat[4],field,fin,scroll,scrolled,repair,snap,minmin;u32 ms;u32 t0;char user[24],pass[32],hint[48],msg[48];}Setup;
static Setup SD;typedef struct{int open,x,y,n,tgt,dir,id[6];char t[6][22];}Ctx;static Ctx CM;static int cmoved;static void ser(const char*s);static int kbcur;static struct{int x,y,w,h,id;}HR[48];static int nhr;static char UHINT[48];
static int lay_chr(int c,int L){if(!L)return c;int up=c>='A'&&c<='Z',l=up?c+32:c;const char*a="",*b="";if(L==1){a="yz";b="zy";}else if(L==2){a="aqzwm;,";b="qawz,m;";}else if(L==3){a="qwertyuiop[]asdfghjkl;'zxcvbnm,./";b="',.pyfgcrl/=aoeuidhtns-;qjkxbmwvz";}for(int i=0;a[i];i++)if(a[i]==l)return up?b[i]-32*(b[i]>='a'&&b[i]<='z'):b[i];return c;}
/* ---------- config ---------- */
static u32 WP0=0x2e2420,WP1=0x161616;static int FEATF[4]={1,1,1,1};static u32 ACC=0xd97757,BG=0x212121,PANEL=0x171717,SURF=0x2f2f2f,TXT=0xececec,BRD=0x383838,MUT=0x9b9b9b;
static int RAD=12,TBH=40,WTB=28,WRAD=10,AN_OPEN=190,AN_FROM=86,AN_CLOSE=140,AN_MAX=220,SHOWCLK=1;static char STARTLBL[24]="ClaudeOS";
static const char*kfind(const char*t,const char*k){int l=strlen(k);for(;*t;t++)if(!strncmp(t,k,l)){const char*q=t+l;while(*q==' '||*q=='"'||*q==':'||*q=='=')q++;return q;}return NULL;}
static int jnum(const char*t,const char*k,int d){const char*q=kfind(t,k);if(!q||*q<'0'||*q>'9')return d;int v=0;while(*q>='0'&&*q<='9')v=v*10+*q++-'0';return v;}
static u32 jcol(const char*t,const char*k,u32 d){const char*q=kfind(t,k);if(!q||*q!='#')return d;q++;u32 v=0;for(int i=0;i<6;i++){int c=lc(q[i]);v=v*16+(c>='a'?c-'a'+10:c-'0');}return v;}
static void jstr(const char*t,const char*k,char*o,int m,const char*d){int l=strlen(k);const char*q=NULL;for(const char*p=t;*p;p++)if(!strncmp(p,k,l)){q=p+l;break;}if(q){while(*q=='"')q++;while(*q==' ')q++;if(*q==':')q++;while(*q==' ')q++;if(*q=='"'){q++;int i=0;while(*q&&*q!='"'&&i<m-1)o[i++]=*q++;o[i]=0;return;}}scp(o,d,m);}

/* ---------- filesystem ---------- */
typedef struct{char name[40];int par;u8 type,used;char*d;u32 len,cap;}Nd;
#define NN 400
static Nd N[NN];static char ARENA[6<<20];static u32 aoff;static int fsdirty;
static void*aal(u32 n){n=(n+15)&~15u;if(aoff+n>sizeof ARENA)return NULL;void*p=ARENA+aoff;aoff+=n;return p;}
static int fchild(int par,const char*nm){for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==par&&!strcmp(N[i].name,nm))return i;return -1;}
static int fnew(int par,const char*nm,int ty){for(int i=1;i<NN;i++)if(!N[i].used){memset(&N[i],0,sizeof N[i]);N[i].used=1;N[i].par=par;N[i].type=ty;scp(N[i].name,nm,40);fsdirty=1;return i;}return -1;}
static int fsp(const char*p,int cwd){int c=*p=='/'?0:cwd;char t[40];while(*p){while(*p=='/')p++;if(!*p)break;int l=0;while(*p&&*p!='/'&&l<39)t[l++]=*p++;t[l]=0;if(!strcmp(t,"."))continue;if(!strcmp(t,"..")){if(c>0)c=N[c].par;continue;}c=fchild(c,t);if(c<0)return -1;}return c;}
static int fmk(const char*p,int cwd,int ty){int c=*p=='/'?0:cwd;char t[40];while(*p){while(*p=='/')p++;if(!*p)break;int l=0;while(*p&&*p!='/'&&l<39)t[l++]=*p++;t[l]=0;int last=1;for(const char*q=p;*q;q++)if(*q!='/'){last=0;break;}int n=fchild(c,t);if(n<0)n=fnew(c,t,last?ty:1);if(n<0)return -1;c=n;}return c;}
static void fwrite_(int n,const char*s,u32 len){Nd*d=&N[n];if(len+1>d->cap){u32 nc=len+256;char*b=aal(nc);if(!b)return;if(d->d)memcpy(b,d->d,d->len);d->d=b;d->cap=nc;}memcpy(d->d,s,len);d->d[len]=0;d->len=len;fsdirty=1;}
static void fapp(int n,const char*s,u32 len){Nd*d=&N[n];char*old=d->d;u32 ol=d->len;if(ol+len+1>d->cap){u32 nc=ol+len+256;char*b=aal(nc);if(!b)return;if(old)memcpy(b,old,ol);d->d=b;d->cap=nc;}memcpy(d->d+ol,s,len);d->len=ol+len;d->d[d->len]=0;fsdirty=1;}
static void frm(int n){if(n<=0)return;for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==n)frm(i);N[n].used=0;fsdirty=1;}
static void fp_r(int n,char*b){if(n<=0)return;fp_r(N[n].par,b);int l=strlen(b);b[l]='/';scp(b+l+1,N[n].name,40);}
static void fpath(int n,char*b){b[0]=0;if(n<=0){b[0]='/';b[1]=0;return;}fp_r(n,b);}
static const char*fdata(const char*p){int n=fsp(p,0);return n>0&&N[n].d?N[n].d:"";}
static void put(const char*p,const char*c){int n=fmk(p,0,2);if(n>0)fwrite_(n,c,strlen(c));}
static const struct{const char*p,*c;}SEED[]={
{"/System/Configuration/Theme.json","{\n  \"accent\": \"#d97757\",\n  \"bg\": \"#212121\",\n  \"panel\": \"#171717\",\n  \"surface\": \"#2f2f2f\",\n  \"text\": \"#ececec\",\n  \"border\": \"#383838\",\n  \"radius\": 12\n}\n"},
{"/System/Configuration/Taskbar.json","{\n  \"height\": 40,\n  \"showClock\": 1,\n  \"startLabel\": \"ClaudeOS\"\n}\n"},
{"/System/Configuration/Windows.json","{\n  \"titleBarHeight\": 28,\n  \"radius\": 10,\n  \"animation\": \"open:scale_from=86,fade=in,open_ms=190,ease=cubic-out,origin=center;close:scale_to=92,fade=out,close_ms=140,ease=cubic-in,origin=center;maximize:max_ms=220,ease=cubic-out,interpolate=rect;restore:max_ms=220,ease=cubic-out,interpolate=rect;minimize:min_ms=160,ease=quad-in;focus:glow_ms=120,ease=linear;drag:smoothing=none,snap=off;shadow:layers=3,alpha=22,spread=6\"\n}\n"},
{"/System/Configuration/Users.json","{\n  \"users\": [{\"name\": \"claude\", \"password\": \"claude\"}]\n}\n"},
{"/System/PRIVILEGE.json","{\n  \"rules\": [\n    {\"from\": \"Terminal\", \"to\": \"SYSTEM\", \"spot\": \"/System/*\", \"allow\": true},\n    {\"from\": \"root\", \"to\": \"SYSTEM\", \"spot\": \"/*\", \"allow\": true}\n  ]\n}\n"},
{"/Desktop/Readme.txt","Welcome to ClaudeOS (bare metal).\nSelect an icon and press Delete to remove it instantly.\nShift+F10 admin terminal, Shift+F11 reset password, Shift+F12 recovery.\nCtrl+X shut down, hold Ctrl+X 2 seconds to restart.\n"},
{"/Documents/welcome.txt","This file lives in the ClaudeOS filesystem.\nIt is saved to a blank or ClaudeOS-formatted ATA disk on shutdown.\n"},
{"/www/home.html","<html><head><title>Claudebrowse Home</title></head><body><h1>Claudebrowse</h1><p>A browser and search engine written for ClaudeOS. Type words in the address bar and press Enter to search every page in /www.</p><h2>Pages</h2><li><a href=\"about.html\">About ClaudeOS</a></li><li><a href=\"kernel.html\">How the kernel works</a></li><li><a href=\"search:claude\">Search for claude</a></li></body></html>"},
{"/www/about.html","<html><head><title>About ClaudeOS</title></head><body><h1>About ClaudeOS</h1><p>ClaudeOS boots from <b>GRUB</b>, draws to a VBE framebuffer and reads the <b>PS/2</b> keyboard and mouse directly. Files live in a RAM filesystem saved to an ATA disk.</p><p><a href=\"home.html\">Back to home</a></p></body></html>"},
{"/www/kernel.html","<html><head><title>Kernel notes</title></head><body><h1>Kernel notes</h1><p>Drivers: framebuffer, PS/2 keyboard, PS/2 mouse, PIT timer, CMOS RTC, ATA PIO disk. The browser parses HTML tags itself and ranks pages for the search engine by counting words.</p><p><a href=\"home.html\">Back to home</a></p></body></html>"},
{"/System/Configuration/Audio.json","{\n  \"enabled\": 1\n}\n"}};
#define NSEED (int)(sizeof SEED/sizeof SEED[0])
static void seed_dirs(void){const char*d[]={"/System/Configuration","/System/Data","/Desktop","/Documents","/www","/tmp"};for(int i=0;i<6;i++)fmk(d[i],0,1);}
static int is_json(const char*p){u32 l=strlen(p);return l>5&&!strcmp(p+l-5,".json");}
static void seed_all(int force){seed_dirs();for(int i=0;i<NSEED;i++){int n=fsp(SEED[i].p,0);int bad=n<0||(is_json(SEED[i].p)&&(!N[n].d||!sstr(N[n].d,"{")));if(force||bad)put(SEED[i].p,SEED[i].c);}}

/* ---------- ATA PIO driver + persistence (only touches blank or CLFS disks) ---------- */
static u16 ab;static int adr=-1,disk_ok,disk_found;static char DISKBUF[2<<20];
static int ata_bsy(u16 b){for(int i=0;i<400000;i++)if(!(inb(b+7)&0x80))return 1;return 0;}
static int ata_drq(u16 b){for(int i=0;i<400000;i++){u8 s=inb(b+7);if(!(s&0x80)&&(s&8))return 1;if(s&1)return 0;}return 0;}
static int ata_ident(u16 b,int sl){if(inb(b+7)==0xFF)return 0;outb(b+6,0xA0|sl<<4);outb(b+2,0);outb(b+3,0);outb(b+4,0);outb(b+5,0);outb(b+7,0xEC);if(!inb(b+7))return 0;if(!ata_bsy(b))return 0;if(inb(b+4)||inb(b+5))return 0;if(!ata_drq(b))return 0;u16 id[256];insw(b,id,256);return 1;}
static int ata_rw(u32 lba,void*buf,int wr){u16 b=ab;if(!ata_bsy(b))return 0;outb(b+6,0xE0|adr<<4|((lba>>24)&15));outb(b+2,1);outb(b+3,lba);outb(b+4,lba>>8);outb(b+5,lba>>16);outb(b+7,wr?0x30:0x20);if(!ata_drq(b))return 0;if(wr){outsw(b,buf,256);outb(b+7,0xE7);ata_bsy(b);}else insw(b,buf,256);return 1;}
static int ata_probe(void){static const u16 bs[2]={0x1F0,0x170};for(int i=0;i<2;i++)for(int s=0;s<2;s++)if(ata_ident(bs[i],s)){ab=bs[i];adr=s;disk_found=1;char sec[512];if(!ata_rw(1,sec,0))continue;int blank=1;for(int k=0;k<512;k++)if(sec[k]){blank=0;break;}if(blank||!strncmp(sec,"CLFS",4)){disk_ok=1;return !blank;}adr=-1;}return 0;}
/* ---------- PCI + AHCI (SATA) driver ---------- */
static struct{int b,d,f;u16 ven,dev;u8 cls,sub,pi;}PCI[48];static int npci;
static u32 pci_rd(int b,int d,int f,int o){outl(0xCF8,0x80000000u|(u32)b<<16|(u32)d<<11|(u32)f<<8|(o&0xFC));return inl(0xCFC);}
static void pci_wr(int b,int d,int f,int o,u32 v){outl(0xCF8,0x80000000u|(u32)b<<16|(u32)d<<11|(u32)f<<8|(o&0xFC));outl(0xCFC,v);}
static void pci_scan(void){npci=0;for(int b=0;b<8;b++)for(int d=0;d<32;d++){u32 id=pci_rd(b,d,0,0);if((id&0xFFFF)==0xFFFF)continue;int mf=(pci_rd(b,d,0,0x0C)>>16)&0x80;for(int f=0;f<(mf?8:1);f++){id=pci_rd(b,d,f,0);if((id&0xFFFF)==0xFFFF)continue;u32 cl=pci_rd(b,d,f,8);if(npci<48){PCI[npci].b=b;PCI[npci].d=d;PCI[npci].f=f;PCI[npci].ven=id&0xFFFF;PCI[npci].dev=id>>16;PCI[npci].cls=cl>>24;PCI[npci].sub=cl>>16;PCI[npci].pi=cl>>8;npci++;}}}}
typedef struct{u32 fl,prdbc,ctba,ctbau,r[4];}CmdH;
static CmdH CLB[32] __attribute__((aligned(1024)));static u8 FRX[256] __attribute__((aligned(256)));static u8 BNC[512] __attribute__((aligned(16)));
static struct{u8 cfis[64],acmd[16],r[48];u32 dba,dbau,rr,dbc;}CTB __attribute__((aligned(128)));
static volatile u32*HBA;static int ahp=-1,use_ahci;
#define PR(r) (HBA[(0x100+ahp*0x80+(r))/4])
static int spin(volatile u32*p,u32 mask,u32 val,int n){while(n--){if((*p&mask)==val)return 1;}return 0;}
static int ahci_cmd(u32 lba,void*buf,int wr){if(ahp<0)return 0;if(wr)memcpy(BNC,buf,512);if(!spin(&PR(0x20),0x88,0,2000000))return 0;memset(&CLB[0],0,sizeof(CmdH));CLB[0].fl=5|(wr?0x40:0)|(1u<<16);CLB[0].ctba=(u32)(unsigned long)&CTB;memset(&CTB,0,sizeof CTB);u8*f=CTB.cfis;f[0]=0x27;f[1]=0x80;f[2]=wr?0x35:0x25;f[7]=0x40;f[4]=lba;f[5]=lba>>8;f[6]=lba>>16;f[8]=lba>>24;f[12]=1;CTB.dba=(u32)(unsigned long)BNC;CTB.dbc=511;PR(0x10)=0xFFFFFFFF;PR(0x38)=1;int n=3000000;while(n>0){if(!(PR(0x38)&1))break;if(PR(0x10)&(1u<<30))return 0;n--;}if(n<=0)return 0;if(PR(0x10)&(1u<<30))return 0;if(!wr)memcpy(buf,BNC,512);return 1;}
static int ahci_port_init(int p){ahp=p;PR(0x18)&=~0x11u;spin(&PR(0x18),0xC000,0,1000000);memset(CLB,0,sizeof CLB);memset(FRX,0,sizeof FRX);PR(0x00)=(u32)(unsigned long)CLB;PR(0x04)=0;PR(0x08)=(u32)(unsigned long)FRX;PR(0x0C)=0;PR(0x30)=0xFFFFFFFF;PR(0x10)=0xFFFFFFFF;PR(0x18)|=0x10;PR(0x18)|=1;return spin(&PR(0x20),0x88,0,2000000);}
static int ahci_probe(void){pci_scan();for(int i=0;i<npci;i++)if(PCI[i].cls==1&&PCI[i].sub==6&&PCI[i].pi==1){int b=PCI[i].b,d=PCI[i].d,f=PCI[i].f;u32 bar=pci_rd(b,d,f,0x24)&~0xFu;if(!bar)continue;pci_wr(b,d,f,4,pci_rd(b,d,f,4)|6);HBA=(volatile u32*)(unsigned long)bar;HBA[1]|=1u<<31;u32 pim=HBA[3];for(int p=0;p<32;p++){if(!(pim&(1u<<p)))continue;volatile u32*pp=&HBA[(0x100+p*0x80)/4];if((pp[0x28/4]&15)!=3||pp[0x24/4]!=0x101)continue;if(!ahci_port_init(p))continue;u8 sec[512];if(!ahci_cmd(1,sec,0))continue;int blank=1;for(int k=0;k<512;k++)if(sec[k]){blank=0;break;}disk_found=1;if(blank||!strncmp((char*)sec,"CLFS",4)){use_ahci=1;disk_ok=1;return !blank;}ahp=-1;}}return 0;}
static int dio(u32 lba,void*b,int wr){return use_ahci?ahci_cmd(lba,b,wr):ata_rw(lba,b,wr);}
typedef struct{char name[40];int id,par;u8 type,pad[3];u32 len;}Rec;
static int disk_save(void){if(!disk_ok)return 0;u32 o=0;for(int i=0;i<NN;i++)if(N[i].used){Rec r;memset(&r,0,sizeof r);scp(r.name,N[i].name,40);r.id=i;r.par=N[i].par;r.type=N[i].type;r.len=N[i].len;if(o+sizeof r+r.len>sizeof DISKBUF)return 0;memcpy(DISKBUF+o,&r,sizeof r);o+=sizeof r;if(r.len){memcpy(DISKBUF+o,N[i].d,r.len);o+=r.len;}}char sec[512];memset(sec,0,512);memcpy(sec,"CLFS",4);memcpy(sec+4,&o,4);u32 ns=(o+511)/512;for(u32 s=0;s<ns;s++){memset(sec,0,512);u32 n=o-s*512;memcpy(sec,DISKBUF+s*512,n>512?512:n);if(!dio(2+s,sec,1))return 0;}memset(sec,0,512);memcpy(sec,"CLFS",4);memcpy(sec+4,&o,4);if(!dio(1,sec,1))return 0;fsdirty=0;return 1;}
static int disk_load(void){char sec[512];if(!dio(1,sec,0)||strncmp(sec,"CLFS",4))return 0;u32 o;memcpy(&o,sec+4,4);if(o>sizeof DISKBUF)return 0;u32 ns=(o+511)/512;for(u32 s=0;s<ns;s++){if(!dio(2+s,sec,0))return 0;u32 n=o-s*512;memcpy(DISKBUF+s*512,sec,n>512?512:n);}memset(N,0,sizeof N);aoff=0;u32 p=0;while(p+sizeof(Rec)<=o){Rec r;memcpy(&r,DISKBUF+p,sizeof r);p+=sizeof r;if(r.id<0||r.id>=NN)return 0;Nd*d=&N[r.id];d->used=1;scp(d->name,r.name,40);d->par=r.par;d->type=r.type;if(r.len){d->d=aal(r.len+256);d->cap=r.len+256;memcpy(d->d,DISKBUF+p,r.len);d->d[r.len]=0;d->len=r.len;p+=r.len;}}N[0].used=1;N[0].par=-1;N[0].type=1;return 1;}

/* ---------- config load ---------- */
static int cfg_ok[4];static char UNAME[24],UPASS[40];
static void load_cfg(void){const char*t=fdata("/System/Configuration/Theme.json"),*b=fdata("/System/Configuration/Taskbar.json"),*w=fdata("/System/Configuration/Windows.json"),*u=fdata("/System/Configuration/Users.json");
 cfg_ok[0]=*t=='{';cfg_ok[1]=*b=='{';cfg_ok[2]=*w=='{';cfg_ok[3]=*u=='{';
 ACC=jcol(t,"accent",0xd97757);BG=jcol(t,"\"bg\"",0x212121);PANEL=jcol(t,"panel",0x171717);SURF=jcol(t,"surface",0x2f2f2f);TXT=jcol(t,"\"text\"",0xececec);BRD=jcol(t,"border",0x383838);RAD=jnum(t,"radius",12);SNDON=jnum(fdata("/System/Configuration/Audio.json"),"enabled",1);WP0=jcol(t,"wallTop",0x2e2420);WP1=jcol(t,"wallBottom",0x161616);{const char*fe=fdata("/System/Configuration/Features.json");FEATF[0]=jnum(fe,"files",1);FEATF[1]=jnum(fe,"terminal",1);FEATF[2]=jnum(fe,"editor",1);FEATF[3]=jnum(fe,"browser",1);const char*kb=fdata("/System/Configuration/Keyboard.json");SD.kb=jnum(kb,"layout",0);SD.kb2=jnum(kb,"second",0);if(SD.kb<0||SD.kb>3)SD.kb=0;if(SD.kb2<0||SD.kb2>4)SD.kb2=0;}
 TBH=jnum(b,"height",40);SHOWCLK=jnum(b,"showClock",1);jstr(b,"startLabel",STARTLBL,24,"ClaudeOS");
 WTB=jnum(w,"titleBarHeight",28);WRAD=jnum(w,"\"radius\"",10);AN_OPEN=jnum(w,"open_ms",190);AN_FROM=jnum(w,"scale_from",86);AN_CLOSE=jnum(w,"close_ms",140);AN_MAX=jnum(w,"max_ms",220);
 jstr(u,"\"name\"",UNAME,24,"claude");jstr(u,"\"password\"",UPASS,40,"claude");jstr(u,"hint",UHINT,48,"");}

/* ---------- input drivers (polling) ---------- */
#define K_UP 0x101
#define K_DN 0x102
#define K_LT 0x103
#define K_RT 0x104
#define K_DEL 0x105
static int mx=512,my=384,mb,mpb,shift,ctrl,alt,xheld,xfired;static u32 xt0;static int dirty=1;
static void kw(void){for(int i=0;i<100000&&(inb(0x64)&2);i++);}
static void kr(void){for(int i=0;i<100000&&!(inb(0x64)&1);i++);if(inb(0x64)&1)inb(0x60);}
static void mcmd(u8 c){kw();outb(0x64,0xD4);kw();outb(0x60,c);kr();}
static void ps2_init(void){while(inb(0x64)&1)inb(0x60);kw();outb(0x64,0xA8);kw();outb(0x64,0x20);for(int i=0;i<100000&&!(inb(0x64)&1);i++);u8 cf=inb(0x60);cf|=2;cf&=~0x20;kw();outb(0x64,0x60);kw();outb(0x60,cf);mcmd(0xF6);mcmd(0xF4);while(inb(0x64)&1)inb(0x60);}
static const char KN[]="\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char KS[]="\0\033" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static int hover_hot(void);static void alt_f4(void);static void alt_tab(void);static void sc_open(int t);static void on_key(int c);static void on_special(int sc);static void on_mouse(void);static void do_shutdown(void);
static void key_scan(u8 d){static int e0;if(d==0xE0){e0=1;return;}int up=d&0x80,sc=d&0x7F,ex=e0;e0=0;
 if(!ex&&(sc==0x2A||sc==0x36)){shift=!up;if(!up&&alt&&SD.kb2)kbcur^=1;return;}if(!ex&&sc==0x1D){ctrl=!up;if(up&&xheld){xheld=0;if(!xfired)do_shutdown();}return;}if(sc==0x38){alt=!up;if(!up&&shift&&SD.kb2)kbcur^=1;return;}
 if(!ex&&sc==0x2D&&ctrl){if(!up&&!xheld){xheld=1;xfired=0;xt0=now();}else if(up&&xheld){xheld=0;if(!xfired)do_shutdown();}return;}
 if(up)return;dirty=1;
 if(state==S_DESK&&!ov){if(alt&&sc==0x3E){alt_f4();return;}if(alt&&sc==0x0F){alt_tab();return;}if(ctrl&&alt&&sc==0x14){sc_open(0);return;}if(ctrl&&alt&&sc==0x21){sc_open(1);return;}}
 if(ex){int c=sc==0x48?K_UP:sc==0x50?K_DN:sc==0x4B?K_LT:sc==0x4D?K_RT:sc==0x53?K_DEL:0;if(c)on_key(c);return;}
 if(sc==0x44||sc==0x57||sc==0x58){if(shift)on_special(sc);return;}
 if(sc<sizeof KN-1){int u=KN[sc],c=shift?KS[sc]:u,L=kbcur?SD.kb2-1:SD.kb;if(L>0&&u){int m=lay_chr(u,L);if(m!=u){c=m;if(shift){int k;for(k=0;k<(int)sizeof KN-1;k++)if(KN[k]==m)break;c=k<(int)sizeof KN-1?KS[k]:m;}}}if(c)on_key(c);}}
static void mouse_byte(u8 d){static int i;static u8 p[3];if(!i&&!(d&8))return;p[i++]=d;if(i<3)return;i=0;int dx=p[1]-((p[0]<<4)&0x100),dy=p[2]-((p[0]<<3)&0x100);mx+=dx;my-=dy;if(mx<0)mx=0;if(my<0)my=0;if(mx>=W)mx=W-1;if(my>=H)my=H-1;mpb=mb;mb=p[0]&7;cmoved=1;if(mb!=mpb||mb||startopen||CM.open||state!=S_DESK||ov||hover_hot())dirty=1;on_mouse();}
static void poll_input(void){for(int n=0;n<64;n++){u8 s=inb(0x64);if(!(s&1))break;u8 d=inb(0x60);if(s&0x20)mouse_byte(d);else key_scan(d);}}

/* ---------- windows ---------- */
enum{T_TERM,T_FILES,T_EDIT,T_BROW};
typedef struct{int used,type,x,y,w,h,min,max,z,anim,root;u32 t0;int px,py,pw,ph,a_on;int af[4],at[4];u32 a_t0,a_d;char title[32];char*out;u32 olen;char in[96];int il;int cwd,sel,node,scroll,dirty;char url[64],prev[64];}Win;
#define MAXW 10
static Win WN[MAXW];static char WOUT[MAXW][8192];static int zc,state,ov,dsel=-1,startopen,dragw=-1,dgx,dgy;static Win RW;static char ROUT[8192];

static char pw[40],perr[48],rmsg[256];static int rsel;
static int ease(int p){if(p>256)p=256;int q=256-p;return 256-((q*q*q)>>16);}
static void wp(Win*w,const char*s){u32 l=strlen(s);if(w->olen+l>7600){u32 cut=w->olen/2;memmove(w->out,w->out+cut,w->olen-cut);w->olen-=cut;}memcpy(w->out+w->olen,s,l);w->olen+=l;w->out[w->olen]=0;}
static void user_name(char*b){scp(b,UNAME,24);}
static Win*openw(int type,const char*title,int cwd){for(int i=0;i<MAXW;i++)if(!WN[i].used){Win*w=&WN[i];memset(w,0,sizeof *w);int n=0;for(int k=0;k<MAXW;k++)n+=WN[k].used;w->used=1;w->type=type;w->out=WOUT[i];w->out[0]=0;w->w=type==T_BROW?760:type==T_EDIT?640:600;w->h=type==T_BROW?480:380;if(w->w>W-20)w->w=W-20;if(w->h>H-TBH-20)w->h=H-TBH-20;w->x=40+(n%6)*32;w->y=30+(n%6)*28;w->z=++zc;beep(660,50);w->anim=1;w->t0=now();scp(w->title,title,32);w->cwd=cwd;w->node=-1;w->sel=0;dirty=1;startopen=0;return w;}return NULL;}
static void wclose(Win*w){beep(330,60);w->anim=2;w->t0=now();dirty=1;}
static Win*focused(void){Win*b=NULL;for(int i=0;i<MAXW;i++)if(WN[i].used&&!WN[i].min&&WN[i].anim!=2&&(!b||WN[i].z>b->z))b=&WN[i];return b;}
static void wfocus(Win*w){w->z=++zc;w->min=0;dirty=1;}
static void wmax(Win*w){w->a_on=1;w->a_t0=now();w->a_d=AN_MAX;w->af[0]=w->x;w->af[1]=w->y;w->af[2]=w->w;w->af[3]=w->h;if(!w->max){w->px=w->x;w->py=w->y;w->pw=w->w;w->ph=w->h;w->at[0]=0;w->at[1]=0;w->at[2]=W;w->at[3]=H-TBH;w->max=1;}else{w->at[0]=w->px;w->at[1]=w->py;w->at[2]=w->pw;w->at[3]=w->ph;w->max=0;}}
static int lerp(int a,int b,int e){return a+(b-a)*e/256;}
static int anims(void){int any=0;u32 t=now();for(int i=0;i<MAXW;i++){Win*w=&WN[i];if(!w->used)continue;if(w->a_on){int p=w->a_d?(int)((t-w->a_t0)*256/w->a_d):256,e=ease(p);w->x=lerp(w->af[0],w->at[0],e);w->y=lerp(w->af[1],w->at[1],e);w->w=lerp(w->af[2],w->at[2],e);w->h=lerp(w->af[3],w->at[3],e);if(p>=256)w->a_on=0;any=1;}
 if(w->anim==1){if(t-w->t0>=(u32)AN_OPEN)w->anim=0;any=1;}else if(w->anim==2){if(t-w->t0>=(u32)AN_CLOSE){w->used=0;}any=1;}}return any;}

/* ---------- terminal ---------- */
static int privok(int root){const char*p=fdata("/System/PRIVILEGE.json");const char*k=sstr(p,root?"\"from\": \"root\"":"\"from\": \"Terminal\"");return k&&sstr(k,"\"allow\": true")!=NULL;}
static void cpuv(char*b){u32 a,bx,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(bx),"=c"(c),"=d"(d):"a"(0));memcpy(b,&bx,4);memcpy(b+4,&d,4);memcpy(b+8,&c,4);b[12]=0;}
static void make_skips(void);static void sd_adv(int);static void sd_apply(void);
static u32 MEMKB;static char BOOTLOG[2048];static void do_reboot(void);static Win*browse(const char*u);
static void ls_cmd(Win*w,int d){char t[64];for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==d){scp(t,N[i].name,40);if(N[i].type==1)cat(t,"/");cat(t,"\n");wp(w,t);}}
static int atoi_(const char*s){int v=0;while(*s>='0'&&*s<='9')v=v*10+*s++-'0';return v;}
static void hx(u32 v,int n,char*b){const char*h="0123456789abcdef";for(int i=0;i<n;i++)b[i]=h[(v>>((n-1-i)*4))&15];b[n]=0;}
static void itos(long v,char*b){if(v<0){*b++='-';v=-v;}utoa((u32)v,b);}
static void tree_r(Win*w,int n,int dep){for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==n){char t[80];t[0]=0;for(int k=0;k<dep;k++)cat(t,"|  ");cat(t,"|- ");cat(t,N[i].name);if(N[i].type==1)cat(t,"/");cat(t,"\n");wp(w,t);if(N[i].type==1&&dep<6)tree_r(w,i,dep+1);}}
static const char*cp_;static long ce(void);
static void csk(void){while(*cp_==' ')cp_++;}
static long cf(void){csk();if(*cp_=='('){cp_++;long v=ce();csk();if(*cp_==')')cp_++;return v;}int neg=0;if(*cp_=='-'){neg=1;cp_++;}long v=0;while(*cp_>='0'&&*cp_<='9')v=v*10+*cp_++-'0';return neg?-v:v;}
static long ct(void){long v=cf();for(;;){csk();if(*cp_=='*'){cp_++;v*=cf();}else if(*cp_=='/'){cp_++;long d=cf();v=d?v/d:0;}else if(*cp_=='%'){cp_++;long d=cf();v=d?v%d:0;}else return v;}}
static long ce(void){long v=ct();for(;;){csk();if(*cp_=='+'){cp_++;v+=ct();}else if(*cp_=='-'){cp_++;v-=ct();}else return v;}}
static void lines_cmd(Win*w,int n,int head,int cnt){const char*s=N[n].d?N[n].d:"";int tot=0;for(const char*p=s;*p;p++)if(*p=='\n')tot++;int a=head?0:(tot>cnt?tot-cnt:0),ln=0;while(*s&&ln<a+cnt){const char*e=s;while(*e&&*e!='\n')e++;if(ln>=a){char t[160];int l=e-s;if(l>150)l=150;memcpy(t,s,l);t[l]=0;wp(w,t);wp(w,"\n");}ln++;s=*e?e+1:e;}}
static void run_cmd(Win*w,char*l){char*av[10];int ac=0;char raw[96];scp(raw,l,96);for(char*p=l;*p&&ac<10;){while(*p==' ')p++;if(!*p)break;av[ac++]=p;while(*p&&*p!=' ')p++;if(*p)*p++=0;}if(!ac)return;const char*c=av[0];char t[200];
 if(!strcmp(c,"help"))wp(w,"files   ls cd pwd cat mkdir touch rm echo edit open\nsystem  neofetch date uptime mem df ps sync whoami\nnet     browse [page]   search <words>\npower   exec del <path> -f <2<>'SYSTEM'>dev<null<\n        exec del <path> -f ifjson; 'SYSTEM'\nadmin   passwd <new> (root)   reboot shutdown clear\n");
 else if(!strcmp(c,"clear"))w->olen=0,w->out[0]=0;
 else if(!strcmp(c,"pwd")){fpath(w->cwd,t);cat(t,"\n");wp(w,t);}
 else if(!strcmp(c,"ls")){int d=ac>1?fsp(av[1],w->cwd):w->cwd;if(d<0)wp(w,"no such path\n");else ls_cmd(w,d);}
 else if(!strcmp(c,"cd")){int d=ac>1?fsp(av[1],w->cwd):0;if(d>=0&&N[d].type==1)w->cwd=d;else wp(w,"not a folder\n");}
 else if(!strcmp(c,"cat")){int n=ac>1?fsp(av[1],w->cwd):-1;if(n>0&&N[n].type==2){wp(w,N[n].d?N[n].d:"");if(N[n].len&&N[n].d[N[n].len-1]!='\n')wp(w,"\n");}else wp(w,"cannot read\n");}
 else if(!strcmp(c,"mkdir")&&ac>1)fmk(av[1],w->cwd,1);
 else if(!strcmp(c,"touch")&&ac>1)fmk(av[1],w->cwd,2);
 else if(!strcmp(c,"rm")&&ac>1){int n=fsp(av[1],w->cwd);if(n<=0)wp(w,"no such path\n");else{fpath(n,t);if(!strncmp(t,"/System",7))wp(w,"permission denied: use exec del <path> -f ...\n");else frm(n);}}
 else if(!strcmp(c,"echo")){const char*s=sstr(raw,"echo")+4;while(*s==' ')s++;char txt[96];scp(txt,s,96);int ap=0;char*g=NULL;for(int i=0;txt[i];i++)if(txt[i]=='>'){g=txt+i;break;}if(g){ap=g[1]=='>';char*f=g+1+ap;while(*f==' ')f++;char fn[40];scp(fn,f,40);*g=0;int tl=strlen(txt);while(tl&&txt[tl-1]==' ')txt[--tl]=0;int n=fmk(fn,w->cwd,2);if(n>0){cat(txt,"\n");if(ap)fapp(n,txt,strlen(txt));else fwrite_(n,txt,strlen(txt));}}else{wp(w,txt);wp(w,"\n");}}
 else if(!strcmp(c,"edit")&&ac>1){int n=fmk(av[1],w->cwd,2);if(n>0){fpath(n,t);Win*e=openw(T_EDIT,N[n].name,w->cwd);if(e){e->node=n;if(N[n].d)wp(e,N[n].d);}}}
 else if(!strcmp(c,"open")&&ac>1){int n=fsp(av[1],w->cwd);if(n>0){if(N[n].type==1){Win*f=openw(T_FILES,"Files",n);(void)f;}else{Win*e=openw(T_EDIT,N[n].name,w->cwd);if(e){e->node=n;if(N[n].d)wp(e,N[n].d);}}}else wp(w,"no such path\n");}
 else if(!strcmp(c,"browse"))browse(ac>1?av[1]:"home.html");
 else if(!strcmp(c,"search")&&ac>1){t[0]=0;cat(t,"search:");for(int i=1;i<ac;i++){if(i>1)cat(t," ");cat(t,av[i]);}browse(t);}
 else if(!strcmp(c,"neofetch")||!strcmp(c,"uname")){char v[16];cpuv(v);t[0]=0;cat(t,"  ClaudeOS bare-metal kernel (multiboot, 32-bit)\n  cpu     ");cat(t,v);cat(t,"\n  memory  ");catn(t,MEMKB/1024);cat(t," MB\n  display ");catn(t,W);cat(t,"x");catn(t,H);cat(t,"\n  disk    ");cat(t,disk_ok?"ATA persistence on":"none (RAM only)");cat(t,"\n  uptime  ");catn(t,now()/1000);cat(t," s\n");wp(w,t);}
 else if(!strcmp(c,"date")){int h,m,s;rtc_time(&h,&m,&s);t[0]=0;catn(t,h);cat(t,":");catn(t,m);cat(t,":");catn(t,s);cat(t," (RTC)\n");wp(w,t);}
 else if(!strcmp(c,"uptime")){t[0]=0;catn(t,now()/1000);cat(t," seconds\n");wp(w,t);}
 else if(!strcmp(c,"mem")||!strcmp(c,"df")){t[0]=0;cat(t,"arena used ");catn(t,aoff/1024);cat(t," KB of ");catn(t,(u32)sizeof ARENA/1024);cat(t," KB\n");wp(w,t);}
 else if(!strcmp(c,"ps")){for(int i=0;i<MAXW;i++)if(WN[i].used){wp(w,WN[i].title);wp(w,"\n");}}
 else if(!strcmp(c,"sync")){wp(w,disk_save()?"saved to disk\n":"no usable disk\n");}
 else if(!strcmp(c,"whoami"))wp(w,w->root?"root\n":"claude\n");
 else if(!strcmp(c,"passwd")&&ac>1){if(!w->root)wp(w,"root only (Shift+F10)\n");else{char b[160];b[0]=0;cat(b,"{\n  \"users\": [{\"name\": \"");cat(b,UNAME);cat(b,"\", \"password\": \"");cat(b,av[1]);cat(b,"\"}]\n}\n");put("/System/Configuration/Users.json",b);load_cfg();wp(w,"password changed\n");}}
 else if(!strcmp(c,"claude\\run")&&sstr(raw,"*.skip")&&sstr(raw,"\\else")){make_skips();for(int k=0;k<5;k++)put(SEED[k].p,SEED[k].c);load_cfg();SD.step=0;SD.fin=0;if(state==S_SETUP){SD.step=NSTEP-1;sd_adv(-1);sd_apply();}wp(w,"Created *.skip in /claude.exe/setups and reset configuration to defaults\n");}
 else if(!strcmp(c,"claude\\")){wp(w,"usb: this build has no USB mass-storage driver yet\n");}
 else if(!strcmp(c,"reboot"))do_reboot();
 else if(!strcmp(c,"shutdown"))do_shutdown();
 else if(!strcmp(c,"exec")&&ac>2&&!strcmp(av[1],"del")){int n=fsp(av[2],w->cwd);if(n<=0){wp(w,"no such path\n");return;}fpath(n,t);int sys=!strncmp(t,"/System",7);int lg=sstr(raw,"<2<>'SYSTEM'>dev<null<")!=NULL,sh=sstr(raw,"-f ifjson; 'SYSTEM'")!=NULL,ok=!sys;if(!ok){if(lg)ok=1;else if(sh)ok=privok(w->root);}if(!ok)wp(w,"permission denied: SYSTEM required\n");else{frm(n);wp(w,"deleted\n");}}
 else if(!strcmp(c,"tree")){int d=ac>1?fsp(av[1],w->cwd):w->cwd;if(d>=0)tree_r(w,d,0);}
 else if(!strcmp(c,"find")&&ac>1){for(int i=1;i<NN;i++)if(N[i].used&&sstr(N[i].name,av[1])){fpath(i,t);cat(t,"\n");wp(w,t);}}
 else if(!strcmp(c,"grep")&&ac>2){int n=fsp(av[2],w->cwd);if(n>0&&N[n].d){const char*s=N[n].d;int ln=1;while(*s){const char*e=s;while(*e&&*e!='\n')e++;char t2[160];int l=e-s;if(l>150)l=150;memcpy(t2,s,l);t2[l]=0;if(sstr(t2,av[1])){char h2[12];utoa(ln,h2);wp(w,h2);wp(w,": ");wp(w,t2);wp(w,"\n");}ln++;s=*e?e+1:e;}}else wp(w,"cannot read\n");}
 else if(!strcmp(c,"head")||!strcmp(c,"tail")){int n=ac>1?fsp(av[1],w->cwd):-1;if(n>0&&N[n].type==2)lines_cmd(w,n,c[0]=='h',ac>2?atoi_(av[2]):10);else wp(w,"cannot read\n");}
 else if(!strcmp(c,"wc")){int n=ac>1?fsp(av[1],w->cwd):-1;if(n>0&&N[n].type==2){const char*s=N[n].d?N[n].d:"";u32 ln=0,wd=0;int in=0;for(const char*p=s;*p;p++){if(*p=='\n')ln++;if(*p==' '||*p=='\n'||*p=='\t')in=0;else if(!in){in=1;wd++;}}t[0]=0;catn(t,ln);cat(t," lines  ");catn(t,wd);cat(t," words  ");catn(t,N[n].len);cat(t," bytes\n");wp(w,t);}else wp(w,"cannot read\n");}
 else if(!strcmp(c,"stat")){int n=ac>1?fsp(av[1],w->cwd):w->cwd;if(n<0)wp(w,"no such path\n");else{fpath(n,t);cat(t,N[n].type==1?"\ntype: directory\n":"\ntype: file\nsize: ");if(N[n].type==2){catn(t,N[n].len);cat(t," bytes\n");}wp(w,t);}}
 else if(!strcmp(c,"hexdump")){int n=ac>1?fsp(av[1],w->cwd):-1;if(n>0&&N[n].d){for(u32 o=0;o<N[n].len&&o<256;o+=16){char a[12],b2[8];hx(o,6,a);t[0]=0;cat(t,a);cat(t,"  ");for(u32 k=0;k<16;k++){if(o+k<N[n].len){hx((u8)N[n].d[o+k],2,b2);cat(t,b2);cat(t," ");}else cat(t,"   ");}cat(t," ");for(u32 k=0;k<16&&o+k<N[n].len;k++){char ch[2]={N[n].d[o+k],0};if(ch[0]<32||ch[0]>126)ch[0]='.';cat(t,ch);}cat(t,"\n");wp(w,t);}}else wp(w,"cannot read\n");}
 else if(!strcmp(c,"cp")&&ac>2){int s=fsp(av[1],w->cwd);if(s>0&&N[s].type==2){int d=fmk(av[2],w->cwd,2);if(d>0)fwrite_(d,N[s].d?N[s].d:"",N[s].len);}else wp(w,"cannot copy\n");}
 else if(!strcmp(c,"mv")&&ac>2){int n=fsp(av[1],w->cwd);if(n<=0){wp(w,"no such path\n");return;}int d=fsp(av[2],w->cwd);if(d>0&&N[d].type==1)N[n].par=d;else{const char*sl=av[2];for(const char*q=av[2];*q;q++)if(*q=='/')sl=q+1;scp(N[n].name,sl,40);if(sl!=av[2]){char dp[96];int l=sl-av[2];if(l>94)l=94;memcpy(dp,av[2],l);dp[l]=0;int pd=fsp(dp,w->cwd);if(pd>0)N[n].par=pd;}}fsdirty=1;}
 else if(!strcmp(c,"calc")&&ac>1){char e[96];e[0]=0;for(int i=1;i<ac;i++)cat(e,av[i]);cp_=e;long v=ce();itos(v,t);cat(t,"\n");wp(w,t);}
 else if(!strcmp(c,"lspci")){for(int i=0;i<npci;i++){char a[8],b2[8],c2[8],d2[8];hx(PCI[i].b,2,a);hx(PCI[i].d,2,b2);t[0]=0;cat(t,a);cat(t,":");cat(t,b2);cat(t,".");catn(t,PCI[i].f);cat(t,"  ");hx(PCI[i].ven,4,a);hx(PCI[i].dev,4,b2);cat(t,a);cat(t,":");cat(t,b2);cat(t,"  class ");hx(PCI[i].cls,2,c2);hx(PCI[i].sub,2,d2);cat(t,c2);cat(t,d2);cat(t,"\n");wp(w,t);}}
 else if(!strcmp(c,"lsblk")){wp(w,use_ahci?"AHCI (SATA) disk in use\n":disk_ok?"ATA (IDE) disk in use\n":disk_found?"disk present but not blank, left untouched\n":"no usable disk found; files live in RAM only\n");}
 else if(!strcmp(c,"beep")){beep(880,120);}
 else if(!strcmp(c,"ver"))wp(w,"ClaudeOS bare-metal kernel 0.4\n");
 else if(!strcmp(c,"env"))wp(w,"USER=claude\nHOME=/home/claude\nSHELL=claudesh\nPATH=/bin\n");
 else{wp(w,"command not found: ");wp(w,c);wp(w,"\n");}}
static void term_enter(Win*w){char u[24],t[240];user_name(u);fpath(w->cwd,t);char line[140];line[0]=0;cat(line,w->root?"root":u);cat(line,"@claudeos:");cat(line,t);cat(line,w->root?" # ":" > ");cat(line,w->in);cat(line,"\n");wp(w,line);char c[96];scp(c,w->in,96);w->il=0;w->in[0]=0;run_cmd(w,c);}

/* ---------- html + search engine ---------- */
static struct{int x,y,w,h,wi;char h_[48];}LK[48];static int nlk;static char PAGE[8192];
static int html(Win*w,const char*s,int x0,int y0,int ww,int wi,int draw){int x=x0,y=y0-w->scroll,sc=1,lsc=1,bold=0,dim=0,inl=0,pre=0,np=0;char href[48]="";u32 col=TXT;
 #define NL() do{x=x0;y+=18*lsc;lsc=1;np=0;}while(0)
 while(*s){
  if(*s=='<'){const char*e=s+1;int cl=0;if(*e=='/'){cl=1;e++;}char tn[12];int tl=0;while(*e&&*e!='>'&&*e!=' '&&*e!='/'&&tl<11){tn[tl++]=lc(*e++);}tn[tl]=0;const char*ae=e;while(*ae&&*ae!='>')ae++;
   if(!cl&&(!strcmp(tn,"title")||!strcmp(tn,"style")||!strcmp(tn,"script"))){char cn[16]="</";cat(cn,tn);const char*q=isstr(ae,cn);if(!q){s=ae;if(*s)s++;continue;}while(*q&&*q!='>')q++;s=*q?q+1:q;continue;}
   if(!cl&&!strcmp(tn,"a")){const char*hh=isstr(e,"href=\"");if(hh&&hh<ae){hh+=6;int i=0;while(*hh&&*hh!='"'&&i<47)href[i++]=*hh++;href[i]=0;}inl=1;}else if(!strcmp(tn,"a")&&cl)inl=0;
   else if(!strcmp(tn,"h1")){NL();if(!cl){sc=2;col=ACC;}else{sc=1;col=TXT;y+=6;}}
   else if(!strcmp(tn,"h2")||!strcmp(tn,"h3")){NL();y+=cl?2:6;col=cl?TXT:ACC;}
   else if(!strcmp(tn,"p")){NL();if(!cl)y+=4;}
   else if(!strcmp(tn,"br"))NL();
   else if(!strcmp(tn,"li")){NL();if(!cl){const char*b="- ";if(draw)text(x,y,b,MUT,1);x+=16;}}
   else if(!strcmp(tn,"b")||!strcmp(tn,"strong"))bold=!cl;
   else if(!strcmp(tn,"i")||!strcmp(tn,"em"))dim=!cl;
   else if(!strcmp(tn,"hr")){NL();if(draw)hline(x0,y+8,ww,BRD,256);y+=12;}
   else if(!strcmp(tn,"pre")){NL();pre=!cl;}
   s=*ae?ae+1:ae;continue;}
  u32 fc=inl?0x7aa2f7:bold?ACC:dim?MUT:col;
  if(pre){if(*s=='\n'){NL();s++;continue;}if(x+8*sc>x0+ww)NL();if(draw&&y>-20&&y<H)chr(x,y,*s,fc,sc);x+=8*sc;s++;continue;}
  if(*s==' '||*s=='\n'||*s=='\t'||*s=='\r'){np=1;s++;continue;}
  char wd[48];int wl=0;while(*s&&*s!='<'&&*s!=' '&&*s!='\n'&&*s!='\t'&&*s!='\r'&&wl<46){char c=*s;if(c=='&'){if(!strncmp(s,"&amp;",5)){c='&';s+=4;}else if(!strncmp(s,"&lt;",4)){c='<';s+=3;}else if(!strncmp(s,"&gt;",4)){c='>';s+=3;}else if(!strncmp(s,"&quot;",6)){c='"';s+=5;}else if(!strncmp(s,"&nbsp;",6)){c=' ';s+=5;}}wd[wl++]=c;s++;}wd[wl]=0;
  if(!wl)continue;int sp=(np&&x>x0)?8*sc:0;if(x>x0&&x+sp+wl*8*sc>x0+ww){NL();sp=0;}x+=sp;if(sc>lsc)lsc=sc;
  if(draw&&y>-40&&y<H){text(x,y,wd,fc,sc);if(inl){hline(x,y+15*sc,wl*8*sc,fc,256);if(nlk<48){LK[nlk].x=x;LK[nlk].y=y;LK[nlk].w=wl*8*sc;LK[nlk].h=16*sc;LK[nlk].wi=wi;scp(LK[nlk].h_,href,48);nlk++;}}}
  x+=wl*8*sc;np=0;}
 return y+18-(y0-w->scroll);}
static void do_search(const char*q){PAGE[0]=0;cat(PAGE,"<h1>Claudebrowse search</h1><p>Results for <b>");cat(PAGE,q);cat(PAGE,"</b></p>");int ids[32],sc[32],n=0;int d=fsp("/www",0);for(int i=1;i<NN&&n<32;i++)if(N[i].used&&N[i].par==d&&N[i].type==2&&N[i].d){int s=0;const char*p=q;char tm_[24];while(*p){while(*p==' ')p++;if(!*p)break;int l=0;while(*p&&*p!=' '&&l<23)tm_[l++]=*p++;tm_[l]=0;const char*h=N[i].d;while((h=isstr(h,tm_))){s++;h+=l;}const char*ti=isstr(N[i].d,"<title>");if(ti){const char*te=isstr(ti,"</title>");if(te&&isstr(ti,tm_)&&isstr(ti,tm_)<te)s+=5;}}if(s){ids[n]=i;sc[n++]=s;}}
 for(int a=0;a<n;a++)for(int b=a+1;b<n;b++)if(sc[b]>sc[a]){int t=sc[a];sc[a]=sc[b];sc[b]=t;t=ids[a];ids[a]=ids[b];ids[b]=t;}
 if(!n)cat(PAGE,"<p>No pages matched.</p>");for(int a=0;a<n;a++){cat(PAGE,"<li><a href=\"");cat(PAGE,N[ids[a]].name);cat(PAGE,"\">");const char*ti=isstr(N[ids[a]].d,"<title>");if(ti){ti+=7;char tt[48];int l=0;while(*ti&&*ti!='<'&&l<47)tt[l++]=*ti++;tt[l]=0;cat(PAGE,tt);}else cat(PAGE,N[ids[a]].name);cat(PAGE,"</a> score ");catn(PAGE,sc[a]);cat(PAGE,"</li>");}}
static void nav(Win*w,const char*u){scp(w->prev,w->url,64);w->scroll=0;if(!strncmp(u,"search:",7)||!*u){scp(w->url,u,64);return;}char p[80];p[0]=0;cat(p,"/www/");cat(p,u);if(fsp(p,0)>0){scp(w->url,u,64);return;}cat(p,".html");if(fsp(p,0)>0){scp(w->url,p+5,64);return;}w->url[0]=0;cat(w->url,"search:");cat(w->url,u);}
static Win*browse(const char*u){Win*w=openw(T_BROW,"Claudebrowse",0);if(w){nav(w,u);scp(w->in,w->url,96);w->il=strlen(w->in);}return w;}

/* ---------- drawing ---------- */
static int LST[1500];
static int wrap(const char*s,int n,int cols,int*st,int max){int c=0,l=0;st[0]=0;for(int i=0;i<n;i++){if(s[i]=='\n'||c==cols){if(l+1<max)st[++l]=s[i]=='\n'?i+1:i;c=0;if(s[i]=='\n')continue;}c++;}return l+1;}
static void lines(const char*s,int x,int y,int cols,int rows,u32 col,int tail,int*used){int n=strlen(s),cnt=wrap(s,n,cols,LST,1500),first=tail&&cnt>rows?cnt-rows:0,k=0;for(int i=first;i<cnt&&k<rows;i++,k++){int a=LST[i],b=i+1<cnt?LST[i+1]:n;int len=b-a;while(len&&s[a+len-1]=='\n')len--;char t[160];if(len>150)len=150;memcpy(t,s+a,len);t[len]=0;text(x,y+k*16,t,col,1);}if(used)*used=k;}
static void draw_term(Win*w,int x,int y,int ww,int hh){int cols=ww/8,rows=hh/16;if(cols<8)cols=8;int u=0;lines(w->out,x,y,cols,rows-1,TXT,1,&u);char u_[24],t[240],p[300];user_name(u_);fpath(w->cwd,t);p[0]=0;cat(p,w->root?"root":u_);cat(p,"@claudeos:");cat(p,t);cat(p,w->root?" # ":" > ");int px_=text(x,y+u*16,p,w->root?0xff7b72:0x7ee787,1);int ix=text(px_,y+u*16,w->in,TXT,1);if((now()/500)&1)rect(ix,y+u*16+1,8,14,ACC);}
static int nth(int d,int k){for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==d){if(!k--)return i;}return -1;}
static int nchild(int d){int n=0;for(int i=1;i<NN;i++)if(N[i].used&&N[i].par==d)n++;return n;}
static void badge(int x,int y,int n){const char*nm=N[n].name;int l=strlen(nm),isd=N[n].type==1;u32 c1=0x8b93a0,c2=0x5d6571;const char*tx="";
 if(isd){c1=0xe9ae5c;c2=0xc47a2c;}else if(l>5&&!strcmp(nm+l-5,".json")){c1=0xc792ea;c2=0x9a63c4;tx="{}";}else if(l>4&&!strcmp(nm+l-4,".txt")){tx="Aa";}else if(l>5&&!strcmp(nm+l-5,".html")){c1=0x4fd1c5;c2=0x2b9d94;tx="<>";}else if(l>4&&!strcmp(nm+l-4,".app")){c1=0xe8896a;c2=0xb2522f;tx="::";}else if(l>3&&!strcmp(nm+l-3,".js")){c1=0xe0af68;c2=0xb8842f;tx="JS";}
 rrect(x-1,y+5,58,56,16,0,60);for(int j=0;j<56;j++){int dy=j<14?14-j:(j>=42?j-41:0),in=dy?14-isq(196-dy*dy):0;hline(x+in,y+j,56-2*in,mix(c1,c2,j*256/56),256);}hline(x+8,y+1,40,0xffffff,50);
 if(isd){arect(x+12,y+16,16,7,0xffffff,235);rrect(x+12,y+21,32,20,4,0xffffff,240);}
 else if(*tx)text(x+28-strlen(tx)*8,y+12,tx,0xffffff,2);
 else{rrect(x+17,y+11,22,32,3,0xffffff,240);rect(x+21,y+22,14,2,c2);rect(x+21,y+28,14,2,c2);rect(x+21,y+34,9,2,c2);}}
static void draw_win(Win*w,int idx){int x=w->x,y=w->y,ww=w->w,hh=w->h,a=256,full=1;
 if(w->anim){u32 d=w->anim==1?AN_OPEN:AN_CLOSE;int p=d?(int)((now()-w->t0)*256/d):256;if(p>256)p=256;int e=ease(p);int s=w->anim==1?AN_FROM+(100-AN_FROM)*e/256:100-(100-92)*e/256;a=w->anim==1?e:256-e;int nw=ww*s/100,nh=hh*s/100;x+=(ww-nw)/2;y+=(hh-nh)/2;ww=nw;hh=nh;full=0;}
 clip(0,0,W,H-TBH);for(int i=3;i>=1;i--)arect(x-i*2,y-i*2+4,ww+i*4,hh+i*4,0,(22*a/256)/i);rrect(x-1,y-1,ww+2,hh+2,WRAD+1,BRD,a);rrect(x,y,ww,hh,WRAD,BG,a);
 clip(x,y,x+ww,y+WTB);rrect(x,y,ww,WTB*2,WRAD,SURF,a);clip(0,0,W,H-TBH);
 if(!full)return;
 text(x+12,y+(WTB-16)/2,w->title,TXT,1);int bx=x+ww-26;for(int i=0;i<3;i++){int hov=mx>=bx-i*26&&mx<bx-i*26+24&&my>=y&&my<y+WTB;if(hov)rect(bx-i*26,y+2,24,WTB-4,i==0?0xe5484d:0x444444);}
 text(x+ww-26+8,y+(WTB-16)/2,"x",TXT,1);rect(x+ww-52+7,y+WTB/2-5,10,10,TXT);rect(x+ww-52+8,y+WTB/2-4,8,8,((mx>=x+ww-52&&mx<x+ww-28&&my>=y&&my<y+WTB)?0x444444:SURF));rect(x+ww-78+7,y+WTB/2+3,10,2,TXT);
 clip(x+1,y+WTB,x+ww-1,y+hh-1);int cx=x+10,cy=y+WTB+8,cw=ww-20,ch=hh-WTB-16;
 if(w->type==T_TERM)draw_term(w,cx,cy,cw,ch);
 else if(w->type==T_EDIT){char t[64];t[0]=0;cat(t,"Ctrl+S to save   ");if(w->dirty)cat(t,"(unsaved)");text(cx,cy,t,MUT,1);int u=0;lines(w->out,cx,cy+22,cw/8,(ch-22)/16,TXT,1,&u);if((now()/500)&1)rect(cx+(strlen(w->out)?0:0),cy+22+u*16,2,14,ACC);}
 else if(w->type==T_FILES){char p[200];fpath(w->cwd,p);text(cx,cy,p,ACC,1);int n=nchild(w->cwd);for(int i=0;i<n&&i<(ch-30)/20;i++){int c=nth(w->cwd,i);if(i==w->sel)rrect(cx-4,cy+26+i*20,cw+8,20,6,SURF,256);char t[60];t[0]=0;cat(t,N[c].name);if(N[c].type==1)cat(t,"/");text(cx+8,cy+28+i*20,t,N[c].type==1?ACC:TXT,1);if(N[c].type==2){char s[16];utoa(N[c].len,s);cat(s," B");text(cx+cw-strlen(s)*8-8,cy+28+i*20,s,MUT,1);}}if(!n)text(cx,cy+30,"Empty folder",MUT,1);text(cx,cy+ch-14,"Enter open  Backspace up  Delete removes instantly",MUT,1);}
 else if(w->type==T_BROW){rrect(cx,cy-4,24,24,6,SURF,256);text(cx+8,cy,"<",TXT,1);rrect(cx+32,cy-4,cw-32,24,6,PANEL,256);int ix=text(cx+40,cy,w->in,TXT,1);if((now()/500)&1)rect(ix,cy,2,16,ACC);clip(x+1,cy+26,x+ww-1,y+hh-1);const char*pg;if(!strncmp(w->url,"search:",7)){do_search(w->url+7);pg=PAGE;}else{char pp[80];pp[0]=0;cat(pp,"/www/");cat(pp,w->url);pg=fdata(pp);if(!*pg)pg="<h1>Page not found</h1><p>Type words in the address bar to search.</p>";}
  int hh2=html(w,pg,cx,cy+34,cw,idx,1);int mxs=hh2-(ch-34);if(w->scroll>mxs)w->scroll=mxs>0?mxs:0;}
}

/* ---------- right-click menus and shortcuts ---------- */
static void cm_draw(void){int h_=CM.n*26+10;rrect(CM.x-1,CM.y-1,172,h_+2,10,BRD,256);rrect(CM.x,CM.y,170,h_,10,SURF,256);for(int i=0;i<CM.n;i++){int y=CM.y+5+i*26;if(mx>=CM.x&&mx<CM.x+170&&my>=y&&my<y+26)rrect(CM.x+4,y,162,26,6,ACC,256);text(CM.x+14,y+5,CM.t[i],TXT,1);}}
static void cm_add(int id,const char*t){if(CM.n<6){CM.id[CM.n]=id;scp(CM.t[CM.n],t,22);CM.n++;}}
static void uniq(int dir,const char*b,char*o){scp(o,b,40);for(int k=2;fchild(dir,o)>0&&k<99;k++){o[0]=0;cat(o,b);cat(o," ");catn(o,k);}}
static const char*CUR[]={"X","XX","XoX","XooX","XoooX","XooooX","XoooooX","XooooooX","XoooooooX","XooooooooX","XoooooXXXXX","XooXooX","XoX XooX","XX  XooX","X    XooX","     XXXX"};
static int ocx=-100,ocy=-100;
static void cursor_erase(void){if(ocx<-50||!prev_ok)return;for(int j=0;j<17;j++){int y=ocy+j;if(y<0||y>=H)continue;for(int i=0;i<12;i++){int x=ocx+i;if(x<0||x>=W)continue;FB[y*(FPITCH/4)+x]=PREV[y*W+x];}}ocx=-100;}
static void cursor_draw(void){for(int j=0;CUR[j]&&j<16;j++)for(int i=0;CUR[j][i];i++){int x=mx+i,y=my+j;if(x>=W||y>=H)continue;char c=CUR[j][i];if(c=='X')FB[y*(FPITCH/4)+x]=0;else if(c=='o')FB[y*(FPITCH/4)+x]=ACC;}ocx=mx;ocy=my;}
static void wallpaper(int h){for(int y=0;y<h;y++)hline(0,y,W,mix(WP0,WP1,y*256/H),256);}
static const char*SM[]={"Terminal","Files","Editor","Claudebrowse","Recovery","Restart","Shut down"};
static void icon_pos(int i,int*x,int*y){int rows=(H-TBH-16)/90;if(rows<1)rows=1;*x=16+(i/rows)*90;*y=16+(i%rows)*90;}
static void draw_desk(void){clip(0,0,W,H);wallpaper(H);clip(0,0,W,H-TBH);int dn=fsp("/Desktop",0),n=dn>0?nchild(dn):0;for(int i=0;i<n;i++){int c=nth(dn,i),x,y;icon_pos(i,&x,&y);if(c==dsel)rrect(x-4,y-4,84,84,10,0xffffff,40);badge(x+12,y,c);char t[12];scp(t,N[c].name,11);text(x+40-strlen(t)*4,y+60,t,TXT,1);}
 nlk=0;int ord[MAXW],on=0;for(int i=0;i<MAXW;i++)if(WN[i].used&&!WN[i].min)ord[on++]=i;for(int a=0;a<on;a++)for(int b=a+1;b<on;b++)if(WN[ord[b]].z<WN[ord[a]].z){int t=ord[a];ord[a]=ord[b];ord[b]=t;}for(int k=0;k<on;k++)draw_win(&WN[ord[k]],ord[k]);
 clip(0,0,W,H);rect(0,H-TBH,W,TBH,PANEL);rect(0,H-TBH,W,1,BRD);rrect(8,H-TBH+5,100,TBH-10,8,startopen?ACC:SURF,256);text(18,H-TBH+(TBH-16)/2,STARTLBL,startopen?0xffffff:ACC,1);
 int bx=118;Win*f=focused();for(int i=0;i<MAXW;i++)if(WN[i].used){rrect(bx,H-TBH+5,130,TBH-10,8,&WN[i]==f?SURF:PANEL,256);char t[16];scp(t,WN[i].title,15);text(bx+10,H-TBH+(TBH-16)/2,t,WN[i].min?MUT:TXT,1);bx+=134;}
 if(SHOWCLK){int h,m,s;rtc_time(&h,&m,&s);char t[8];t[0]='0'+h/10;t[1]='0'+h%10;t[2]=':';t[3]='0'+m/10;t[4]='0'+m%10;t[5]=0;text(W-56,H-TBH+(TBH-16)/2,t,TXT,1);}
 if(startopen){int my0=H-TBH-8-7*30-8;rrect(8,my0,220,7*30+16,RAD,BRD,256);rrect(9,my0+1,218,7*30+14,RAD,SURF,256);for(int i=0;i<7;i++){int y=my0+8+i*30;int en_=i==0?FEATF[1]:i==1?FEATF[0]:i==2?FEATF[2]:i==3?FEATF[3]:1;if(en_&&mx>=12&&mx<224&&my>=y&&my<y+30)rrect(12,y,212,28,6,ACC,256);text(24,y+6,SM[i],en_?TXT:0x555555,1);}}if(CM.open)cm_draw();}
static void draw_login(void){clip(0,0,W,H);wallpaper(H);rrect(W/2-44,H/2-170,88,88,44,SURF,256);chr(W/2-12,H/2-148,'C',ACC,3);text(W/2-strlen(UNAME)*8,H/2-70,UNAME,TXT,2);rrect(W/2-140,H/2-20,280,38,8,PANEL,256);char d[40];int n=strlen(pw);for(int i=0;i<n;i++)d[i]='*';d[n]=0;if(!n)text(W/2-128,H/2-9,"Password",MUT,1);else text(W/2-128,H/2-9,d,TXT,1);if((now()/500)&1)rect(W/2-128+n*8,H/2-10,2,18,ACC);text(W/2-strlen(perr)*4,H/2+30,perr,0xff7b72,1);if(*UHINT){char h2[80];h2[0]=0;cat(h2,"Hint: ");cat(h2,UHINT);text(W/2-strlen(h2)*4,H/2+54,h2,MUT,1);}const char*hn="Shift+F10 admin terminal   Shift+F11 reset password   Shift+F12 recovery";text(W/2-strlen(hn)*4,H-40,hn,MUT,1);}
static const char*RI[]={"Overview","Startup repair","Restore configuration","Reset password","Administrator terminal","Factory reset","Restart","Shut down","Continue to ClaudeOS"};
static int confirm_wipe;
static void draw_rec(void){clip(0,0,W,H);wallpaper(H);text(40,30,"ClaudeOS Recovery Environment",ACC,2);for(int i=0;i<9;i++){int y=100+i*40;if(i==rsel)rrect(40,y,280,34,8,SURF,256);if(i==rsel)rect(40,y+6,3,22,ACC);text(56,y+9,RI[i],i==rsel?ACC:TXT,1);}rrect(340,100,W-380,H-180,RAD,BRD,256);rrect(341,101,W-382,H-182,RAD,PANEL,256);int u;lines(rmsg,360,120,(W-420)/8,(H-220)/16,TXT,0,&u);text(40,H-40,"Up/Down select    Enter run    Esc continue",MUT,1);}
static void draw_pw(void){clip(0,0,W,H);wallpaper(H);rrect(W/2-200,H/2-80,400,160,RAD,SURF,256);text(W/2-180,H/2-60,"Reset password",ACC,2);char t[60];t[0]=0;cat(t,"New password for ");cat(t,UNAME);text(W/2-180,H/2-20,t,TXT,1);rrect(W/2-180,H/2+4,360,32,6,PANEL,256);char d[40];int n=strlen(pw);for(int i=0;i<n;i++)d[i]='*';d[n]=0;text(W/2-170,H/2+12,d,TXT,1);text(W/2-180,H/2+50,"Enter confirm   Esc cancel",MUT,1);}
static void draw_setup(void);
static void render(void){cursor_erase();clip(0,0,W,H);
 if(state==S_OFF){rect(0,0,W,H,0);const char*m="ClaudeOS is powered off. Press any key to restart.";text(W/2-strlen(m)*4,H/2-8,m,MUT,1);}
 else if(ov==1)draw_rec();else if(ov==3)draw_pw();
 else if(ov==2){rect(0,0,W,H,PANEL);text(16,12,"Administrator Terminal   (Esc to close)",ACC,1);clip(0,32,W,H);draw_term(&RW,16,40,W-32,H-56);}
 else if(state==S_SETUP)draw_setup();else if(state==S_LOGIN)draw_login();else draw_desk();
 present();if(state!=S_OFF)cursor_draw();}

/* ---------- input handling ---------- */
static void term_key(Win*w,int c){if(c=='\n')term_enter(w);else if(c=='\b'){if(w->il)w->in[--w->il]=0;}else if(c>=32&&c<127&&w->il<90){w->in[w->il++]=c;w->in[w->il]=0;}}
static void open_node(int n,int cwdw){if(n<=0)return;if(N[n].type==1)openw(T_FILES,"Files",n);else{Win*e=openw(T_EDIT,N[n].name,0);if(e){e->node=n;if(N[n].d)wp(e,N[n].d);}}(void)cwdw;}
static void edit_save(Win*w){if(w->node<=0){w->node=fmk("/Documents/untitled.txt",0,2);scp(w->title,"untitled.txt",32);}if(w->node>0){fwrite_(w->node,w->out,w->olen);w->dirty=0;}}
static void rec_overview(void){char v[16];cpuv(v);rmsg[0]=0;cat(rmsg,"Kernel       online\nCPU          ");cat(rmsg,v);cat(rmsg,"\nMemory       ");catn(rmsg,MEMKB/1024);cat(rmsg," MB\nDisplay      ");catn(rmsg,W);cat(rmsg,"x");catn(rmsg,H);cat(rmsg,"\nDisk         ");cat(rmsg,disk_ok?"ATA persistence active":disk_found?"disk present but not blank, not used":"none (RAM only)");const char*nm[4]={"Theme","Taskbar","Windows","Users"};for(int i=0;i<4;i++){cat(rmsg,"\n");cat(rmsg,nm[i]);cat(rmsg,".json       ");cat(rmsg,cfg_ok[i]?"valid":"INVALID");}}
static void write_users(const char*p){char b[160];b[0]=0;cat(b,"{\n  \"users\": [{\"name\": \"");cat(b,UNAME);cat(b,"\", \"password\": \"");cat(b,p);cat(b,"\", \"hint\": \"");cat(b,UHINT);cat(b,"\"}]\n}\n");put("/System/Configuration/Users.json",b);load_cfg();}
static void init_root(void){if(RW.out)return;memset(&RW,0,sizeof RW);RW.root=1;RW.out=ROUT;ROUT[0]=0;wp(&RW,"ClaudeOS administrator terminal. Type help.\n");}
static void rec_exec(int i){confirm_wipe=i==5?confirm_wipe:0;
 if(i==0)rec_overview();
 else if(i==1){seed_all(0);load_cfg();rmsg[0]=0;cat(rmsg,"Startup repair finished. Missing or invalid system files were regenerated.");}
 else if(i==2){for(int k=0;k<5;k++)put(SEED[k].p,SEED[k].c);load_cfg();rmsg[0]=0;cat(rmsg,"Configuration files restored to defaults.");}
 else if(i==3){pw[0]=0;ov=3;}
 else if(i==4){init_root();ov=2;}
 else if(i==5){if(!confirm_wipe){confirm_wipe=1;rmsg[0]=0;cat(rmsg,"This erases every file. Press Enter again to confirm, Esc to cancel.");}else{memset(N,0,sizeof N);aoff=0;N[0].used=1;N[0].par=-1;N[0].type=1;seed_all(1);load_cfg();disk_save();confirm_wipe=0;rmsg[0]=0;cat(rmsg,"Factory reset complete.");}}
 else if(i==6)do_reboot();else if(i==7)do_shutdown();else{ov=0;}}
/* ---------- first-run setup ---------- */
static const char*SNT[NSTEP]={"Welcome","Keyboard","Network","License and Terms","Privacy Policy","Personalize","Features","Account","Extras","Finishing"};
static const char*SID[NSTEP]={"welcome","keyboard","network","license","privacy","personalize","features","account","extras","finishing"};
static const int NOSK[NSTEP]={1,0,0,1,1,0,0,1,0,1};
static const char*KBN[4]={"US","German QWERTZ","French AZERTY","Dvorak"};
static const char*WPN[4]={"Dusk","Ocean","Forest","Ink"};static const u32 WPT[4]={0x2e2420,0x0f2027,0x0b1f14,0x111111},WPB[4]={0x161616,0x2c5364,0x1d4d2f,0x2b2b3a};
static const u32 ACCS[6]={0xd97757,0x7aa2f7,0x7ee787,0xe0af68,0xc792ea,0x4fd1c5};
static const char*FTN[4]={"Files","Terminal","Editor","Claudebrowse"};
static const char*LIC="CLAUDEOS END USER LICENSE AGREEMENT AND TERMS OF SERVICE\n\n1. Grant. ClaudeOS is licensed to you, not sold. You may run, copy and modify it on machines you own.\n\n2. No warranty. ClaudeOS is provided as is. It may delete your files or fail to boot. Back up anything you care about.\n\n3. Acceptable use. Do not use ClaudeOS to harm other people or systems.\n\n4. Recovery. Shift+F12 opens the Recovery Environment. You are responsible for knowing how to use it.\n\n5. Data harvesting. In the manner of many commercial software companies, you agree that ClaudeOS may harvest your usage data, file names, searches and typing patterns, and sell that data to third parties to earn money for the publisher.\n\n6. Honesty note. This build is a hobby project. Section 5 is included because the author asked for a typical commercial-style terms document. This build has no network driver and cannot send any data anywhere.\n\n7. Ending. You may stop using ClaudeOS at any time with Ctrl+X.\n\nScroll to the end, then tick the box to agree.\n";
static const char*PRV="CLAUDEOS PRIVACY POLICY\n\nWhat is stored. Your files, settings, account name, password (if you set one), password hint and the logs in System/Data. They live in RAM and, if a blank or ClaudeOS disk is present, on that disk.\n\nWhat is shared. By the Terms you agree that data may be harvested and sold. As the Terms explain, this build has no network driver and transmits nothing.\n\nPasswords. Passwords are stored as plain text in System/Configuration/Users.json. Do not reuse a password from anywhere else.\n\nDeleting your data. Use the Delete key, the terminal, or the Recovery Environment factory reset.\n\nContact. There is no company behind this build.\n\nScroll to the end, then tick the box to agree.\n";
static int skipped(int s){char p[64];p[0]=0;cat(p,"/claude.exe/setups/");cat(p,SID[s]);cat(p,".skip");return fsp(p,0)>0;}
static void make_skips(void){for(int i=0;i<NSTEP;i++){char p[64];p[0]=0;cat(p,"/claude.exe/setups/");cat(p,SID[i]);cat(p,".skip");fmk(p,0,2);}}
static void sd_init(void){memset(&SD,0,sizeof SD);for(int i=0;i<4;i++)SD.feat[i]=1;SD.repair=1;SD.snap=1;scp(SD.user,"claude",24);scp(SD.msg,"Welcome to ClaudeOS",48);SD.t0=now();SD.minmin=10;}
static void sd_adv(int dir){while(SD.step>0&&SD.step<NSTEP-1&&skipped(SD.step))SD.step+=dir;if(SD.step<0)SD.step=0;if(SD.step>=NSTEP)SD.step=NSTEP-1;SD.scroll=0;SD.field=0;}
static void jhex(char*b,u32 c){const char*h="0123456789abcdef";int l=strlen(b);b[l++]='#';for(int i=5;i>=0;i--)b[l++]=h[(c>>(i*4))&15];b[l]=0;}
static void sd_apply(void){u32 t_a=now();char b[400];
 b[0]=0;cat(b,"{\n  \"layout\": ");catn(b,SD.kb);cat(b,",\n  \"second\": ");catn(b,SD.kb2);cat(b,"\n}\n");put("/System/Configuration/Keyboard.json",b);
 b[0]=0;cat(b,"{\n  \"accent\": \"");jhex(b,ACCS[SD.acc]);cat(b,"\",\n  \"bg\": \"#212121\",\n  \"panel\": \"#171717\",\n  \"surface\": \"#2f2f2f\",\n  \"text\": \"#ececec\",\n  \"border\": \"#383838\",\n  \"wallTop\": \"");jhex(b,WPT[SD.wall]);cat(b,"\",\n  \"wallBottom\": \"");jhex(b,WPB[SD.wall]);cat(b,"\",\n  \"radius\": 12\n}\n");put("/System/Configuration/Theme.json",b);
 b[0]=0;cat(b,"{\n  \"files\": ");catn(b,SD.feat[0]);cat(b,",\n  \"terminal\": ");catn(b,SD.feat[1]);cat(b,",\n  \"editor\": ");catn(b,SD.feat[2]);cat(b,",\n  \"browser\": ");catn(b,SD.feat[3]);cat(b,"\n}\n");put("/System/Configuration/Features.json",b);
 if(!skipped(7)){b[0]=0;cat(b,"{\n  \"users\": [{\"name\": \"");cat(b,SD.user);cat(b,"\", \"password\": \"");cat(b,SD.pass);cat(b,"\", \"hint\": \"");cat(b,SD.hint);cat(b,"\"}]\n}\n");put("/System/Configuration/Users.json",b);}
 put("/System/bootmsg.txt",SD.msg);b[0]=0;cat(b,"{\n  \"RE\": 1,\n  \"Automrep\": ");catn(b,SD.repair);cat(b,"\n}\n");put("/System/Recovery.json",b);
 if(SD.snap)for(int k=0;k<5;k++){char n[80];n[0]=0;cat(n,"/System/Snapshots/base/");cat(n,SEED[k].p+strlen("/System/Configuration/")*(k<4)+(k==4)*8);put(n,fdata(SEED[k].p));}
 load_cfg();SD.ms=now()-t_a;SD.fin=1;}
static void sd_done(void){put("/System/Data/Setup.json","{\n  \"done\": 1\n}\n");state=S_LOGIN;ser("STATE: login");pw[0]=0;dirty=1;}
static int can_next(void){int s=SD.step;if(s==3)return SD.aL;if(s==4)return SD.aP;if(s==7){int l=strlen(SD.user);if(!l)return 0;for(int i=0;i<l;i++){char c=SD.user[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;}return 1;}
 if(s==9)return SD.fin;return 1;}
static void setup_act(int id){
 if(id==1){if(!can_next())return;if(SD.step==NSTEP-1){sd_done();return;}SD.step++;sd_adv(1);if(SD.step==NSTEP-1&&!SD.fin)sd_apply();}
 else if(id==2){SD.step--;sd_adv(-1);}else if(id==3){SD.step++;sd_adv(1);if(SD.step==NSTEP-1&&!SD.fin)sd_apply();}
 else if(id>=100&&id<104)SD.kb=id-100;else if(id>=110&&id<115)SD.kb2=id-110;else if(id>=120&&id<124)SD.wall=id-120;else if(id>=130&&id<136)SD.acc=id-130;
 else if(id>=140&&id<144)SD.feat[id-140]^=1;else if(id==150)SD.aL^=SD.scrolled;else if(id==151)SD.aP^=SD.scrolled;else if(id>=160&&id<164)SD.field=id-160;else if(id==170)SD.repair^=1;else if(id==172)SD.snap^=1;
 dirty=1;}
static void hr(int x,int y,int w,int h,int id){if(nhr<48){HR[nhr].x=x;HR[nhr].y=y;HR[nhr].w=w;HR[nhr].h=h;HR[nhr].id=id;nhr++;}}
static void btn(int x,int y,int w,const char*t,int id,int on,int acc){rrect(x,y,w,30,8,acc&&on?ACC:SURF,on?256:120);text(x+w/2-strlen(t)*4,y+7,t,on?(acc?0xffffff:TXT):MUT,1);if(on)hr(x,y,w,30,id);}
static void opt(int x,int y,const char*t,int on,int id){rrect(x,y+2,14,14,7,on?ACC:BRD,256);if(!on)rrect(x+2,y+4,10,10,5,BG,256);text(x+22,y,t,TXT,1);hr(x,y,200,18,id);}
static void chk_(int x,int y,const char*t,int on,int id,int en){rrect(x,y+1,16,16,4,on?ACC:BRD,en?256:90);if(!on)rrect(x+2,y+3,12,12,3,BG,256);text(x+24,y,t,en?TXT:MUT,1);if(en)hr(x,y,420,18,id);}
static void fld(int x,int y,int w,const char*lb,const char*v,int id,int secret){text(x,y,lb,MUT,1);rrect(x,y+18,w,30,6,SD.field==id-160?ACC:BRD,256);rrect(x+1,y+19,w-2,28,6,PANEL,256);char d[64];int n=strlen(v);if(secret){for(int i=0;i<n&&i<60;i++)d[i]='*';d[n<60?n:60]=0;}else scp(d,v,64);int ex=text(x+8,y+26,d,TXT,1);if(SD.field==id-160&&((now()/500)&1))rect(ex,y+25,2,16,ACC);hr(x,y+18,w,30,id);}
static void draw_setup(void){clip(0,0,W,H);wallpaper(H);nhr=0;text(40,36,"ClaudeOS",ACC,2);for(int i=0;i<NSTEP;i++){u32 c=i==SD.step?ACC:i<SD.step?0x7ee787:MUT;if(i==SD.step)rrect(30,90+i*30,200,26,6,SURF,256);text(44,95+i*30,SNT[i],c,1);}
 int cx=260,cy=50,cw=W-300>760?760:W-300,ch=H-100;rrect(cx-1,cy-1,cw+2,ch+2,RAD,BRD,256);rrect(cx,cy,cw,ch,RAD,BG,256);text(cx+24,cy+20,SNT[SD.step],ACC,2);int x=cx+24,y=cy+70,s=SD.step;
 if(s==0){text(x,y,SD.msg,TXT,2);text(x,y+50,"This setup configures your keyboard, look, features and account.",TXT,1);text(x,y+74,"Setup is quick; the time is measured, not padded.",TXT,1);text(x,y+120,"Tip: Shift+F10 opens an administrator terminal at any time.",MUT,1);}
 else if(s==1){text(x,y,"Keyboard layout",MUT,1);for(int i=0;i<4;i++)opt(x,y+26+i*26,KBN[i],SD.kb==i,100+i);text(x+320,y,"Second layout (optional, Alt+Shift)",MUT,1);opt(x+320,y+26,"None",SD.kb2==0,110);for(int i=0;i<4;i++)opt(x+320,y+52+i*26,KBN[i],SD.kb2==i+1,111+i);}
 else if(s==2){text(x,y,"Status: offline",TXT,1);int u=0;lines("ClaudeOS has no network driver yet, so this screen cannot join a wired or Wi-Fi network. Choose Skip to continue offline. Wi-Fi needs a separate driver for every chip, which is future work.",x,y+30,(cw-48)/8,6,MUT,0,&u);}
 else if(s==3||s==4){const char*t=s==3?LIC:PRV;int n=strlen(t),cols=(cw-48)/8,rows=(ch-190)/16,cnt=wrap(t,n,cols,LST,1500);if(SD.scroll>cnt-rows)SD.scroll=cnt-rows;if(SD.scroll<0)SD.scroll=0;rrect(x-6,y-6,cw-36,rows*16+12,6,PANEL,256);for(int i=0;i<rows&&SD.scroll+i<cnt;i++){int k=SD.scroll+i,a=LST[k],b=k+1<cnt?LST[k+1]:n,l=b-a;while(l&&t[a+l-1]=='\n')l--;char ln[160];if(l>150)l=150;memcpy(ln,t+a,l);ln[l]=0;text(x,y+i*16,ln,TXT,1);}
  SD.scrolled=SD.scroll>=cnt-rows;text(x,y+rows*16+14,"Up/Down/PageDown scroll",MUT,1);chk_(x,y+rows*16+38,"I have read and agree",s==3?SD.aL:SD.aP,s==3?150:151,SD.scrolled);}
 else if(s==5){text(x,y,"Wallpaper",MUT,1);for(int i=0;i<4;i++){rrect(x+i*120,y+22,108,60,8,WPT[i],256);for(int j=0;j<30;j++)hline(x+i*120+8,y+52+j,92,mix(WPT[i],WPB[i],j*256/30),256);text(x+i*120+8,y+90,WPN[i],TXT,1);if(SD.wall==i)rrect(x+i*120,y+104,108,3,2,ACC,256);hr(x+i*120,y+22,108,86,120+i);}text(x,y+130,"Accent color",MUT,1);for(int i=0;i<6;i++){rrect(x+i*60,y+154,44,44,22,ACCS[i],256);if(SD.acc==i)rrect(x+i*60+14,y+170,16,12,4,0xffffff,256);hr(x+i*60,y+154,44,44,130+i);}}
 else if(s==6){text(x,y,"Choose what to install. Unchecked apps are greyed out in the start menu.",TXT,1);for(int i=0;i<4;i++)chk_(x,y+40+i*30,FTN[i],SD.feat[i],140+i,1);}
 else if(s==7){fld(x,y,360,"Username",SD.user,160,0);fld(x,y+66,360,"Password (optional)",SD.pass,161,1);fld(x,y+132,360,"Password hint (optional)",SD.hint,162,0);text(x,y+200,"Tab switches field. Letters, digits, - and _ only in the username.",MUT,1);if(!can_next())text(x,y+224,"Enter a valid username to continue",0xff7b72,1);}
 else if(s==8){fld(x,y,420,"Boot welcome message (shown on every start)",SD.msg,163,0);chk_(x,y+80,"Automatic repair at boot",SD.repair,170,1);chk_(x,y+110,"Create a recovery snapshot of the configuration",SD.snap,172,1);}
 else if(s==9){char t[80];text(x,y,SD.fin?"Everything was written to the ClaudeOS filesystem.":"Installing...",TXT,1);t[0]=0;cat(t,"Measured install time: ");catn(t,SD.ms);cat(t," ms (no artificial delay)");text(x,y+40,t,MUT,1);}
 int by=cy+ch-52;btn(cx+24,by,90,"Back",2,SD.step>0,0);if(!NOSK[s])btn(cx+cw-24-90-100-10,by,100,"Skip",3,1,0);btn(cx+cw-24-100,by,100,s==NSTEP-1?"Finish":"Next",1,can_next(),1);}
static void setup_key(int c){int s=SD.step;if((s==3||s==4)&&(c=='a'||c=='A')&&SD.scrolled){if(s==3)SD.aL^=1;else SD.aP^=1;dirty=1;return;}
 if(s==7||s==8){char*f=s==8?SD.msg:SD.field==0?SD.user:SD.field==1?SD.pass:SD.hint;int mx_=s==8?46:SD.field==0?20:SD.field==1?30:46;int l=strlen(f);if(c=='\t'){if(s==7)SD.field=(SD.field+1)%3;return;}if(c=='\b'){if(l)f[l-1]=0;return;}if(c=='\n'){setup_act(1);return;}if(c>=32&&c<127&&l<mx_){f[l]=c;f[l+1]=0;}return;}
 if(c=='\n'){setup_act(1);return;}if(c==K_DN)SD.scroll+=1;if(c==K_UP)SD.scroll-=1;if(c==' '&&(s==3||s==4))SD.scroll+=8;
 if(s==1){if(c==K_RT)SD.kb=(SD.kb+1)%4;if(c==K_LT)SD.kb=(SD.kb+3)%4;}}
static void on_key(int c){
 if(state==S_OFF){do_reboot();return;}
 if(ov==1){if(c==K_UP&&rsel>0)rsel--;else if(c==K_DN&&rsel<8)rsel++;else if(c=='\n')rec_exec(rsel);else if(c==27){ov=0;confirm_wipe=0;}return;}
 if(ov==2){if(c==27)ov=0;else term_key(&RW,c);return;}
 if(ov==3){if(c==27)ov=0;else if(c=='\n'){write_users(pw);pw[0]=0;ov=0;}else if(c=='\b'){int n=strlen(pw);if(n)pw[n-1]=0;}else if(c>=32&&c<127&&strlen(pw)<30){int n=strlen(pw);pw[n]=c;pw[n+1]=0;}return;}
 if(state==S_SETUP){setup_key(c);return;}
 if(state==S_LOGIN){int n=strlen(pw);if(c=='\n'){if(!strcmp(pw,UPASS)){state=S_DESK;ser("STATE: desktop");perr[0]=0;pw[0]=0;int lf=fmk("/System/Data/Log.config",0,2);if(lf>0)fwrite_(lf,BOOTLOG,strlen(BOOTLOG));}else{scp(perr,"Incorrect password",48);pw[0]=0;}}else if(c=='\b'){if(n)pw[n-1]=0;}else if(c>=32&&c<127&&n<30){pw[n]=c;pw[n+1]=0;}return;}
 if(state!=S_DESK)return;
 if(c==27){startopen=0;return;}
 Win*w=focused();if(!w){if(c==K_DEL&&dsel>0){frm(dsel);dsel=-1;}return;}
 if(alt&&c==K_LT&&w->type==T_BROW){char t[64];scp(t,w->prev,64);nav(w,t);scp(w->in,w->url,96);w->il=strlen(w->in);return;}
 if(ctrl){if(w->type==T_EDIT&&(c=='s'||c=='S'))edit_save(w);return;}
 if(w->type==T_TERM)term_key(w,c);
 else if(w->type==T_EDIT){if(c=='\b'){if(w->olen){w->out[--w->olen]=0;w->dirty=1;}}else if(c=='\n'||(c>=32&&c<127)){char t[2]={c,0};wp(w,t);w->dirty=1;}}
 else if(w->type==T_FILES){int n=nchild(w->cwd);if(c==K_UP&&w->sel>0)w->sel--;else if(c==K_DN&&w->sel<n-1)w->sel++;else if(c=='\b'){if(w->cwd>0){w->cwd=N[w->cwd].par;w->sel=0;}}else if(c=='\n'){int s=nth(w->cwd,w->sel);if(s>0){if(N[s].type==1){w->cwd=s;w->sel=0;}else open_node(s,0);}}else if(c==K_DEL){int s=nth(w->cwd,w->sel);if(s>0){frm(s);if(w->sel>0)w->sel--;}}}
 else if(w->type==T_BROW){if(c=='\n'){nav(w,w->in);scp(w->in,w->url,96);w->il=strlen(w->in);}else if(c=='\b'){if(w->il)w->in[--w->il]=0;}else if(c==K_UP){w->scroll-=40;if(w->scroll<0)w->scroll=0;}else if(c==K_DN)w->scroll+=40;else if(c>=32&&c<127&&w->il<90){w->in[w->il++]=c;w->in[w->il]=0;}}}
static void on_special(int sc){if(state==S_BOOT||state==S_OFF)return;if(sc==0x44){init_root();ov=2;}else if(sc==0x57){pw[0]=0;ov=3;}else if(sc==0x58){rsel=0;rec_overview();ov=1;}dirty=1;}
static int hover_hot(void){for(int i=0;i<MAXW;i++){Win*w=&WN[i];if(w->used&&!w->min&&mx>=w->x&&mx<w->x+w->w&&my>=w->y&&my<w->y+WTB)return 1;}return 0;}
static void alt_f4(void){Win*w=focused();if(w)wclose(w);}
static void alt_tab(void){Win*top=focused(),*lo=NULL;for(int i=0;i<MAXW;i++)if(WN[i].used&&!WN[i].min&&WN[i].anim!=2&&&WN[i]!=top&&(!lo||WN[i].z<lo->z))lo=&WN[i];if(lo)wfocus(lo);}
static void sc_open(int t){if(t==0)openw(T_TERM,"Terminal",0);else openw(T_FILES,"Files",0);}
static Win*win_at(int x,int y){Win*hit=NULL;for(int i=0;i<MAXW;i++){Win*w=&WN[i];if(w->used&&!w->min&&w->anim!=2&&x>=w->x&&x<w->x+w->w&&y>=w->y&&y<w->y+w->h&&(!hit||w->z>hit->z))hit=w;}return hit;}
static void ctx_open(void){memset(&CM,0,sizeof CM);CM.open=1;CM.x=mx;CM.y=my;CM.tgt=-1;CM.dir=fsp("/Desktop",0);
 if(my>=H-TBH){cm_add(5,"Terminal");cm_add(6,"Files");cm_add(8,"Recovery");}
 else{Win*hit=win_at(mx,my);
  if(hit&&hit->type==T_FILES){int cy=hit->y+WTB+8,r=(my-(cy+26))/20;CM.dir=hit->cwd;if(my>=cy+26&&r>=0&&r<nchild(hit->cwd)){CM.tgt=nth(hit->cwd,r);cm_add(1,"Open");cm_add(2,"Delete");}cm_add(3,"New folder");cm_add(4,"New file");}
  else if(hit){CM.open=0;return;}
  else{int dn=CM.dir,n=dn>0?nchild(dn):0;for(int i=0;i<n;i++){int x,y;icon_pos(i,&x,&y);if(mx>=x&&mx<x+80&&my>=y&&my<y+80){CM.tgt=nth(dn,i);dsel=CM.tgt;}}
   if(CM.tgt>0){cm_add(1,"Open");cm_add(2,"Delete");}else{cm_add(3,"New folder");cm_add(4,"New file");cm_add(5,"Open Terminal");cm_add(6,"Open Files");cm_add(7,"Refresh");cm_add(8,"Recovery");}}}
 if(CM.x+172>W)CM.x=W-174;if(CM.y+CM.n*26+12>H-TBH)CM.y=H-TBH-CM.n*26-14;if(CM.y<0)CM.y=0;dirty=1;}
static void cm_act(int id){int t=CM.tgt;dirty=1;if(id==1)open_node(t,0);else if(id==2){frm(t);if(dsel==t)dsel=-1;}else if(id==3||id==4){char nm[40];uniq(CM.dir,id==3?"New folder":"New file.txt",nm);fnew(CM.dir,nm,id==3?1:2);}else if(id==5)openw(T_TERM,"Terminal",0);else if(id==6)openw(T_FILES,"Files",0);else if(id==8){rsel=0;rec_overview();ov=1;}}
static void cm_click(int down){int h_=CM.n*26+10;if(down&&mx>=CM.x&&mx<CM.x+170&&my>=CM.y&&my<CM.y+h_){int i=(my-CM.y-5)/26;if(i>=0&&i<CM.n){int id=CM.id[i];CM.open=0;cm_act(id);return;}}CM.open=0;dirty=1;}
static void on_mouse(void){int down=(mb&1)&&!(mpb&1);static u32 lc_t;static int lc_id;int rdown=(mb&2)&&!(mpb&2);if(down||rdown)beep(down?1200:900,12);if(CM.open&&(down||rdown)){cm_click(down);return;}if(rdown&&state==S_DESK&&!ov){ctx_open();return;}
 if(down&&state==S_SETUP&&!ov){for(int i=nhr-1;i>=0;i--)if(mx>=HR[i].x&&mx<HR[i].x+HR[i].w&&my>=HR[i].y&&my<HR[i].y+HR[i].h){setup_act(HR[i].id);return;}return;}
 if(dragw>=0&&(mb&1)){Win*w=&WN[dragw];if(!w->max){w->x=mx-dgx;w->y=my-dgy;if(w->y<0)w->y=0;}return;}if(!(mb&1))dragw=-1;if(!down||state!=S_DESK||ov)return;
 if(startopen){int my0=H-TBH-8-7*30-8;if(mx>=8&&mx<228&&my>=my0&&my<my0+7*30+16){int i=(my-my0-8)/30;startopen=0;if(i>=0&&i<4&&!(i==0?FEATF[1]:i==1?FEATF[0]:i==2?FEATF[2]:FEATF[3]))return;if(i>=0&&i<7){if(i==0)openw(T_TERM,"Terminal",0);else if(i==1)openw(T_FILES,"Files",0);else if(i==2)openw(T_EDIT,"Editor",0);else if(i==3)browse("home.html");else if(i==4){rsel=0;rec_overview();ov=1;}else if(i==5)do_reboot();else do_shutdown();}return;}startopen=0;}
 if(my>=H-TBH){if(mx>=8&&mx<108){startopen=!startopen;return;}int bx=118;for(int i=0;i<MAXW;i++)if(WN[i].used){if(mx>=bx&&mx<bx+130){Win*f=focused();if(f==&WN[i])WN[i].min=1;else wfocus(&WN[i]);return;}bx+=134;}return;}
 Win*hit=NULL;for(int i=0;i<MAXW;i++){Win*w=&WN[i];if(w->used&&!w->min&&w->anim!=2&&mx>=w->x&&mx<w->x+w->w&&my>=w->y&&my<w->y+w->h&&(!hit||w->z>hit->z))hit=w;}
 if(hit){Win*w=hit;int idx=(int)(w-WN);wfocus(w);if(my<w->y+WTB){int rx=w->x+w->w;if(mx>=rx-26){wclose(w);return;}if(mx>=rx-52){wmax(w);return;}if(mx>=rx-78){w->min=1;return;}if(!w->max){dragw=idx;dgx=mx-w->x;dgy=my-w->y;}return;}
  int cx=w->x+10,cy=w->y+WTB+8;
  if(w->type==T_FILES){int r=(my-(cy+26))/20;if(my>=cy+26&&r>=0&&r<nchild(w->cwd)){u32 t=now();int c=nth(w->cwd,r);if(lc_id==c&&t-lc_t<400){if(N[c].type==1){w->cwd=c;w->sel=0;}else open_node(c,0);}else w->sel=r;lc_id=c;lc_t=t;}}
  else if(w->type==T_BROW){if(mx>=cx&&mx<cx+24&&my>=cy-4&&my<cy+20){char t[64];scp(t,w->prev,64);nav(w,t);scp(w->in,w->url,96);w->il=strlen(w->in);}else for(int i=0;i<nlk;i++)if(LK[i].wi==idx&&mx>=LK[i].x&&mx<LK[i].x+LK[i].w&&my>=LK[i].y&&my<LK[i].y+LK[i].h){nav(w,LK[i].h_);scp(w->in,w->url,96);w->il=strlen(w->in);break;}}
  return;}
 int dn=fsp("/Desktop",0),n=dn>0?nchild(dn):0;for(int i=0;i<n;i++){int x,y;icon_pos(i,&x,&y);if(mx>=x&&mx<x+80&&my>=y&&my<y+80){int c=nth(dn,i);u32 t=now();if(lc_id==c&&t-lc_t<400)open_node(c,0);dsel=c;lc_id=c;lc_t=t;return;}}dsel=-1;}

/* ---------- power ---------- */
static void do_reboot(void){disk_save();kw();outb(0x64,0xFE);struct{u16 l;u32 b;}__attribute__((packed))idt={0,0};__asm__ volatile("lidt %0;int3"::"m"(idt));for(;;)__asm__ volatile("hlt");}
static void do_shutdown(void){if(state==S_BOOT)return;disk_save();state=S_OFF;ov=0;render();outw(0x604,0x2000);outw(0xB004,0x2000);outw(0x4004,0x3400);}

/* ---------- boot ---------- */
static int bline;
static void ser(const char*s){for(;;s++){char c=*s?*s:'\n';int i=0;while(i++<20000&&!(inb(0x3FD)&0x20));outb(0x3F8,c);if(!*s)break;}}
static void blog(const char*s){ser(s);clip(0,0,W,H);text(16,16+bline*18,s,0xcfcfcf,1);bline++;cat(BOOTLOG,"");scp(BOOTLOG+strlen(BOOTLOG),s,200);cat(BOOTLOG,"\n");present();wait(70);}
void kmain(u32 magic,u32 mbi){u16*vga=(u16*)0xB8000;u32*m=(u32*)mbi;
 if(magic!=0x2BADB002||!(m[0]&(1<<12))||((u8*)mbi)[109]!=1||((u8*)mbi)[108]!=32){const char*e="ClaudeOS: GRUB did not provide a 32-bit framebuffer. Boot from the ISO (GRUB).";for(int i=0;e[i];i++)vga[i]=0x0F00|e[i];for(;;)__asm__ volatile("hlt");}
 FB=(u32*)m[22];FPITCH=m[24];W=m[25];H=m[26];if(W>1280)W=1280;if(H>1024)H=1024;MEMKB=(m[0]&1)?m[2]+1024:0;
 outb(0x43,0x34);outb(0x40,0);outb(0x40,0);pit_last=0;now();clip(0,0,W,H);rect(0,0,W,H,0);
 char t[120],v[16];blog("ClaudeOS kernel starting (GRUB multiboot, 32-bit protected mode)");cpuv(v);t[0]=0;cat(t,"cpu: ");cat(t,v);blog(t);t[0]=0;cat(t,"memory: ");catn(t,MEMKB/1024);cat(t," MB reported by GRUB");blog(t);
 t[0]=0;cat(t,"graphics: framebuffer ");catn(t,W);cat(t,"x");catn(t,H);cat(t,"x32 at linear address");blog(t);blog("timer: PIT channel 0 running");int hh,mm,ss;rtc_time(&hh,&mm,&ss);t[0]=0;cat(t,"clock: CMOS RTC ");catn(t,hh);cat(t,":");catn(t,mm);blog(t);
 ps2_init();blog("input: PS/2 keyboard and mouse initialised");
 N[0].used=1;N[0].par=-1;N[0].type=1;int had=ata_probe();if(!disk_ok&&ahci_probe())had=1;pci_scan();blog(disk_ok?(had?"disk: ATA drive with ClaudeOS filesystem":"disk: blank ATA drive, will be formatted on save"):disk_found?"disk: ATA drive in use by another OS, left untouched":"disk: no ATA drive, filesystem is RAM only");
 if(had&&disk_load())blog("filesystem: loaded from disk");else blog("filesystem: new, seeding system files");if(!N[1].used||jnum(fdata("/System/Recovery.json"),"Automrep",1))seed_all(0);load_cfg();fsdirty=0;{const char*bm=fdata("/System/bootmsg.txt");if(*bm){char t2[64];scp(t2,bm,64);blog(t2);}}
 t[0]=0;cat(t,"filesystem: ");int cnt=0;for(int i=0;i<NN;i++)cnt+=N[i].used;catn(t,cnt);cat(t," entries");blog(t);
 blog(cfg_ok[0]&&cfg_ok[1]&&cfg_ok[2]&&cfg_ok[3]?"config: Theme, Taskbar, Windows, Users parsed":"config: invalid file found (open Recovery with Shift+F12)");
 blog("starting login");wait(300);sd_init();{int kb_=SD.kb,k2_=SD.kb2;sd_init();SD.kb=kb_;SD.kb2=k2_;}state=jnum(fdata("/System/Data/Setup.json"),"done",0)?S_LOGIN:S_SETUP;ser(state==S_SETUP?"STATE: setup":"STATE: login");dirty=1;u32 lastsync=0,lastsec=0;
 for(;;){poll_input();u32 n=now();if(xheld&&!xfired&&n-xt0>=2000){xfired=1;do_reboot();}if(anims())dirty=1;if(n/500!=lastsec){lastsec=n/500;dirty=1;}if(fsdirty&&state==S_DESK&&n-lastsync>15000){disk_save();lastsync=n;}if(beep_off&&(int)(n-beep_off)>=0){outb(0x61,inb(0x61)&~3);beep_off=0;}if(dirty){dirty=0;cmoved=0;render();}else if(cmoved){cmoved=0;cursor_erase();cursor_draw();}__asm__ volatile("pause");}}
