/*---------------------------------------*/
/* TREEVOLVE v1.3 - main source file     */
/*                                       */
/*  The coalescent process with:         */
/*      recombination                    */
/*      population subdivision           */
/*      exponential growth               */
/*                                       */
/* 1997 (c) Nick Grassly                 */
/*          Dept. of Zoology             */
/*          Oxford. OX1 3PS              */
/*      nicholas.grassly@zoo.ox.ac.uk    */
/*      http://evolve.zoo.ox.ac.uk/      */
/*---------------------------------------*/
/* 2006 NetEvolve Modifications by		 */
/*		Patricia Buendia				 */
/*		http://biorg.cs.fiu.edu/ 		 */
/*---------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "treevolve.h"
#include "coseq.h"
#include "models.h"
#include "nodestack.h"
#include "const.h"
#include "const.sub.h"
#include "exp.h"
#include "exp.sub.h"
#include "mathfuncs.h"
#ifdef __MWERKS__
#include <console.h>
#endif	

#define PROGRAM_NAME "treevolve\\netEvolve"
#define VERSION_NO 1.3


/* -------------- variables --------------- */
int numBases, sampleSize;
double mutRate;
int SSizeperP=0; /* Sample Size per period*/
int PeriodsWanted=0; /* Number of Sampling Periods */
int StartAt=0; /* First Period to start sampling */
int ClassicTV=0;
int NoClock=0; /* Clock */
int pCounter;
int outputFile=0;
int currPeriod2;
double iNodeProb=0;
int range=2;
int repNo;
char OFile[100];
Node ***NodesPerPeriod;

Node *first, *avail;
char *ModelNames[numModels]={
	"F84",
	"HKY",
	"REV"
};
int ki[MAX_NUMBER_SUBPOPS], K, noRE, noCA, noMI;
double globTime, factr;
double genTimeInvVar;

static int noPeriods;
static Regime history[MAX_NUMBER_REGIMES];
static int numIter, haploid;
static int outputCoTimes;
static char coTimeFile[50];

/* -------------- prototypes ------------- */
static void PrintTitle();
static void PrintUsage();
static void PrintParams();
int ImplementRegime(Regime* pp);
void ReadParams(int argc, char **argv);
void ReadPeriod();
int CheckPeriods(Regime *pp);
void ReadUntil(FILE *fv, char stopChar, char *what);


#define oops(s) { perror((s)); exit(EXIT_FAILURE); }
#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { oops("error: malloc() "); }

/*-----------------------------------------*/
main(int argc, char **argv)
{
int i, j, l,result, currPeriod;
double maxRecRate;
char ppath[90],  pbase[100], temp[100];
FILE *coTimes;

#ifdef __MWERKS__
	argc=ccommand(&argv);
#endif	

	if (setvbuf( stderr, NULL, _IOLBF, 512 ) ||
		setvbuf( stdout, NULL, _IOLBF, 512 ) ) {
		fprintf(stderr, "Failed to buffer stdout and stderr\n");
		exit(0);
	}

	SetSeed(time(NULL));
	avail=NULL;
	PrintTitle();
	ReadParams(argc, argv);
	maxRecRate=0;
	for(i=0;i<noPeriods;i++){
		if(CheckPeriods(&history[i])==0) exit(0);
		if(history[i].r>0 && maxRecRate==0)
			maxRecRate=history[i].r;
		else if(maxRecRate<history[i].r)
			maxRecRate=history[i].r;
	}
	PrintParams();
	if(StartAt==0 && PeriodsWanted>0 && SSizeperP>0){
		if(maxRecRate==0)
			range=4;
		else{
			range=8+(int)(log10(maxRecRate/mutRate));
			if(range<2)
				range=2;
		}
		sampleSize=PeriodsWanted*SSizeperP;
		MALLOC(NodesPerPeriod, sizeof(Node **) * (sampleSize*range));/* nodes in slimTree */
		for (i=0; i < sampleSize*range; i++) 
			MALLOC(NodesPerPeriod[i], sizeof(Node *) * (sampleSize*range));/* (recombination adds nodes:How many? should be prop. to rec rate) */

	}
	else if(PeriodsWanted==0 ||  SSizeperP==0)
		ClassicTV=1;

	if(outputCoTimes){
		if( (coTimes=fopen(coTimeFile, "w"))==NULL){
			fprintf(stderr, "Failed to open coalescent times file\n\n");
			exit(0);
		}
		fprintf(stderr, "Coalescent times output to file: %s\n\n\n", coTimeFile);
	}
	RateHetMem();
	SetModel(model);

	for(repNo=1;repNo<=numIter;repNo++){
		//if (maxRecRate>0)
		//	printf ("Creating Recombinant Network #%d... Please wait\n",repNo);
		//else
		//	printf ("Creating Tree #%d... Please wait\n",repNo); Error: Prints to stdout
		if(!ClassicTV){
			for (i=0; i < sampleSize*range; i++) {
				for(j=0;j<sampleSize*range ;j++)
					NodesPerPeriod[i][j]=NULL;
			}
		}

		K=sampleSize;
		first=FirstNodePop();/*memory allocation for first node*/
		first->type=2;
		first->time=0.0;
		first->sampled=1;
		first->Period=1;
		first->deme=0;
		for(i=1;i<sampleSize;i++){
			first=NodePop(first);/*memory allocation for rest of sample at t=0*/
			first->type=2;
			first->time=0.0;
			first->sampled=1;
			first->Period=1;
			first->deme=0;
		}
		globTime=0.0;
		noRE=noCA=noMI=0;
		currPeriod2=0;
		currPeriod=0;
		pCounter=0;
		result=1;
		while(currPeriod<noPeriods && result==1){
			result=ImplementRegime(&history[currPeriod]);
			fprintf(stderr, "Coalescent calculations finished for period %d\n", (currPeriod+1));
			if(result==0){
				fprintf(stderr, "All coalescences have now occurred at time %f\n\n", globTime);
				if(outputCoTimes)
					fprintf(coTimes, "%f\n", globTime);
			}else
				fprintf(stderr, "No. of extant lineages = %d\n", K);
			currPeriod++;
		}	
		fprintf(stderr, "No. of coalescent events    = %d\n", noCA);
		fprintf(stderr, "No. of recombination events = %d\n", noRE);
		fprintf(stderr, "No. of migrations           = %d\n", noMI);
		if(outputFile){
			j=strlen(OFile);
			ppath[0]='\0';
			l=0;
			for(i=0;i<j;i++){
				temp[l++]=OFile[i];
				if(OFile[i]=='\\'){
					temp[l]='\0';
					strcat(ppath,temp);
					l=0;
				} else if(OFile[i]=='.'){
					temp[l-1]='\0';
					strcpy(pbase,ppath);
					strcat(pbase,temp);
					break;
				}

			}
			sprintf(temp,"%s%d.txt",pbase,repNo);
			remove(temp);
		}
		else{
			pbase[0]='\0';
			ppath[0]='\0';

		}
		if(ClassicTV)
			SeqEvolve(pbase,ppath);
		else
			SeqEvolveSSTV(pbase,ppath);
		fprintf(stderr, "Sequence evolution finished for replicate %d\n\n", repNo);
	}
	if(outputCoTimes)
		fclose(coTimes);
	if (!ClassicTV) {
		for (i = 0; i < PeriodsWanted*2; i++) 
			free(NodesPerPeriod[i]);
		free(NodesPerPeriod);
	}

	fprintf(stderr, "it's all over! -\n	sequences written to stdout\n");
	exit(1);
}

int ImplementRegime(Regime* pp)
{
	if(pp->e==0.0){
		if(pp->d==1)
			return(ConstRoutine(pp));
		else
			return(ConstSubRoutine(pp));
	}else{
		if(pp->d==1)
			return(EpiRoutine(pp));
		else
			return(EpiSubRoutine(pp));
	}
}

void PrintTitle()
{
	fprintf(stderr, "%s%.2f - Coalescent Simulation and Sequence Evolution\n",PROGRAM_NAME ,VERSION_NO);
	fprintf(stderr, "-----------------------------------------------------------\n\n");
}

void PrintUsage()
{
	fprintf(stderr, "Usage: %s -options <PARAMETER.FILE >OUPUT_FILE\n", PROGRAM_NAME);
	fprintf(stderr, "\tFor a description of options available please\n");
	fprintf(stderr, "\trefer to manual included in package\n\n");
	exit(0);
}

void ReadParams(int argc, char **argv)
{
	int  j;
	char ch, str[255], *str2;
	//char *P;
	
	numBases=500;
	sampleSize=20;
	mutRate=0.0000001;

	numCats=1;
	rateHetero=NoRates;
	catRate[0]=1.0;
	gammaShape=1.0;	
	Rmat[0]=Rmat[1]=Rmat[2]=Rmat[3]=Rmat[4]=1.0;

	freq[0]=freq[1]=freq[2]=freq[3]=0.25;
	tstv=2.0;
	model=F84;
	numIter=1;
	haploid=0;
	outputCoTimes=0;
	genTimeInvVar=1.0;
	
	if(feof(stdin)){
		fprintf(stderr, "Unable to read parameters from stdin\n");
		exit(0);
	}
	str2="BEGIN TVBLOCK\n";
	fgets(str, 255, stdin);
	if(strcmp(str, str2)!=0){
		fprintf(stderr, "Input does not contain a paramters block\n");
		exit(0);
	}
	ch=fgetc(stdin);
	while(isspace(ch))
		ch=fgetc(stdin);
	while(ch=='['){
		ReadUntil(stdin, ']', "closing bracket");
		ch=fgetc(stdin);
		while(isspace(ch))
			ch=fgetc(stdin);
	}
	noPeriods=0;
	
	while(!feof(stdin)){
		if(ch=='*'){
			ReadPeriod();
			noPeriods++;
		}else{
			ch=toupper(ch);
			switch (ch) {
				case 'A':
					if (rateHetero==CodonRates) {
						fprintf(stderr, "You can only have codon rates or gamma rates not both\n");
						exit(0);
					}
					if (rateHetero==NoRates)
						rateHetero=GammaRates;
					if (fscanf(stdin, "%lf", &gammaShape)!=1 || gammaShape<=0.0) {
						fprintf(stderr, "Bad Gamma Shape\n");
						exit(0);
					}
				break;
				case 'B':
					if (fscanf(stdin, "%lf", &genTimeInvVar)!=1) {
						fprintf(stderr, "Bad compound generation time parameter\n\n");
						PrintUsage();
					}
				break;
				case 'C':
					if (rateHetero==GammaRates) {
						fprintf(stderr, "You can only have codon rates or gamma rates not both\n");
						exit(0);
					}
					numCats=3;
					rateHetero=CodonRates;
					if (fscanf(stdin, "%lf,%lf,%lf", &catRate[0], &catRate[1], &catRate[2])!=3) {
						fprintf(stderr, "Bad codon-specific rates\n\n");
						PrintUsage();
					}
				break;
				case 'G':
					if (rateHetero==CodonRates) {
						fprintf(stderr, "You can only have codon rates or gamma rates not both\n");
						exit(0);
					}
					
					rateHetero=DiscreteGammaRates;
					if ((fscanf(stdin, "%d", &numCats))!=1 || numCats<2 || numCats>MAX_RATE_CATS) {
						fprintf(stderr, "Bad number of Gamma Categories\n");
						exit(0);
					}
				break;
				case 'D':
					if (fscanf(stdin, "%d", &StartAt)!=1) {
						fprintf(stderr, "Bad [Starting Period] wanted number\n\n");
						PrintUsage();
					}
				break;
				case 'F':
					if (fscanf(stdin, "%lf,%lf,%lf,%lf", &freq[0], &freq[1], 
													&freq[2], &freq[3])!=4) {
						fprintf(stderr, "Bad Base Frequencies\n\n");
						PrintUsage();
					}
				break;
				case 'H':
					haploid=1;
					while(!isspace(ch))
						ch=getc(stdin);
				break;
				case 'I':
					if (fscanf(stdin, "%lf", &iNodeProb)!=1|| iNodeProb<0) {
						fprintf(stderr, "Bad [Internal Nodes Sampling Probability] number\n\n");
						PrintUsage();
					}
				break;
				case 'K':
					NoClock=1;
					while(!isspace(ch))
						ch=getc(stdin);
				break;
				
				case 'L':
					if (fscanf(stdin, "%d", &numBases)!=1 || numBases<1) {
						fprintf(stderr, "Bad sequence length\n\n");
						PrintUsage();
					}
				break;
				case 'N':
					if (fscanf(stdin, "%d", &numIter)!=1 || numIter <1) {
						fprintf(stderr, "Bad number of replicates\n\n");
						PrintUsage();
					}
				break;
				case 'O':
					outputCoTimes=1;
					ch=fgetc(stdin);
					if(isspace(ch))
						strcpy(coTimeFile, "coalescent.times");
					else{
						j=0;
						do{
							coTimeFile[j]=ch;
							j++;
							ch=fgetc(stdin);
						}while(!isspace(ch));
						coTimeFile[j]='\0';
					}
				break;
				case 'P':
					if (fscanf(stdin, "%d", &PeriodsWanted)!=1 || PeriodsWanted <2) {
						fprintf(stderr, "Bad [Periods wanted] number\n\n");
						PrintUsage();
					}
				break;
				case 'R':
					if (fscanf(stdin, "%lf,%lf,%lf,%lf,%lf,%lf", &Rmat[0], &Rmat[1], 
													&Rmat[2], &Rmat[3], &Rmat[4], &Rmat[5])!=6) {
						fprintf(stderr, "Bad general rate matrix\n\n");
						PrintUsage();
					}
					if (Rmat[5]!=1.0) {
						for (j=0; j<5; j++) 
							Rmat[j]/=Rmat[5];
						Rmat[5]=1.0;
					}
				break;
				case 'S':
					if (fscanf(stdin, "%d", &sampleSize)!=1 || sampleSize<1) {
						fprintf(stderr, "Bad sample size\n\n");
						PrintUsage();
					}
				break;
				case 'U':
					if (fscanf(stdin, "%lf", &mutRate)!=1) {
						fprintf(stderr, "Bad mutation rate\n\n");
						PrintUsage();
					}
				break;
				case 'T':
					if (fscanf(stdin, "%lf", &tstv)!=1) {
						fprintf(stderr, "Bad Ts/Tv ratio\n\n");
						PrintUsage();
					}
				break;
				case 'V':
					model=-1;
					fgets(str, 4, stdin);
					for (j=F84; j<numModels; j++) {
						if (strcmp(str, ModelNames[j])==0)
							model=j;
					}
					if (model==-1) {
						fprintf(stderr, "Unknown Model: %s\n\n", str);
						PrintUsage();
						exit(0);
					}
				break;
				case 'Z':
					if (fscanf(stdin, "%d", &SSizeperP)!=1|| SSizeperP <2) {
						fprintf(stderr, "Bad [Sampling Size Per Period] number\n\n");
						PrintUsage();
					}
				break;
				case 'X':
					outputFile=1;
					ch=fgetc(stdin);
					if(isspace(ch))
						outputFile=0;
					else{
						j=0;
						if(ch=='"'){
							ch=fgetc(stdin);
							do{
								OFile[j]=ch;
								j++;
								ch=fgetc(stdin);
							}while(ch!='"');
						}
						else
							fprintf(stderr, "Output file needs to be enclosed in quotation marks\n\n");

					
						OFile[j]='\0';
					}

				break;

				default :
					fprintf(stderr, "Incorrect parameter: %c\n\n", ch);
					PrintUsage();
				break;
			}
		}
		ch=fgetc(stdin);
		while(isspace(ch) && !feof(stdin))
			ch=fgetc(stdin);
		while(ch=='['){
			ReadUntil(stdin, ']', "closing bracket");
			ch=fgetc(stdin);
			while(isspace(ch))
				ch=fgetc(stdin);
		}
	}
		
}

void ReadPeriod()
{
int periodNo, i;
char ch, str[255];
	
	ch=fgetc(stdin);
	if(ch!='P'){
		fprintf(stderr, "Asterisks denote period data\n");
		exit(0);
	}
	while(!isspace(ch))
		ch=fgetc(stdin);
	if(fscanf(stdin, "%d", &periodNo)!=1){
		fprintf(stderr, "Periods must have numerical label\n");
		exit(0);
	}
	periodNo--;
	history[periodNo].t=-1.0;	/* set defaults */
	history[periodNo].N=1000000;
	history[periodNo].e=0.0;
	history[periodNo].d=1;
	history[periodNo].m=0.0;
	history[periodNo].r=0.0;
	
	ch=fgetc(stdin);
	while(isspace(ch))
		ch=fgetc(stdin);
	while(ch=='['){
		ReadUntil(stdin, ']', "closing bracket");
		ch=fgetc(stdin);
		while(isspace(ch))
			ch=fgetc(stdin);
	}
	while(ch!='*'){
		ch=toupper(ch);
		switch (ch) {
			case 'T':
				if (fscanf(stdin, "%lf", &history[periodNo].t)!=1) {
					fprintf(stderr, "Bad length of period %d\n\n", periodNo+1);
					PrintUsage();
				}
			break;
			case 'N':
				ch=fgetc(stdin);
				i=0;
				while(!isspace(ch)){
					str[i]=ch;
					i++;
					ch=fgetc(stdin);
				}
				str[i]='\n';
				if (sscanf(str, "%lf", &history[periodNo].N)!=1) {
					str[0]=toupper(str[0]);
					if(str[0]=='P'){
						history[periodNo].N=((history[periodNo-1].N)*exp(-(history[periodNo-1].e)*(history[periodNo-1].t)));
						if(history[periodNo].N<MIN_NE){
							history[periodNo].N=MIN_NE;
							fprintf(stderr, "Period %d Ne too small. Therefore setting to %e\n", periodNo, MIN_NE);
						}
					}else{
						fprintf(stderr, "Bad population size for period %d\n\n", periodNo+1);
						PrintUsage();
					}
				}
			break;
			case 'E':
				if (fscanf(stdin, "%lf", &history[periodNo].e)!=1) {
					fprintf(stderr, "Bad exponential growth for period %d\n\n", periodNo+1);
					PrintUsage();
				}
			break;
			case 'D':
				if (fscanf(stdin, "%d", &history[periodNo].d)!=1) {
					fprintf(stderr, "Bad # sub populations for period %d\n\n", periodNo+1);
					PrintUsage();
				}
			break;
			case 'M':
				if (fscanf(stdin, "%lf", &history[periodNo].m)!=1) {
					fprintf(stderr, "Bad migration rate for period %d\n\n", periodNo+1);
					PrintUsage();
				}
			break;
			case 'R':
				if (fscanf(stdin, "%lf", &history[periodNo].r)!=1) {
					fprintf(stderr, "Bad recombination rate for period %d\n\n", periodNo+1);
					PrintUsage();
				}
			break;
			default :
				fprintf(stderr, "Incorrect period parameter: %c\n\n", ch);
				PrintUsage();
			break;
		}
		
		ch=fgetc(stdin);
		while(isspace(ch) && !feof(stdin))
			ch=fgetc(stdin);
		while(ch=='['){
			ReadUntil(stdin, ']', "closing bracket");
			ch=fgetc(stdin);
			while(isspace(ch))
				ch=fgetc(stdin);
		}
	}
	while(!feof(stdin) && !isspace(ch))
		ch=fgetc(stdin);
	
}


void ReadUntil(FILE *fv, char stopChar, char *what)
{
	char ch;
	
	ch=fgetc(fv);
	while (!feof(fv) && ch!=stopChar) 
		ch=fgetc(fv);

	if (feof(fv) || ch!=stopChar) {
		fprintf(stderr, "%s missing", what);
		exit(0);
	}
}

int CheckPeriods(Regime *pp)
{

	if(pp->d==1 && pp->m!=0.0){
		fprintf(stderr, "The migration rate has no meaning if the number of subpopulations is one\n");
		return 0;
	}
	return 1;	
}

static void PrintParams()
{
int i;
double t, N;

	fprintf(stderr, "sequence length      = %d\n", numBases);
	fprintf(stderr, "sample size          = %d\n", sampleSize);
	fprintf(stderr, "mutation rate u      = %e\n", mutRate);
	fprintf(stderr, "number of replicates = %d\n", numIter);
	fprintf(stderr, "substitution model   = %s\n", ModelNames[model]);
	if (rateHetero==CodonRates) {
		fprintf(stderr, "Codon position rate heterogeneity:\n");
		fprintf(stderr, "    rates = 1:%f 2:%f 3:%f\n", catRate[0], catRate[1], catRate[2]);
	} else if (rateHetero==GammaRates) {
		fprintf(stderr, "Continuous gamma rate heterogeneity:\n");
		fprintf(stderr, "    shape = %f\n", gammaShape);
	} else if (rateHetero==DiscreteGammaRates) {
		fprintf(stderr, "Discrete gamma rate heterogeneity:\n");
		fprintf(stderr, "    shape = %f, %d categories\n", gammaShape, numCats);
	} else
		fprintf(stderr, "Rate homogeneity of sites.\n");
	fprintf(stderr, "Model=%s\n", ModelNames[model]);
	if (model==F84)
		fprintf(stderr, "  transition/transversion ratio = %G (K=%G)\n", tstv, kappa);
	else if (model==HKY)
		fprintf(stderr, "  transition/transversion ratio = %G (kappa=%G)\n", tstv, kappa);
	else if (model==REV) {
		fprintf(stderr, "  rate matrix = gamma1:%7.4f alpha1:%7.4f  beta1:%7.4f\n", Rmat[0], Rmat[1], Rmat[2]);
		fprintf(stderr, "                                beta2:%7.4f alpha2:%7.4f\n", Rmat[3], Rmat[4]);
		fprintf(stderr, "                                              gamma2:%7.4f\n", Rmat[5]);
	}
	if(haploid){
		fprintf(stderr, "haploid model implemented\n");
		factr=2.0;
	}else{
		fprintf(stderr, "diploid model implemented\n");
		factr=4.0;
	}
	fprintf(stderr, "Generation time / variance in offspring number = %f\n", genTimeInvVar);
	if(genTimeInvVar==1.0)
		fprintf(stderr, "\t- corresponds to Wright-Fisher model of reproduction\n");
	fprintf(stderr, "\nPopulation Dynamic Periods:\n");
	fprintf(stderr, "---------------------------\n");
	t=0.0;
	for(i=0;i<noPeriods;i++){
		fprintf(stderr, "Period %d\n", i+1);
		if(history[i].t>0.0)
			fprintf(stderr, "Length: %f\n",  history[i].t);
		else
			fprintf(stderr, "Period running until final coalescence\n",  history[i].t);
		fprintf(stderr, "Time at start: %f\n", t);
		if(history[i].d>1){
			fprintf(stderr, "Population subdivided into %d demes\n", history[i].d);
			fprintf(stderr, "Deme size at t = %f is: %f\n", t,history[i].N);
			fprintf(stderr, "Migration rate: %e\n", history[i].m);
			if(history[i].e!=0.0){
				fprintf(stderr, "Exponential growth (decline backwards) at rate: %f\n", history[i].e);
				if(history[i].t>0.0){
					N=((history[i].N)*exp(-(history[i].e)*(history[i].t)));
					fprintf(stderr, "Expected deme size at end of period: %f\n", N);
				}
			}
		}else{
			fprintf(stderr, "Population panmictic with size %f\n", history[i].N);
			if(history[i].e!=0.0){
				fprintf(stderr, "Exponential growth (decline backwards) at rate: %f\n", history[i].e);
				if(history[i].t>0.0){
					N=((history[i].N)*exp(-(history[i].e)*(history[i].t)));
					fprintf(stderr, "Expected population size at end of period: %f\n", N);
				}
			}
		}
		fprintf(stderr, "Recombination rate: %e\n\n", history[i].r);
		fprintf(stderr, "---------------------------\n");
		t+=history[i].t;
	}
}


long CalcGi(int deMe)
{
int i, j, k, posn;
long count;
short *ptr1, *ptr2;
Node *nptr;

	count=0;
	nptr=first;
	for(i=0;i<K;i++){
		if(nptr->deme==deMe){/* i.e. if in same deme add Gi */
			ptr1=nptr->ancestral;
			posn=0;
			while(posn<numBases && *ptr1==0){
				ptr1++;
				posn++;
			}
			
			if(posn<(numBases-1)){
				for(j=(posn+1);j<numBases;j++){
					ptr2=ptr1;
					for(k=j;k<numBases;k++){
						ptr2++;
						if(*ptr2==1){
							count++;
							break;
						}
					}
					ptr1++;
				}
			}
		}
		nptr=nptr->next;/*move to next gamete*/
	}
	return count;
}
/********************************************************************************************************/
/*** PB: An array of pointers to tree nodes needs to be allocated with size Periods*2 and SsizePP*2   ***/
/*** Each row identifies a sampling time period. Nodes are added backwards, so last periods first     ***/
/********************************************************************************************************/
void SaveNodeinPeriodsArray(Node *dec1, double t)
{
int i, j, newP;
int end_iNodes;
double rnd,prevTime;

	if(dec1->time==0){
		dec1->daughters[0]=NULL;
		if(pCounter>=SSizeperP ){
			pCounter=0;
			currPeriod2++;
		}
		newP=currPeriod2;
		dec1->Period=currPeriod2;
		pCounter++;

	}
	else{
		if(dec1->type==1)
			newP=dec1->daughters[0]->Period+1;
		else{
			if(dec1->daughters[0]->Period>dec1->daughters[1]->Period)
				newP=dec1->daughters[0]->Period+1;
			else
				newP=dec1->daughters[1]->Period+1;
		}
		dec1->Period=newP;

	}
	i=0;end_iNodes=0;
	while (NodesPerPeriod[newP][i]!=NULL) {/* How many internal nodes*/
		if(NodesPerPeriod[newP][i++]->daughters[0]!=NULL)
			end_iNodes++;
	}
	i=end_iNodes;
	if(newP<=currPeriod2 && dec1->time!=0){/* To have all internal nodes to the left in the array*/
		j=end_iNodes+SSizeperP;
		for(j;j>end_iNodes;j--)
			NodesPerPeriod[newP][j]=NodesPerPeriod[newP][j-1];
		
	}
	else{
		while (NodesPerPeriod[newP][i]!=NULL) 
			i++;
	}

	//if(dec1->time==0 && (NoClock ||i==0||(i==end_iNodes && iNodeProb==0))){/* For Leaves only */
	if(dec1->time==0 && (NoClock ||i==0||i==end_iNodes )){/* For Leaves only */
		rnd=rndu();
		if(!NoClock && newP>0){/* for first leaf */
				j=0;end_iNodes=0;
				while (NodesPerPeriod[newP-1][j]!=NULL) {/* Previous Period internal nodes*/
					if(NodesPerPeriod[newP-1][j++]->daughters[0]!=NULL)
						end_iNodes++;
				}
				prevTime=NodesPerPeriod[newP-1][end_iNodes]->time;
				if(iNodeProb>0){
					if(NodesPerPeriod[newP+1][0]!=NULL)/* new time should be below that of next period first internal node time*/
						t=NodesPerPeriod[newP+1][0]->time-1;
				}
				dec1->time=prevTime+rnd*(t-prevTime);
		}
		else
			dec1->time=rnd*t;
	}
	else if(!NoClock){/* If Clock */ 
		if(	i>0){
			if(dec1->time==0)
				dec1->time=NodesPerPeriod[newP][i-1]->time; /*  leaves have same time like other leaves*/
		}
	}
			
	NodesPerPeriod[newP][i]=dec1;
}

/*-------------------------------------------------------------------------------*/
void Recombine(double t, int deme)
{
int i, picked, sum1, sum2;
double rnd;
Node *rec, *anc1, *anc2;

	do{
		do{
			rnd=rndu();
			picked=(int) ( rnd*(ki[deme]) );
		}while(picked==ki[deme]);
		
		rec=first;
		i=0;
		while(rec->deme!=deme)/*pick recombinant from correct deme*/
			rec=rec->next;
		while(i<picked){
			rec=rec->next;
			while(rec->deme!=deme)
				rec=rec->next;
			i++;
		}
		
		do{
			rnd=rndu();
			picked=(int) (rnd*numBases);
		}while(picked==numBases || picked==0);/*cuts sequence at one of 
												m-1 possible points*/
		sum1=sum2=0;
		for(i=0;i<picked;i++)			/*checks ancestral tuples*/
			sum1+=rec->ancestral[i];
		for(i=picked;i<numBases;i++)
			sum2+=rec->ancestral[i];
			
	}while(sum1==0 || sum2==0);
	if(!ClassicTV)
		SaveNodeinPeriodsArray(rec, t);


	
	anc1=first=NodePop(first);			/*memory allocation*/
	anc2=first=NodePop(first);			/*memory allocation*/
	rec->previous->next=rec->next;		/*maintain loop    */
	rec->next->previous=rec->previous;	/*maintain loop    */
	
	anc1->daughters[0]=rec;				/*point two ancestral gametes to recombinant*/
	anc2->daughters[0]=rec;
	anc1->daughters[1]=anc2;			/*point RE ancestors to each other using spare pointer*/
	anc2->daughters[1]=anc1;
	anc1->time=t;						/*record time of event*/
	anc2->time=t;
	anc1->sampled=0;						/*record time of event*/
	anc2->sampled=0;
	anc1->Period=0;						/*record period*/
	anc2->Period=0;
	anc1->type=1;						/*recombinant node*/
	anc2->type=1;
	anc1->cutBefore=picked;				/*record cut posn*/
	anc2->cutBefore=-1;
	for(i=0;i<picked;i++){				/*sets ancestral tuples*/
		anc1->ancestral[i]=rec->ancestral[i];
		anc2->ancestral[i]=0;
	}
	for(i=picked;i<numBases;i++){
		anc2->ancestral[i]=rec->ancestral[i];
		anc1->ancestral[i]=0;
	}
	anc1->deme=deme;					/*record which deme the ancestors are in*/
	anc2->deme=deme;
	if(!ClassicTV){						/* Serial Sampling */
		SaveNodeinPeriodsArray(anc1, t);
		SaveNodeinPeriodsArray(anc2, t);
	}
	else if(NoClock){					/* No Clock */
		if(rec->time==0){
				rnd=rndu();
				rec->time=rnd*t;
		}

	}

}



/*---------------------------------------------------------------------------------------*/
void Coalesce(double t, int deme)
{
int i, picked1, picked2;
double rnd;
Node *dec1, *dec2;
short *p, *q, *r;


	do{						/*choose CA candidates from correct deme*/
		rnd=rndu();
		picked1=(int) ( rnd*(ki[deme]) );
	}while(picked1==ki[deme]);
	do{
		rnd=rndu();
		picked2=(int) ( rnd*(ki[deme]) );
	}while(picked2==ki[deme] || picked2==picked1);
	
	dec1=dec2=first;
	i=0;
	while(dec1->deme!=deme)/*pick CA candidates from loop*/
		dec1=dec1->next;
	while(i<picked1){
		dec1=dec1->next;
		while(dec1->deme!=deme)
			dec1=dec1->next;
		i++;
	}
	i=0;
	while(dec2->deme!=deme)/*pick CA candidates from loop*/
		dec2=dec2->next;
	while(i<picked2){
		dec2=dec2->next;
		while(dec2->deme!=deme)
			dec2=dec2->next;
		i++;
	}
	if(!ClassicTV){ /*PB: Modify time of leaves and save in periods array*/
		if(dec1->time==0)
			SaveNodeinPeriodsArray(dec1, t);
		if(dec2->time==0)
			SaveNodeinPeriodsArray(dec2, t);
	}
	else if(NoClock){/* No Clock but classic Treevolve*/
		if(dec1->time==0){
				rnd=rndu();
				dec1->time=rnd*t;
		}
		if(dec2->time==0){
				rnd=rndu();
				dec2->time=rnd*t;
		}


	}


	
	first=NodePop(first);/*memory allocation NB before removing dec1 & dec2*/
	first->type=0;/* type CA */
	first->daughters[0]=dec1;
	first->daughters[1]=dec2;
	first->time=t;
	first->sampled=0;					
	first->Period=0;						/*record period*/
	first->deme=deme;
	if(!ClassicTV)
		SaveNodeinPeriodsArray(first, t);

	
	dec1->previous->next=dec1->next;/* loop maintenance */
	dec1->next->previous=dec1->previous;
	dec2->previous->next=dec2->next;
	dec2->next->previous=dec2->previous;
	
	p=first->ancestral;
	q=dec1->ancestral;
	r=dec2->ancestral;
	for(i=0;i<numBases;i++){
		*p= (*q) | (*r);/* ORs the ancestral states*/
		p++;
		q++;
		r++;
	}
}
/*------------------------------*/
void Migration(int deme, int numDemes)
{
double rnd;
int i, migrant, recipDeme;
Node *P;

	do{				/* pick migrant */
		rnd=rndu();
		migrant=((int) (rnd*(ki[deme])) );
	}while(migrant==ki[deme]);
	do{				/* pick recipient deme */
		rnd=rndu();
		recipDeme=( (int) (rnd*numDemes) );
	}while(recipDeme==deme);
	
	P=first;
	i=0;
	while(i<migrant || P->deme!=deme){	/*pick migrant from loop*/
		if(P->deme==deme)
			i++;
		P=P->next;
	}
	P->deme=recipDeme;
	ki[deme]--;
	ki[recipDeme]++;
}



#undef VERSION_NO

