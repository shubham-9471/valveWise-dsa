#include "hydraulics.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VALVEWISE_NO_EPANET
#include "epanet2_2.h"
#include "epanet2_enums.h"
#endif
#ifdef _WIN32
#define HYD_REPORT "NUL"
#else
#define HYD_REPORT "/dev/null"
#endif

typedef struct { char *p; size_t n, cap; bool bad; } HB;
static void hg(HB *b, size_t add) { if (b->bad || add > SIZE_MAX - b->n - 1) { b->bad=true; return; } while (b->n+add+1>b->cap) { size_t c=b->cap>SIZE_MAX/2?0:b->cap*2; if(!c){b->bad=true;return;} char *p=realloc(b->p,c); if(!p){b->bad=true;return;} b->p=p;b->cap=c; } }
static HB *hb_new(void) { HB *b=calloc(1,sizeof(*b)); if(!b)return NULL; b->cap=1024;b->p=malloc(b->cap);if(!b->p){free(b);return NULL;}b->p[0]=0;return b; }
static void hp(HB *b,const char *s) { if(!s||b->bad)return; size_t n=strlen(s);hg(b,n);if(!b->bad){memcpy(b->p+b->n,s,n);b->n+=n;b->p[b->n]=0;} }
static void hq(HB *b,const char *s) { hp(b,"\""); for(const char *p=s; p&&*p;p++){if(*p=='"'||*p=='\\'){char x[2]={'\\',*p};hg(b,2);if(!b->bad){b->p[b->n++]=x[0];b->p[b->n++]=x[1];b->p[b->n]=0;}}else{char x[2]={*p,0};hp(b,x);}} hp(b,"\""); }
static void hf(HB *b,const char *fmt,...) { va_list ap; va_start(ap,fmt); va_list cp;va_copy(cp,ap);int n=vsnprintf(NULL,0,fmt,cp);va_end(cp);if(n<0){b->bad=true;va_end(ap);return;}hg(b,(size_t)n);if(!b->bad){vsnprintf(b->p+b->n,b->cap-b->n,fmt,ap);b->n+=(size_t)n;}va_end(ap); }
typedef struct { double *p,*d,*f; int *st, deficient; double unserved; } HState;
static HState *hs_new(const Graph *g){HState*s=calloc(1,sizeof(*s));if(!s)return NULL;size_t n=(size_t)(g->node_count>0?g->node_count:1),l=(size_t)(g->link_count>0?g->link_count:1);s->p=calloc(n,sizeof(double));s->d=calloc(n,sizeof(double));s->f=calloc(l,sizeof(double));s->st=calloc(l,sizeof(int));if(!s->p||!s->d||!s->f||!s->st){free(s->p);free(s->d);free(s->f);free(s->st);free(s);return NULL;}return s;}
static void hs_free(HState*s){if(!s)return;free(s->p);free(s->d);free(s->f);free(s->st);free(s);}
#ifndef VALVEWISE_NO_EPANET
static bool has(const int*a,int n,int x){for(int i=0;i<n;i++)if(a[i]==x)return true;return false;}
static int warn_ok(int rc,int *warn){if(rc==0)return 0;if(rc==6){*warn=rc;return 0;}return rc;}
static int solve_one(const char *inp,const Graph*g,const int*fail,int nf,const int*iso,int ni,const int*opt,int no,double minpsi,HState*out,const double*base,int*warn){
 EN_Project ph=NULL;int rc=EN_createproject(&ph);if(rc)return rc;rc=EN_open(ph,inp,HYD_REPORT,"");if(rc){EN_deleteproject(ph);return rc;}EN_setstatusreport(ph,0);int units=0;rc=EN_getflowunits(ph,&units);if(warn_ok(rc,warn)){EN_deleteproject(ph);return rc;}double scale=units>=EN_LPS?1.422334:1.0;rc=EN_setdemandmodel(ph,EN_PDA,minpsi/scale,30.0/scale,0.5);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}
 for(int i=0;i<g->link_count;i++){char id[LINK_ID_LEN];strncpy(id,g->links[i].id,sizeof(id));id[sizeof(id)-1]=0;int ix=0;if(EN_getlinkindex(ph,id,&ix))continue;if(has(fail,nf,i)||has(iso,ni,i)||has(opt,no,i)){rc=EN_setlinkvalue(ph,ix,EN_STATUS,EN_CLOSED);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}rc=EN_setlinkvalue(ph,ix,EN_INITSTATUS,EN_CLOSED);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}}}
 rc=EN_solveH(ph);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}
 for(int i=0;i<g->node_count;i++){char id[NODE_ID_LEN];strncpy(id,g->nodes[i].id,sizeof(id));id[sizeof(id)-1]=0;int ix=0;if(EN_getnodeindex(ph,id,&ix)){EN_close(ph);EN_deleteproject(ph);return 203;}double v=0;rc=EN_getnodevalue(ph,ix,EN_PRESSURE,&v);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}out->p[i]=v*scale;rc=EN_getnodevalue(ph,ix,EN_DEMAND,&out->d[i]);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}if(g->nodes[i].base_demand>0&&out->p[i]<minpsi)out->deficient++;if(base&&base[i]>out->d[i])out->unserved+=base[i]-out->d[i];}
 for(int i=0;i<g->link_count;i++){char id[LINK_ID_LEN];strncpy(id,g->links[i].id,sizeof(id));id[sizeof(id)-1]=0;int ix=0;if(EN_getlinkindex(ph,id,&ix))continue;rc=EN_getlinkvalue(ph,ix,EN_FLOW,&out->f[i]);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}double v=0;rc=EN_getlinkvalue(ph,ix,EN_STATUS,&v);if(warn_ok(rc,warn)){EN_close(ph);EN_deleteproject(ph);return rc;}out->st[i]=(int)v;}
 EN_close(ph);EN_deleteproject(ph);return 0;
}
#endif
static void state_json(HB*b,const Graph*g,const HState*s,const char*name,double minpsi){hf(b,"\"%s\":{\"nodes\":[",name);for(int i=0;i<g->node_count;i++){if(i)hp(b,",");hp(b,"{\"id\":");hq(b,g->nodes[i].id);hf(b,",\"pressure\":%.6f,\"demand\":%.6f,\"served\":%s}",s->p[i],s->d[i],s->p[i]>=minpsi?"true":"false");}hp(b,"],\"links\":[");for(int i=0;i<g->link_count;i++){if(i)hp(b,",");hp(b,"{\"id\":");hq(b,g->links[i].id);hf(b,",\"flow\":%.6f,\"status\":\"%s\"}",s->f[i],s->st[i]==0?"CLOSED":"OPEN");}hp(b,"],\"pressure_deficient\":[");bool first=true;for(int i=0;i<g->node_count;i++)if(g->nodes[i].base_demand>0&&s->p[i]<minpsi){if(!first)hp(b,",");first=false;hq(b,g->nodes[i].id);}hf(b,"],\"unserved_demand\":%.6f}",s->unserved);}
HydraulicsOutput *hydraulics_run(const char*inp,const Graph*g,const int*fail,int nf,const int*iso,int ni,const PlacementResult*pr,int budget,double minpsi,bool enabled){HydraulicsOutput*out=calloc(1,sizeof(*out));if(!out)return NULL;HB*b=hb_new();if(!b){free(out);return NULL;}hp(b,"{\"available\":false,\"engine\":\"OWA-EPANET 2.2\",\"units\":{\"pressure\":\"psi\",\"flow\":\"native\"},\"min_pressure\":");hf(b,"%.6f",minpsi);if(!enabled){hp(b,",\"note\":\"disabled by --hydraulics off\"}");out->json=b->p;free(b);return out;}
#ifdef VALVEWISE_NO_EPANET
 hp(b,",\"note\":\"OWA-EPANET was not built into this binary\"}");out->json=b->p;free(b);return out;
#else
 HState*a=hs_new(g),*f=hs_new(g),*i=hs_new(g),*o=hs_new(g);int warn=0,no=(budget>0&&pr)?pr->placed_count:0;int*opt=no?malloc((size_t)no*sizeof(int)):NULL;for(int x=0;x<no;x++)opt[x]=pr->placed_valves[x];int rc=(!a||!f||!i||!o||solve_one(inp,g,NULL,0,NULL,0,NULL,0,minpsi,a,NULL,&warn)||solve_one(inp,g,fail,nf,NULL,0,NULL,0,minpsi,f,a->d,&warn)||solve_one(inp,g,fail,nf,iso,ni,NULL,0,minpsi,i,a->d,&warn)||solve_one(inp,g,fail,nf,iso,ni,opt,no,minpsi,o,a->d,&warn));if(rc){hp(b,",\"note\":\"OWA-EPANET could not solve one or more requested states\"}");out->json=b->p;free(b);free(opt);hs_free(a);hs_free(f);hs_free(i);hs_free(o);return out;}out->pressure_deficient_count=f->deficient;b->n=0;b->p[0]=0;hp(b,"{\"available\":true,\"engine\":\"OWA-EPANET 2.2\",\"units\":{\"pressure\":\"psi\",\"flow\":\"native\"},\"min_pressure\":");hf(b,"%.6f",minpsi);if(warn)hf(b,",\"note\":\"EPANET warning code %d was retained\"",warn);hp(b,",");state_json(b,g,a,"baseline",minpsi);hp(b,",");state_json(b,g,f,"after_failure",minpsi);hp(b,",");state_json(b,g,i,"after_isolation",minpsi);hp(b,",");state_json(b,g,o,"after_optimization",minpsi);hp(b,"}");out->json=b->bad?NULL:b->p;if(b->bad)free(b->p);free(b);free(opt);hs_free(a);hs_free(f);hs_free(i);hs_free(o);return out;
#endif
}
void hydraulics_free(HydraulicsOutput*out){if(!out)return;free(out->json);free(out);}
