/* Source module for treevolve            */
/* Exponentially changing population size */
/* No population subdivision              */
/* (c) N Grassly 1997                     */
/* Dept. Zoology, Oxford. OX1 3PS         */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "treevolve.h"
#include "mathfuncs.h"
#include "exp.h"

#define BRACKET 0.1

static double GenerateTimeEpi();
static double approxFunc();

static double recRate, lambda, N;
static double probRE, Pn;

int EpiRoutine(Regime *pp)
{
Node *nptr;
int i;
double t, rnd, tiMe;
	fprintf(stderr, "In EpiRoutine\n");

	nptr=first;
	for(i=0;i<K;i++){	/* make all genes in same deme */
		nptr->deme=0;
		nptr=nptr->next;
	}
	ki[0]=K;
	
	recRate=pp->r;
	lambda=pp->e;
	N=pp->N;
	t=0.0;
	do{
		tiMe=GenerateTimeEpi();
		if(t+tiMe  > (pp->t) && pp->t > 0.0 ){
			globTime+=(pp->t);
			fprintf(stderr, "Population size at t = %1f is %f\n", globTime, (N*exp(-lambda*((pp->t)-t))));
			return 1;
		}
		N=(N*exp(-lambda*tiMe));
		t+=tiMe;
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
	globTime+=t;
	fprintf(stderr, "Population size at t = %1f is %f\n", globTime, N);
	return 0;
}

static double GenerateTimeEpi()
{
double t, brackets[2], P;

	do
		Pn=rndu();
	while(Pn==0.0);
	t=approxFunc();
	brackets[0]=0.0;
	brackets[1]=t;
	P=epifunc(brackets[1]);
	while(P>0.0){
		brackets[1]+=brackets[1]*BRACKET;
		P=epifunc(brackets[1]);
	}
	t=zeroin(brackets[0], brackets[1], epifunc);
	return t;
}

double approxFunc()	
{
double t;
long gi;
	if(recRate!=0.0)
		gi=CalcGi(0);
	else
		gi=0;
	t= -(log(Pn)) / (((double) gi * recRate ) + ( ((double)(K*(K-1))) / (factr*genTimeInvVar*N) ) );
	return t;
	
}

double epifunc(double t)	
{
double a, b, temp, diff;
long gi;
	if(recRate!=0.0){
		gi=CalcGi(0);
		a=(double) recRate*gi*t;
	}else
		a=0.0;gi=0;
	temp=( ( (double) (K*(K-1)) ) / (lambda*factr*genTimeInvVar*N) );
	b= (exp(lambda*t) - 1.0) * temp;
	diff=(exp(-a-b))-Pn;
	if(a!=0.0)
		probRE=a/(a+b);
	else
		probRE=0.0;
	return diff;
}

#undef BRACKET
