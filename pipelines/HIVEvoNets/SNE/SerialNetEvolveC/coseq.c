/* coseq.c - source file for TREEVOLVE       */
/* sequence evolution bit                    */
/* (c) Nick Grassly and Andrew Rambaut 1997  */
/*     Dept. of Zoology, Univ. of Oxford,    */
/*     nicholas.grassly@zoo.ox.ac.uk         */
/*     http://evolve.zoo.ox.ac.uk/           */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "treevolve.h"
#include "coseq.h"
#include "models.h"
#include "mathfuncs.h"
#include "Heapsort.h"
#ifdef __MWERKS__
#include <console.h>
#endif

/* Quick fix Constants */

/* varables */
int tipNo;

double gammaShape;
int numCats, rateHetero;
double catRate[MAX_RATE_CATS];
double freqRate[MAX_RATE_CATS];

char *nucleotides="ACGT";
static double matrix[MAX_RATE_CATS][16];
static double vector[4];
static double *siteRates;
static short *categories;

/* Quick fix Globals*/
int PeriodsChosen; /* Counts total number of time periods already chosen*/
 
int Pick;
int TotalRec;

Node **OUTStorage; /* To know where the BKP Nodes are, to free them later */
int slimNodes;
int slimIDs;

int MaxQty;
int numSampled;

typedef struct NodeInterval NodeInterval;
struct NodeInterval{
	double Start;
	double End;
	int Qty;
	int Time0;
	int Rec;
	int LookedAt;
	int Isolated;
};

NodeInterval *Interval;
int IntervalPeriods;
int LastPeriod;

Node *Head_MaxQueue;

/* protoypes */
void SeqEvolve();
void SeqEvolveSSTV();
void Mutate(Node *node,  int *nodeNo);
char SetBase(double *P);
void RandomSequence(char *seq);
void MutateSequence(char *seq, double len);
//void WriteSequence(Node *node, char *P);
void WriteSequence(int sampleNo, char *P,Node *node);

#define oops(s) { perror((s)); exit(EXIT_FAILURE); }
#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { oops("error: malloc() "); }

void ReID(Node *node, int Number)
{
	char T[5], NewID[8];
	if (node->Period<10)
		sprintf(T, "00%d",node->Period);
	else if(node->Period<100)
		sprintf(T, "0%d",node->Period);
	else
		sprintf(T, "%d",node->Period);


	sprintf(NewID,"%s.%d",T, Number);
	strcpy(node->ID,NewID);

}

void FixInternalNodeClockTime(Node *node, double time)
{/* For internal nodes when clock is used*/
	double childTime;
	int j, period;

	if(node->daughters[0]!=NULL){
		if(node->daughters[0]->time>time ){
			j=0;
			period=LastPeriod-node->daughters[0]->Period;
			while( NodesPerPeriod[period][j]->daughters[0]!=NULL )
				j++;
			childTime=NodesPerPeriod[period][j]->time; /* time of leaves from daughter period*/
			childTime=childTime+rndu()*(time-childTime);
			FixInternalNodeClockTime(node->daughters[0],childTime);
		}
	}
	if(node->daughters[1]!=NULL && node->type!=3 && node->type!=1){
		if(node->daughters[1]->time>time){
			j=0;
			period=LastPeriod-node->daughters[1]->Period;
			while( NodesPerPeriod[period][j]->daughters[0]!=NULL )
				j++;
			childTime=NodesPerPeriod[period][j]->time; /* time of leaves from daughter period*/
			childTime=childTime+rndu()*(time-childTime);
			FixInternalNodeClockTime(node->daughters[1],childTime);
		}
	}
	node->time=time;



}

void FixClockPeriods()
{
int i,j;
int period;
	for(i=0;i<LastPeriod;i++){
			j=0;
			while( NodesPerPeriod[i][j]!=NULL){
				period=(int)(((NodesPerPeriod[LastPeriod][0]->time+200)-NodesPerPeriod[i][j]->time)/100);
				if(NodesPerPeriod[i][j]->type!=2){
					if(period>=NodesPerPeriod[i][j]->daughters[0]->Period)
						period=NodesPerPeriod[i][j]->daughters[0]->Period-1;
					if(NodesPerPeriod[i][j]->type==0){
						if(period>=NodesPerPeriod[i][j]->daughters[1]->Period)
							period=NodesPerPeriod[i][j]->daughters[1]->Period-1;
					}
				}
				NodesPerPeriod[i][j]->Period=period;
				j++;
			}
	}
	NodesPerPeriod[LastPeriod][0]->Period=0;
}

void PickPeriods2()
{
	int  	 Period, i,j, iNodes,Isolated;
	double iNodeProb2,prev_rndPick,rndPick,rnd ;

		/* Go Period per period */
		NodesPerPeriod[LastPeriod][0]->Period=0;
		for(i=0;i<LastPeriod;i++){
			Period=LastPeriod-i;
			j=0;iNodes=0;
			while( NodesPerPeriod[i][j]!=NULL && j<sampleSize){
				if(NodesPerPeriod[i][j]->daughters[0]!=NULL)
					iNodes++;
				j++;
			}	
			j=0;
			while(NodesPerPeriod[i][j]!=NULL && j<iNodes+SSizeperP){
				if(iNodeProb>1){
					NodesPerPeriod[i][j]->sampled=1;
					numSampled++;
				}
				else{
					NodesPerPeriod[i][j]->sampled=0;
					if((iNodeProb==0 || iNodes==0) && NodesPerPeriod[i][j]->daughters[0]==NULL){
							NodesPerPeriod[i][j]->sampled=1;/* Sample from leaves only*/
							numSampled++;
					}
				}
				NodesPerPeriod[i][j]->Period=Period;
				j++;		
			}
			if(iNodeProb<=1){
				if(iNodes==0||iNodeProb==0)
					iNodeProb2=0;
				else
					iNodeProb2=(iNodes*iNodeProb)/j;
					

				if(iNodeProb2>0){/* random sampling or strategic sampling*/
					j=0;
					Isolated=0;
					prev_rndPick=0;
					if(i<PeriodsWanted){/* Sample */
						while(Isolated<SSizeperP){
								rnd=rndu();
								j=0;
								rndPick=0;
								while( j<iNodes+SSizeperP){
									if(j<iNodes)
										rndPick+=iNodeProb2/iNodes;
									else
										rndPick+=(1-iNodeProb2)/SSizeperP;
									if(prev_rndPick<rnd && rnd<rndPick ){/* Sample and break*/
										if(NodesPerPeriod[i][j]->sampled==1)
											break;
										if(j<iNodes && !NoClock) {/* internal node: rare event*/
											FixInternalNodeClockTime(NodesPerPeriod[i][j],NodesPerPeriod[i][iNodes]->time);
											if (NodesPerPeriod[i][j]->type==1)
												NodesPerPeriod[i][j]->daughters[1]->time=NodesPerPeriod[i][j]->time;
										}
										NodesPerPeriod[i][j]->sampled=1;
										Isolated++;
										numSampled++;
										break;
									}
									prev_rndPick=rndPick;
									j++;
								}
						}
					}
				}
			}
		}
		if(!NoClock)
			FixClockPeriods();

}

void PicknAssignID(Node *node,  int *nodeNo, int BKP, int recParentBKP)
{
	double r_s,threshold,effectiveQty;
	int Period;
	
	if(StartAt>0){/* Heavy Sampling from large tree */
		Period=node->Period;
		if(Period==LastPeriod)
			effectiveQty=Interval[Period].Qty-Interval[Period].LookedAt;
		else
			effectiveQty=Interval[Period].Qty-Interval[Period].Time0-Interval[Period].LookedAt;
		if(Period==LastPeriod || node->time!=0.0)	
			Interval[Period].LookedAt++;
	}


	(*nodeNo)++;
	ReID(node,*nodeNo);

	if(StartAt>0){
		if(Period!=LastPeriod && NoClock==1 && node->time==0.0)
				node->sampled=0;
		else if(Interval[Period].Isolated>-1 && Interval[Period].Isolated <SSizeperP){
				r_s=rndu();
					//if(recParentBKP>-1 && (recParentBKP<numBases/8||recParentBKP>(numBases*5)/8))
					//	threshold=0.75;/* For seqs whose children will recombine with breakpoint at ends */
					//else{
						//if(BKP==0)
							threshold=(double)(SSizeperP-Interval[Period].Isolated)/effectiveQty;/* Sample x per time period */
						//else {
						//	if(BKP>numBases/8 && BKP<(numBases*5)/8)
						//		threshold=0.75; /* Recomb child with BKP in the middle */
						//	else
						//		threshold=1; /* BKP at ends */
						//}
					//}
					if ( r_s <threshold  ) { /* sample this sequence */
						node->sampled=1;
						Interval[Period].Isolated++;
						numSampled++;
					}
					else
						node->sampled=0;
		}
		else
			node->sampled=0;
	}
	


}




void EnlargeIntervalDimension(int Period)
{
	int i;
	NodeInterval *tmp;
	/* Check if Interval array large enough */
	if (Period+1>=IntervalPeriods){
	  if ((tmp = realloc(Interval, sizeof(NodeInterval) * (IntervalPeriods + 10))) == NULL) {
		fprintf(stderr, "ERROR: realloc failed");
		exit(0);
	  }
	  else{ /* Initialize new array positions*/
		Interval = tmp;
		for(i=IntervalPeriods;i<IntervalPeriods+10;i++){
			Interval[i].Qty=0;
			Interval[i].Time0=0;
			Interval[i].LookedAt=0;
		}
		IntervalPeriods=IntervalPeriods+10;

	  }
	}

}



void StorePeriods1b(Node *node)
{
	int Period;
		if(node->L_father!=NULL){
			if(node->L_father->type==1){
				if (node->L_father->Period>node->R_father->Period)
					Period=node->L_father->Period+1;
				else
					Period=node->R_father->Period+1;
			}
			else
					Period=node->L_father->Period+1;
		} else
			Period=0;
		if(LastPeriod<Period) LastPeriod=Period;

		Interval[Period].Qty++;
		if(node->time==0.0)
			Interval[Period].Time0++;

		
		node->Period=Period;
		if(node->L_father!=NULL){
			if (node->L_father->type==1){ /* Recombinant child */
				Interval[Period].Rec=1; /* This interval contains Recombinants */
				TotalRec++;
			}
		}
		
		EnlargeIntervalDimension(Period);

		if(node->type==1){ /* Is recomb father?*/
			if(node->daughters[0]->Period!=-1){/* First time here */
				if (node->cutBefore!=-1){
					node->daughters[0]->L_father=node;/*Only to check fathers Period */
					node->daughters[0]->R_father=node->daughters[1];
				}
				else{
					node->daughters[0]->L_father=node->daughters[1];
					node->daughters[0]->R_father=node;
				}			
				node->daughters[0]->Period=-1;/* stop */
			}
			else
				StorePeriods1b(node->daughters[0]);/* go on */

		}else if(node->type==0){ /* Not recomb */
			node->daughters[0]->L_father=node;/*Only to check fathers Period */
			node->daughters[1]->L_father=node;
			StorePeriods1b(node->daughters[0]);
			StorePeriods1b(node->daughters[1]);
		}
		
	


}

void Enqueue(Node *maxNode, Node *Child)
{
	Node *SortSearch, *tmp;


	SortSearch=maxNode;
	while(SortSearch->MaxNext !=NULL){
		if (SortSearch->MaxNext->time > Child->time  )
				SortSearch=SortSearch->MaxNext;
		else
			break;
	}
	tmp=SortSearch->MaxNext; /* Could be NULL */
	SortSearch->MaxNext=Child;
	Child->MaxNext=tmp;

	if(SortSearch==maxNode)
		Head_MaxQueue=Child;


}

void StorePeriods1()
{

	Node *maxNode;


	while (Head_MaxQueue != NULL){

		maxNode=Head_MaxQueue;
		Head_MaxQueue=Head_MaxQueue->MaxNext;
		if(maxNode->L_father->Period==LastPeriod)
		{/*If maxNode.parent and maxNode in same interval*/
			Interval[LastPeriod].End=maxNode->time+1;
			LastPeriod++;
			Interval[LastPeriod].Start=maxNode->time;
			Interval[LastPeriod].Rec=0;
		}

		Interval[LastPeriod].Qty++;


		if(MaxQty<Interval[LastPeriod].Qty)
			MaxQty=Interval[LastPeriod].Qty;

		if (maxNode->L_father->type==1){ /* Recombinant child */
			Interval[LastPeriod].Rec=1; /* This interval contains Recombinants */
			TotalRec++;
		}

		maxNode->Period=LastPeriod;

		/*Enqueue maxNode.child1, maxNode.child2(if not recombinant)*/
		if (maxNode->type==1 ){/*recombinant father */
			if ( maxNode->daughters[0]->Period!=-1){/*First time here */
				if (maxNode->cutBefore!=-1){
					maxNode->daughters[0]->L_father=maxNode;/*Only to check fathers Period */
					maxNode->daughters[0]->R_father=maxNode->daughters[1];
				}
				else{
					maxNode->daughters[0]->L_father=maxNode->daughters[1];
					maxNode->daughters[0]->R_father=maxNode;
				}
				maxNode->daughters[0]->Period=-1;
				Enqueue(maxNode,maxNode->daughters[0]); 
			}
				
		} 
		else if(maxNode->type!=2)
			maxNode->daughters[0]->R_father=NULL;
	

		
		if (maxNode->type==0){ /*Common Ancestor */
			maxNode->daughters[0]->L_father=maxNode;/*Only to check fathers Period */
			maxNode->daughters[1]->L_father=maxNode;
			Enqueue(maxNode,maxNode->daughters[0]); 
			Enqueue(maxNode,maxNode->daughters[1]);
		}

		EnlargeIntervalDimension(LastPeriod);

	}/* loop while (Head_MaxQueue != NULL)*/
}

void FixSlimAncestors(Node *node)
{

	if(node->type==3||node->type==1){/* Recomb  Parent*/
		if(node->jumpleft!=NULL){
			if(node->cutBefore!=-1){/* Right parent*/
				node->jumpleft->L_father=node;
				FixSlimAncestors(node->jumpleft);
			}
			else
				node->jumpleft->R_father=node;
		}
	}
	else if(node->type==0){/* CA */
		if(node->jumpleft!=NULL){
			node->jumpleft->L_father=node;
			FixSlimAncestors(node->jumpleft);
		}
		if(node->jumpright!=NULL){
			node->jumpright->L_father=node;
			FixSlimAncestors(node->jumpright);
		}
	}
}

Node *BuildSlimTree(Node *node)
{
	int i;

	node->jumpleft=NULL;
	node->jumpright=NULL;
	if(node->type==3||node->type==1){/* left=3 right=1 recomb parent -> traverse if other path not visited yet*/
		if(node->daughters[1]->jumpleft==node->daughters[1])/* Self-loop to check if first encounter*/
			node->jumpleft=BuildSlimTree(node->daughters[0]);
		else
			node->jumpleft=node->daughters[1]->jumpleft;
	}
	else if (node->type==0) {/*not tip*/
		node->jumpleft=BuildSlimTree(node->daughters[0]);
		node->jumpright=BuildSlimTree(node->daughters[1]);
		if (node->jumpleft==NULL && node->jumpright!=NULL){/* For tree printing */
				node->jumpleft=node->jumpright;
				node->jumpright=NULL;
		}
		if(node->jumpright==NULL && node->jumpleft!=NULL){
			if(node->jumpleft->sampled==0 && (node->jumpleft->type!=3 && node->jumpleft->type!=1) ){
				/* Remove left non-sampled node from path */
				if(!ClassicTV){
					for(i=0;i<slimNodes;i++){
						if(	OUTStorage[i]==node->jumpleft)
							break;
					}
					if(i<slimNodes-1){
						for(i;i<slimNodes-1;i++)
							OUTStorage[i]=OUTStorage[i+1];

					}
					slimNodes--;
				}
				node->jumpright=node->jumpleft->jumpright;
				node->jumpleft=node->jumpleft->jumpleft;

			}
		}

		
	}

	if(!node->sampled){
		if(node->type==2)/* tip can't check for jumpleft !=NULL */
			return(NULL);
		else if(node->type==1 || node->type==3){/* recomb node can't check jumpright!=NULL */
			if( node->jumpleft==NULL)
				return(NULL);
		}
		else if(node->jumpleft==NULL || node->jumpright==NULL ){
			if(node->jumpleft!=NULL)
				return(node->jumpleft);
			else if(node->jumpright!=NULL)
				return(node->jumpright);
			else
				return(NULL);
		}
	}

	if(!ClassicTV){/* This was used for large tree sampling */
		OUTStorage[slimNodes++]=node;
		ReID(node,slimIDs++);
	}
	else if (node->Period==0)
		ReID(node,slimIDs++);

	return(node);

}

void PickPeriods()
{
	int j, SearchOn;
	div_t modzero;


	PeriodsChosen=0;
	SearchOn=0;
	Pick=((LastPeriod-StartAt)/PeriodsWanted);/* Sample every Pick Periods for a total of PeriodsWanted sampling periods */
	if(Pick==0)Pick=1;
	for (j=0; j<IntervalPeriods; j++) {
		if(j>StartAt && PeriodsChosen<PeriodsWanted){
			modzero = div(j,Pick);
			if(modzero.rem==0 ){
				if (Interval[j].Qty-Interval[j].Time0<SSizeperP){
					SearchOn++;
					Interval[j].Isolated =-1;
				}
				else{
					if(SearchOn>0)
						SearchOn--;
					Interval[j].Isolated=0;
					PeriodsChosen++;
				}

			}
			else if(SearchOn>0 && ((Interval[j].Qty-Interval[j].Time0)>=SSizeperP)){
				if(SearchOn>0)
					SearchOn--;
				Interval[j].Isolated=0;
				PeriodsChosen++;
			}
			else
				Interval[j].Isolated=-1;
		}
		else
			Interval[j].Isolated=-1;
	}

}

void PrintNode(FILE *treeFile, Node *node, double maxTime)
{
	double len2Parent;



	if (node->jumpleft==NULL || node->cutBefore>-1 ){/* leaf or left rec parent*/
		/*rec left parent becomes leaf when encountered */
		if(node->sampled==0)
			fputc('~', treeFile);				
		fprintf(treeFile, "%s", node->ID); 
	}
	else  {
		if( node->sampled==1){/* internal sampled Node*/
			/* treat as leaf with 0-length branch*/
			fputc('(', treeFile);	
			fprintf(treeFile, "%s:0,", node->ID);
		}
		if(node->jumpright!=NULL ||node->type==1 || node->type==3 ){

			fputc('(', treeFile);				
			PrintNode(treeFile,  node->jumpleft, maxTime);
			fputc(',', treeFile);		/* Recombination? */	
			if(node->type==1 || node->type==3) {
				if(node->daughters[1]->sampled==0)
					fputc('~', treeFile);
				/* If here: right parent: print left parent */
				fprintf(treeFile, "%s#%d:0", node->daughters[1]->ID,node->daughters[1]->cutBefore);
			}
			else
				PrintNode(treeFile,  node->jumpright, maxTime);
			fputc(')', treeFile);	
			if( node->sampled==1){/* internal sampled Node*/
				/* treat as leaf with 0-length branch*/
				fprintf(treeFile, ":0");
				fputc(')', treeFile);	

			}
		}
		else{
			PrintNode(treeFile,  node->jumpleft, maxTime);
			//if(node->jumpleft->jumpleft==NULL)/* If internal sampled parent of leaf*/
			fputc(')', treeFile);	
		}

	}
	
	/* Calculate tree lengths */
	len2Parent=(node->L_father->time-node->time)/maxTime;
	if (len2Parent<0)
		fprintf (stderr,"negative branch length:%s\n",node->ID);
	if(len2Parent==0)
		fprintf(treeFile, ":0");
	else
		fprintf(treeFile, ":%lf", len2Parent);
	


}

void PrintNetworkOrTree(FILE *fv, Node *node, double maxTime)
{
	if(node->jumpleft==NULL||node->jumpright==NULL){
		if(node->jumpleft!=NULL)
			PrintNode(fv, node->jumpleft, maxTime);
		else if(node->jumpright!=NULL)
			PrintNode(fv, node->jumpright, maxTime);

	}
	else{
		fputc('(', fv);	
		PrintNode(fv, node->jumpleft, maxTime);
		fputc(',', fv);			
		PrintNode(fv, node->jumpright, maxTime);
		fprintf(fv, ");\n");
	}
}


void SeqEvolveSSTV (char * pbase, char *ppath)
{
	int i, j, l;
	char *P;
	char  Anc[20],name[100],space[2]; 
	FILE *ance_f, *out_phy, *treeFile ;
	int n; /* Actual Number of Sampled Seqs */
	Node *PNode;

	remove("TestOutput.txt");
	space[0]=' ';
	space[1]='\0';
	numSampled=0;

	tipNo=0;
	RandomSequence(first->sequence);
	SetCategories();
/* MY Code starts Here: */
	if(outputFile)
		sprintf(name,"%s%d.txt",pbase,repNo);
	else
		sprintf(name,"out%d.txt",repNo);
	if(repNo==1){
		fprintf(stdout, "%s %.2f - Serial Coalescent Simulation and Sequence Evolution\n",PROGRAM_NAME ,VERSION_NO);
		fprintf(stdout, "--------------------------------------------------------------------------\n\n");
	}
	if(outputFile)
		fprintf(stdout, "Data set replicate #%d is written to %s, tree to %s%d.tre \n",repNo,name,pbase,repNo);
	else
		fprintf(stdout, "Data set replicate #%d is written to %s, tree to tv%d.tre in program's folder.\n",repNo,name,repNo);
	if (freopen (name,"w",stdout)==NULL){
		fprintf(stderr, "Error opening file: '%s'\n",name);
		exit(0);
	}

	if(outputFile)
		sprintf(name,"%sA%d.txt",ppath,repNo);
	else
		sprintf(name,"A%d.txt",repNo);
	if ( (ance_f=fopen(name, "w"))==NULL ) {
		fprintf(stderr, "Error opening file: '%s'\n",name);
		exit(0);
	}
	if(outputFile)
		sprintf(name,"%s%d.phy",pbase,repNo);
	else
		sprintf(name,"out%d.phy",repNo);
	if ( (out_phy=fopen(name, "w"))==NULL ) {
		fprintf(stderr, "Error opening file: '%s'\n",name);
		exit(0);
	}
	if(outputFile)
		sprintf(name,"%s%d.tre",pbase,repNo);
	else
		sprintf(name,"tv%d.tre",repNo);
	if ( (treeFile=fopen(name, "w"))==NULL ) {
		fprintf(stderr, "Error opening tree file: '%s'\n",name);
		exit(0);
	}


	MaxQty=0;
	TotalRec=0;
	LastPeriod=0;
	first->L_father=NULL;
	first->R_father=NULL;
	/*** StorePeriods code ***/
	if(StartAt>0){/* Heavy Sampling from large tree */
		IntervalPeriods=10;

		MALLOC(Interval, sizeof(NodeInterval) * sampleSize);
		for(i=0;i<sampleSize;i++){
			Interval[i].Qty=0;
			Interval[i].Time0=0;
			Interval[i].LookedAt=0;
			Interval[i].Isolated=0;
		}
		if(NoClock==0){
			Interval[LastPeriod].Start=first->time;
			Interval[LastPeriod].Qty=1;
			
			first->Period=LastPeriod;
			first->daughters[0]->L_father=first;
			first->daughters[1]->L_father=first;
			if (first->daughters[0]->time >first->daughters[1]->time){
				Head_MaxQueue=first->daughters[0];
				Head_MaxQueue->MaxNext=first->daughters[1];
				first->daughters[1]->MaxNext=NULL;
			}else{
				Head_MaxQueue=first->daughters[1];
				Head_MaxQueue->MaxNext=first->daughters[0];
				first->daughters[0]->MaxNext=NULL;
			
			}
			StorePeriods1();
			Interval[LastPeriod].End=0;
		}else/* Store Periods without clock assumption */
			StorePeriods1b(first);
		PickPeriods();
	}
	else{
		LastPeriod=first->Period;
		PickPeriods2();
	}

	/*** End StorePeriods code ***/



	sprintf(Anc,"000.0");
	strcpy(first->ID,Anc);



	/* Mutate & pick isolated sequences */
	l=1;
	n=0;
	Mutate(first,&n);

	Stack(first);

	MALLOC(OUTStorage, sizeof(Node *) * (sampleSize * range));/* nodes in slimTree - should be prop.to rec rate */
	slimNodes=0;slimIDs=0;
	PNode=BuildSlimTree(first);
	if(PNode->jumpleft==NULL && PNode->jumpright==NULL)
		printf("problem with jumps!\n");
	FixSlimAncestors(PNode);
	PrintNetworkOrTree(treeFile,first,first->time);
	/* Use OutNode to print: first heapsort it */
	slimNodes--;
	for(i=0;i<slimNodes;i++)
		heapify_Nodelist(OUTStorage,i);
	heapsort_Nodelist(OUTStorage,slimNodes-1);

	fprintf(out_phy,"%d %d I O\nO 1 \n",numSampled+1,numBases);
	fprintf(out_phy, "%s     ", first->ID);/* Root or First internal node  */
	P=first->sequence;
	for (j=0; j<numBases; j++) {
		fputc(nucleotides[*P], out_phy);
		P++;
	}
	fputc('\n', out_phy);

	/* Now Print */
	fprintf(ance_f, "%s;%lf \n", first->ID,first->time,Pick);
	for (i=0;i<slimNodes;i++){

		if(OUTStorage[i]->L_father->type==3 || OUTStorage[i]->L_father->type==1)/* recomb has two ancestors*/
			fprintf(ance_f, "%s;%s|%d|%s;%lf;%d \n", OUTStorage[i]->ID,OUTStorage[i]->L_father->ID, OUTStorage[i]->L_father->cutBefore,OUTStorage[i]->R_father->ID, OUTStorage[i]->time, OUTStorage[i]->sampled);
		else
			fprintf(ance_f, "%s;%s;%lf;%d \n", OUTStorage[i]->ID,OUTStorage[i]->L_father->ID,OUTStorage[i]->time,OUTStorage[i]->sampled);

		/* Only the sampled */
		if(OUTStorage[i]->sampled==1){
			strcpy(name,OUTStorage[i]->ID);
			for(j=strlen(name)+1;j<11;j++)
				strcat(name,space);
			fprintf(out_phy,"%s",name);
			fprintf(stdout, ">%s\n",OUTStorage[i]->ID);
			P = OUTStorage[i]->sequence;
			for (l=0; l<numBases; l++) {
				fputc(nucleotides[*P], stdout);
				fputc(nucleotides[*P], out_phy);
				P++;
			
			}
			fputc('\n', stdout);
			fputc('\n', out_phy);
		}
	}
	

	fclose(ance_f);
	fclose(out_phy);
	fclose(treeFile);
	if(!ClassicTV)
		fclose (stdout);	
	free(OUTStorage);
	if(StartAt>0)
		free(Interval);
}

void SeqEvolve (char * pbase, char *ppath)
{
	int n;
	FILE * treeFile;
	Node *PNode;
	char name[100];

	if(outputFile){
		sprintf(name,"%s%d.txt",pbase,repNo);
	
		fprintf(stdout,"Process: 90%%\n");
		if (freopen (name,"w",stdout)==NULL){
			fprintf(stderr, "Error opening file: '%s'\n",name);
			exit(0);
		}
	}
	tipNo=0;
	RandomSequence(first->sequence);
	SetCategories();
	fprintf(stdout, " %d %d\n", sampleSize, numBases);
	n=0;
	Mutate(first,&n);
	Stack(first);

	if(outputFile)
		sprintf(name,"%s%d.tre",pbase,repNo);
	else
		sprintf(name,"tv%d.tre",repNo);
	if ( (treeFile=fopen(name, "w"))==NULL ) {
		fprintf(stderr, "Error opening tree file: '%s'\n",name);
		exit(0);
	}
	slimIDs=tipNo;
	PNode=BuildSlimTree(first);
	if(PNode->jumpleft==NULL && PNode->jumpright==NULL)
		printf("problem with jumps!\n");
	FixSlimAncestors(PNode);
	PrintNetworkOrTree(treeFile,first,first->time);
	fclose(treeFile);

}

void Mutate(Node *node, int *nodeNo)
{
int i, Dir, BKP,cutBefore;//, expNo, posn;
//float xm;
char  *p1, *p2, *child;//*point,
//double rnd;


BKP=0;

	node->jumpleft=node; /* to avoid traversing tree twice in BuildSlimTree */
	node->jumpright=node;

	if(node->type==1){						/*i.e. recombination event */
		if(node->daughters[1]->type==1)		/* first encounter */
			node->type=3;
		else{								/* second encounter */
			p1=node->sequence;
			p2=node->daughters[1]->sequence;/* remember this is spare for re and points to other parent */
			child=node->daughters[0]->sequence;
			if(node->cutBefore!=-1){ 
				Dir=1;
				BKP=node->cutBefore;
			}else{
				Dir=0;
				BKP=node->daughters[1]->cutBefore;
			}

			for(i=0;i<numBases;i++){
				if(node->ancestral[i]==1)
					*child=*p1;
				else
					*child=*p2;
				child++;
				p1++;
				p2++;
				
			}
			*child='\0';
			MutateSequence(node->daughters[0]->sequence, (mutRate*((double) node->time - (double) node->daughters[0]->time)));

			//sprintf(Ance1,"%s-R%d%R-%s", node->daughters[1]->ID, BKP, node->ID);
			if(!ClassicTV)
				PicknAssignID(node->daughters[0], nodeNo, BKP,-1);
			if(node->daughters[0]->type==2){
				if(ClassicTV){
					tipNo++;
					WriteSequence(tipNo, node->daughters[0]->sequence,node->daughters[0]);
				//WriteSequence(node->daughters[0], node->daughters[0]->sequence);
				}
			}else
				Mutate(node->daughters[0], nodeNo);

		
			Stack(node->daughters[1]);		/* stack up memory */
			if(node->daughters[0]->type!=3)
				Stack(node->daughters[0]);
		}
	}else{									/* a coalescent event */

		/* we're moving to the daughter Period */


		memcpy(node->daughters[0]->sequence, node->sequence, (numBases+1));
		MutateSequence(node->daughters[0]->sequence, (mutRate*((double) node->time - (double) node->daughters[0]->time)));
		cutBefore=-1;
		if(node->daughters[0]->type==1||node->daughters[0]->type==3 ){
			if(node->daughters[0]->cutBefore>-1 )
				cutBefore=node->daughters[0]->cutBefore;
			else if (node->daughters[0]->daughters[1]->cutBefore>-1)
				cutBefore=node->daughters[0]->daughters[1]->cutBefore;
		}
		if(!ClassicTV)
			PicknAssignID(node->daughters[0],  nodeNo, 0, cutBefore);
		
		if(node->daughters[0]->type==2 ){/* it's a leaf */
			if(ClassicTV){
				tipNo++;
				WriteSequence(tipNo, node->daughters[0]->sequence,node->daughters[0]);
			}
		}else
			Mutate(node->daughters[0],  nodeNo);
		if(node->daughters[0]->type!=3)
			Stack(node->daughters[0]);		/* stack up memory */
		
		memcpy(node->daughters[1]->sequence, node->sequence, (numBases+1));
		MutateSequence(node->daughters[1]->sequence, (mutRate*((double) node->time - (double) node->daughters[1]->time)));
		cutBefore=-1;
		if(node->daughters[1]->type==1 || node->daughters[1]->type==3){
			if(node->daughters[1]->cutBefore>-1 )
				cutBefore=node->daughters[1]->cutBefore;
			else if (node->daughters[1]->daughters[1]->cutBefore>-1)
				cutBefore=node->daughters[1]->daughters[1]->cutBefore;
		}
		if(!ClassicTV)
			PicknAssignID(node->daughters[1], nodeNo, 0,cutBefore);
		if(node->daughters[1]->type==2 ){
			if(ClassicTV){
				tipNo++;
				WriteSequence(tipNo, node->daughters[1]->sequence,node->daughters[1]);
			}

		}else 
			Mutate(node->daughters[1],  nodeNo);

			
		if(node->daughters[1]->type!=3)
			Stack(node->daughters[1]);		/* stack up memory */
	}

}

void RateHetMem()
{
	if (rateHetero==GammaRates){
		if( (siteRates=(double*)calloc(numBases, sizeof(double)))==NULL){
			fprintf(stderr, "Out of memory allocating site specific rates (RateHetMem)\n");
			exit(0);
		}
	}else if (rateHetero==DiscreteGammaRates){
		if( (categories=(short*)calloc(numBases, sizeof(short)))==NULL){
			fprintf(stderr, "Out of memory allocating discrete gamma categories (RateHetMem)\n");
			exit(0);
		}
	}
}


void SetCategories()
{
	int  i;//, cat,j;
	double sumRates;
	
	if (rateHetero==CodonRates) {
		sumRates=catRate[0]+catRate[1]+catRate[2];
		if (sumRates!=3.0) {
			catRate[0]*=3.0/sumRates;
			catRate[1]*=3.0/sumRates;
			catRate[2]*=3.0/sumRates;
		}
	} else if (rateHetero==GammaRates) {
		for (i=0; i<numBases; i++)
			siteRates[i]=rndgamma(gammaShape) / gammaShape;

	} else if (rateHetero==DiscreteGammaRates) {
		DiscreteGamma(freqRate, catRate, gammaShape, gammaShape, numCats, 0);
		for (i=0; i<numBases; i++)
			categories[i]=(int)(rndu()*numCats);
	}
}


char SetBase(double *P)
{
	char j;
	double r;
	
	r=rndu();
	for (j='\0'; r>(*P) && j<'\3'; j++) P++;
	return j;
}


void RandomSequence(char *seq)
{
	int i;
	char *P;
	
	P=seq;
	for (i=0; i<numBases; i++) {
		*P=SetBase(addFreq);
		P++;
	}
	*P='\0';
}

void MutateSequence(char *seq, double len)
{
	int i, cat;
	double *Q;
	short *R;
	char *P;
	
	P=seq;
	
	switch (rateHetero) {
		case GammaRates:
			Q=siteRates;
			
			for (i=0; i<numBases; i++) {
				SetVector(vector, *P, (*Q)*len);
				*P=SetBase(vector);
				P++;
				Q++;
			}
		break;
		case DiscreteGammaRates:
			for (i=0; i<numCats; i++)
				SetMatrix(matrix[i], catRate[i]*len);
			
			R=categories;
			for (i=0; i<numBases; i++) {
				*P=SetBase(matrix[*R]+(*P<<2));
				P++;
				R++;
			}
		break;
		case CodonRates:
			for (i=0; i<numCats; i++)
				SetMatrix(matrix[i], catRate[i]*len);
			
			for (i=0; i<numBases; i++) {
				cat=i%3;
				*P=SetBase(matrix[cat]+(*P<<2));
				P++;
			}
		break;
		case NoRates:
			SetMatrix(matrix[0], len);
			
			for (i=0; i<numBases; i++) {
				*P=SetBase(matrix[0]+(*P<<2));
				P++;
			}
		break;
	}
}

//void WriteSequence(int sampleNo, char *P)
//void WriteSequence(Node *node, char *P)
//{
//int j;
//	fprintf(stdout, "sample%s\n", node->ID);
//	for (j=0; j<numBases; j++) {
//		fputc(nucleotides[*P], stdout);
//		P++;
//	}
//	fputc('\n', stdout);
//}


void WriteSequence(int sampleNo, char *P, Node *node)
{
int j;
	ReID(node,sampleNo);

	fprintf(stdout, "%-8s ", node->ID);
	for (j=0; j<numBases; j++) {
		fputc(nucleotides[*P], stdout);
		P++;
	}
	fputc('\n', stdout);

}

