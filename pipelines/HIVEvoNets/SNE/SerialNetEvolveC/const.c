/* Source module for treevolve */
/* Constant population size    */
/* No population subdivision   */
/* (c) N Grassly 1997          */
/* Dept. Zoology, Oxford.      */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "treevolve.h"
#include "const.h"
#include "mathfuncs.h"

static double GenerateTimeEnd(Regime *pp);	

static double probRE;

/*-----------------------*/
int ConstRoutine(Regime *pp)
{
double t, rnd;
int i;
Node *nptr;

	nptr=first;
	for(i=0;i<K;i++){	/* make all genes in same deme */
		nptr->deme=0;
		nptr=nptr->next;
	}
	ki[0]=K;
	t=0.0;
	do{
		if(!NoClock && intervalDis>0.0 && pCounter>=SSizeperP-1 && (t+globTime)<prevTime+intervalDis)
			t=prevTime+intervalDis;
			
		t+=GenerateTimeEnd(pp);
		if( t > (pp->t) && pp->t > 0.0){
			globTime+=(pp->t);
			return 1;
		}
	//fprintf(stderr, "probRE=%lf K=%d\n",probRE, K);
		rnd=rndu();
		if(rnd<probRE){	/*RE event*/
			Recombine(t+globTime, 0);
			K++;
			ki[0]++;
			noRE++;
		}
		else{			/*CA event*/
			Coalesce(t+globTime, 0);
			K--;
			ki[0]--;
			noCA++;
		}
	}while(K>1);
	/* Detective Search for Recomb rate Bug */
	//	pp->r=0.0;

	globTime+=t;
	return 0;
}


static double GenerateTimeEnd(Regime *pp) /* for constant pop size and no subdivision */
{
double zz, rnd, t, GR;
long gi;

	do
		rnd=rndu();
	while(rnd==0.0);
	
	if(pp->r!=0.0){
		gi=CalcGi(0);
		GR=((double) gi) * factr * (pp->N) * genTimeInvVar * (pp->r);
	}else
		GR=0.0;gi=0;
	zz=(double) ( GR + (K*(K-1)) ); /* times are exponentially distributed                */
	probRE=GR/zz;					/* with parameter genTimeInvVar*factr*N*r + (K*(K-1)) */
	t=((-log(rnd))/zz) * factr * genTimeInvVar * (pp->N);
	return t;
}
