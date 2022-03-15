/* Source module for treevolve */
/* Constant population size    */
/* Population subdivision      */
/* (c) N Grassly 1997          */
/* Dept. Zoology, Oxford.      */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "treevolve.h"
#include "const.sub.h"
#include "mathfuncs.h"

#define BIG_NUMBER 1.0e+30

static double GenTimeEndSub(Regime *pp);
static void DistributeGenes(Regime *pp);

static double probRE, probMI;
static int currDeme;

int ConstSubRoutine(Regime *pp)
{
double t, rnd;

	DistributeGenes(pp);
	t=0.0;
	do{
		t+=GenTimeEndSub(pp);
		if( t > (pp->t) && pp->t > 0.0){
			globTime+=(pp->t);
			return 1;
		}
		rnd=rndu();
		if(rnd<probRE){ 				/* RE event */
			Recombine(t+globTime, currDeme);
			K++;
			ki[currDeme]++;
			noRE++;
		}else{			
			if(rnd<(probRE+probMI)){	/* MI event */
				Migration(currDeme, pp->d);
				noMI++;
			}else{						/* CA event */
				Coalesce(t+globTime, currDeme);
				K--;
				ki[currDeme]--;
				noCA++;
			}
		}
	}while(K>1);
	globTime+=t;
	return 0;
}


static void DistributeGenes(Regime *pp)
{
int i, j;
Node *nptr;

	for(i=0;i<(pp->d);i++)
		ki[i]=0;
	nptr=first;
	for(i=0;i<K;i++){
		do
			j=((int) (rndu()*(pp->d)));
		while(j==(pp->d));
		ki[j]++;
		nptr->deme=j;
		nptr=nptr->next;
	}
}

static double GenTimeEndSub(Regime *pp)
{
int shortest;
long gi;
double GR, MK, zz, t1, t2, rnd;

	t1=BIG_NUMBER;
	for(currDeme=0;currDeme<(pp->d);currDeme++){
		if(ki[currDeme]!=0){
			do
				rnd=rndu();
			while(rnd==0.0);
			if(pp->r!=0.0){
				gi=CalcGi(currDeme);
				GR=( ((double) gi) * factr * genTimeInvVar * (pp->N) * (pp->r) );
			}else
				GR=0.0;gi=0;
			MK=factr * genTimeInvVar * (pp->N) * (pp->m) * (ki[currDeme]);
			zz=GR+MK+( (ki[currDeme])*(ki[currDeme]-1) );
			probRE=GR/zz;
			probMI=MK/zz;
			t2=(-log(rnd))/zz;
			if(t2<t1){
				t1=t2;
				shortest=currDeme;
			}
		}
	}
	currDeme=shortest;
	if(pp->r!=0.0){
		gi=CalcGi(currDeme);
		GR=( ((double) gi) * factr * genTimeInvVar * (pp->N) * (pp->r) );
	}else
		GR=0.0;gi=0;
	MK=factr * genTimeInvVar * (pp->N) * (pp->m) * (ki[currDeme]);
	zz=GR+MK+( (ki[currDeme])*(ki[currDeme]-1) );
	probRE=GR/zz;
	probMI=MK/zz;
	return (t1 * factr * genTimeInvVar * (pp->N) );
}

#undef BIG_NUMBER
