/* Source module for treevolve */
/* Exponential population size */
/* Population subdivision      */
/* (c) N Grassly 1997          */
/* Dept. Zoology, Oxford.      */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "treevolve.h"
#include "mathfuncs.h"
#include "exp.sub.h"

#define BIG_NUMBER 1.0e+30
#define BRACKET 0.1

static double GenTimeEpiSub(Regime *pp);
static double approxFuncSub();
static void DistributeGenes(Regime *pp);
static void InstantCoalesce(double t, Regime *pp);

static double probRE, probMI, Pn;
static double lambda, N, migrationRate, recRate;
static int currDeme;

int EpiSubRoutine(Regime *pp)
{
double t, tiMe, rnd, Ntemp;
//int i;

	DistributeGenes(pp);
	lambda=pp->e;
	N=pp->N;
	migrationRate=pp->m;
	recRate=pp->r;
	t=0.0;
	do{
		tiMe=GenTimeEpiSub(pp);
		if(t+tiMe  > (pp->t) && pp->t > 0.0 ){	/* End of this Regime */
			Ntemp=((pp->N)*exp(-lambda*((pp->t)-t)));
			if(Ntemp<MIN_NE){
				InstantCoalesce(t, pp);
				return 0;
			}
			globTime+=(pp->t);
			return 1;
		}
		Ntemp=(N*exp(-lambda*tiMe));
		if(Ntemp<MIN_NE){
			InstantCoalesce(t, pp);
			return 0;
		}
		N=Ntemp;
		t+=tiMe;
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
	fprintf(stderr, "Population size at t = %1f is %f\n", globTime, N);
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
static void InstantCoalesce(double t, Regime *pp)
{
double tiMe;
Node *nptr;
int i;

	fprintf(stderr, "Each subpopulation has reached 1.0 before final coalescence,\n");
	fprintf(stderr, "with the number of genes at %d.\n", K);
	fprintf(stderr, "All genes are therefore instantly coalescing\n");
	tiMe= -(log(MIN_NE/N)) / lambda;
	N=MIN_NE;
	t+=tiMe;
	nptr=first;
	for(i=0;i<K;i++){
		nptr->deme=0;
		nptr=nptr->next;
	}
	for(i=0;i<pp->d;i++)
		ki[i]=0;
	ki[0]=K;
	while(K>1){
		Coalesce(t+globTime, 0);
		K--;
		ki[0]--;
		noCA++;
	}
}


static double GenTimeEpiSub(Regime *pp)
{
double t1, t2, brackets[2], P;
int shortest;
double a, b, m, temp;
long gi;

	t1=HUGE_VAL;
	for(currDeme=0;currDeme<pp->d;currDeme++){
		if(ki[currDeme]!=0){
			do
				Pn=rndu();
			while(Pn==0.0);
			if(migrationRate!=0.0)
				t2=approxFuncSub();/* exact if ki=1 (but inf. if m=0.0) */
			else
				t2=BIG_NUMBER;
			
			if(ki[currDeme]!=1){
				brackets[0]=0.0;
				brackets[1]=t2;
				P=epiSubfunc(brackets[1]);
				while(P>0.0){
					brackets[1]+=brackets[1]*BRACKET;
					P=epiSubfunc(brackets[1]);
				}
				t2=zeroin(brackets[0], brackets[1], epiSubfunc);
			}
			
			if(t2<t1){
				t1=t2;
				shortest=currDeme;
			}
		}
	}	
	currDeme=shortest;
	if(recRate!=0.0){
		gi=CalcGi(currDeme);
		a=(double) recRate*gi*t1;
	}else
		a=0.0;gi=0;
	
	m=(double) migrationRate*ki[currDeme]*t1;
	if(ki[currDeme]!=1){
		temp=( ( (double) ((ki[currDeme])*(ki[currDeme]-1)) ) / (lambda*factr*genTimeInvVar*N) );
		b= temp*(exp(lambda*t1) - 1.0);
	}else
		b=0.0;
	probRE=a/(a+m+b);
	probMI=m/(a+m+b);
	return t1;
}

static double approxFuncSub()
{
double t, zz;
long gi;
	
	if(recRate!=0.0)
		gi=CalcGi(currDeme);
	else
		gi=0;
	zz=( ((double) (ki[currDeme]*(ki[currDeme]-1)))/(factr*genTimeInvVar*N));
	t= -(log(Pn)) / (((double) gi * recRate) + (migrationRate*ki[currDeme]) + zz);
	return t;

}


double epiSubfunc(double t)
{
double a, b, m, temp, diff;
long gi;
	if(recRate!=0.0){
		gi=CalcGi(currDeme);
		a=(double) recRate*gi*t;
	}else
		a=0.0;gi=0;
	m=(double) migrationRate*ki[currDeme]*t;
	temp=( ( (double) ((ki[currDeme])*(ki[currDeme]-1)) ) / (lambda*factr*genTimeInvVar*N) );
	b= temp*(exp(lambda*t) - 1.0);
	diff=(exp(-a-m-b))-Pn;
	return diff;
}

#undef BIG_NUMBER
#undef BRACKET
