/************************************************************************/
/*  MinPD: Nucleotide Pairwise Minimum Distance calculations							*/
/*  For Quasispecies data isolated at consecutive time periods			*/
/*  Copyright Patricia Buendia www.cs.fiu.edu/~giri/bioinf/bioinf.html	*/
/************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include "NJ.h"
#include "MinPD.h"
#include "Heapsort.h"
#include "mathfuncs.h"

#define GammaDis(x,alpha) (alpha*(pow(x,-1/alpha)))
BKPNode *BootscanCandidates(FILE *fp4,  Fasta **seqs, int  *Frag_Seq, int step, int windowSize , short int bootReps,
						 int numSeqs, int s, int numWins, int min_seq, double min_seq_dist, int align, int maxDim, int op, int *avgBS);

int closestRel=0;
double PCCThreshold=0;
int model=TN93;
int seed=-3;
double alpha=0.5;
int fullBootscan=false;
int distPenalty=0;
int BootThreshold=96;
int BootTieBreaker=1;
int crossOpt=1;
int Bootstrap=0;
int recOn = 1;
int reportDistances=0;
double gapPenalty = 1;/* 1 is default: means gap columns are ignored are not counted*/
int printBoot=0;
char codonFile[100];
int clustBoot=0;
int *codonList;

Fasta	*seqs[NUMSEQS]; /* Declare an array of Fasta structures */ 




void ErrorMessageExit(char *errMsg,int opt)
{
	printf("%s\n",errMsg);
	fflush(stdout);
	if(opt==1)
		exit(1);
}

void freeBKPNode(BKPNode *bkpLL)
{
	BKPNode *Navigate;
	while(bkpLL!=NULL){
		Navigate=bkpLL;
		bkpLL=bkpLL->Next;
	//	printf("%d",Navigate->BKP);
		free(Navigate);
	}

}

/*Bitwise criteria selection */
int BitCriteria(int Num, int Bit){/* Is bit set in Num */
	int i;
	int PowBit;
	PowBit=(int)pow(2,Bit);
	if(PowBit>Num) return(0);
	if(PowBit==Num) return(1);
	for(i=3;i>=0;i--){
		if((Num-(int)pow(2,i))>=0){
			Num=Num-(int)pow(2,i);
			if (PowBit==(int)pow(2,i)) return(1);
		}
	}
	return(0);
}
/* Crossover bit criteria */
int TwoCrossovers(int Num, int MaxBit, int *Begin, int *End){/* Is bit set in Num */
	int i;
	int Start,Stop;
	Stop=-1;
	Start=-1;
	for(i=MaxBit;i>=0;i--){
		if((Num-(int)pow(2,i))>=0){
			if(Start==-1) Start=i;
			Num=Num-(int)pow(2,i);
			if (i==0) Stop=0;
		}
		else {
			if (Start!=-1){  
				Stop=i+1;
				break;
			}
		}
	}
	if(Num==0) {
		if ((Start<=MaxBit && Stop==0) || Start==MaxBit && Stop>0) {
			*Begin=Stop ;
			*End=Start;
			return(0);
		}
		else return(1);
	}
	else return(1);
}

double SumofSquaredValues(double ***values, int **values2, int points, int data, int s,double *m_x)
{
	double Sx;
	int t;
		(*m_x)=0;
		for(t=0;t<points;t++){
			if(values2==NULL)
			(*m_x) = (*m_x) + values[t][s][data];//data=Frag_Seq[f]
			else
			(*m_x) = (*m_x) + (double)values2[data][t];//data=Frag_Seq[f]
		}
		(*m_x)=(*m_x)/points;
		Sx=0;
		for(t=0;t<points;t++){
			if(values2==NULL)
				Sx=Sx + pow((values[t][s][data]-(*m_x)),2);
			else
				Sx=Sx + pow(((double)values2[data][t]-(*m_x)),2);
		}
		return(sqrt(Sx));
}





/*		MATRIX INITIALIZATION */
char **MatrixInit(int s, int dim1, int dim2)
{
	char  **matrix  = NULL;
	int   i;
   
	/* 1st dim */
	if((matrix = (char **)malloc(dim1 * sizeof(char *))) == NULL)
      return(NULL);

	/* 2nd dim  */
	for(i=0; i<dim1; i++){
	if((matrix[i] = (char *)malloc(dim2 * s)) == NULL) {
			for(i=0; i<dim1; i++)   if(matrix[i]) free(matrix[i]);
				free(matrix);
			return(NULL);
			}
	}
	return(matrix);
}

/*********************************************************************/
/*		Calculate JC69 distance										 */
/*********************************************************************/
double JC69distance(int gaps, int matches, int al_len, double alpha)
{
	 double p,d,lenght,w1,g1;

	 lenght=al_len-(gaps*gapPenalty);

	p = (lenght-matches)/lenght;
	w1 = 1 - (4*p/3);
	if(w1<0.02) w1=0.02;
	
	if(alpha<=0)
		d = (-3*log(w1))/4 ;
	else{
		g1=GammaDis(w1,alpha);
		d=(3*(g1-alpha))/4;
	}
	return(d);

}

/*********************************************************************/
/*		Calculate K2P distance										 */
/*********************************************************************/
double K2P80distance(int p1, int p2, int gaps, int matches, int al_len, double alpha)
{
	double Q,P, d,lenght,w1,w2;

	lenght=al_len-(gaps*gapPenalty);
	Q=(double)(al_len-p1-p2-matches-gaps)/lenght;	/*transversion ratio */
	P=(double)(p1+p2)/lenght;						/*transition ratio */
	w1=1 - 2*P - Q;
	w2=1 - 2*Q;
	if(w1<0.02) w1=0.02;
	if(w2<0.02) w2=0.02;
	if (alpha<=0)
		d = -(log(w1)/2) -(log(w2)/4);
	else
		d=GammaDis(w1,alpha)/2 +  GammaDis(w2,alpha)/4 - 3*alpha/4;
	 return(d);
}

/*********************************************************************/
/*		Calculate TN93 distance										 */
/*********************************************************************/
double TN93distance(int pT, int pC, int pG, int pA, int p1, int p2, int gaps,
					int matches, int al_len, double alpha, int s, int t)
{
	double		p[4],P1,P2,Q,R,Y,TC,AG,e1,e2,e3,e4,d,n2, lenght;


	lenght=al_len-(gaps*gapPenalty);

	n2=(double)(2*lenght); 
	p[0]=(double)pT/n2;	/*frequency of T*/
	p[1]=(double)pC/n2;	/*frequency of C*/
	p[2]=(double)pA/n2;	/*frequency of A*/
	p[3]=(double)pG/n2;	/*frequency of G*/
	Y=p[0]+p[1];		/* gY */
	R=p[2]+p[3];		/* gR */
	TC=p[0]*p[1];		/* gTgC */
	AG=p[2]*p[3];		/* gAgG */


	Q=(double)(al_len-p1-p2-matches-gaps)/lenght;	/*transversion ratio */
	P1=((double)p1)/lenght;				/* ratio of A->G G->A */
	P2=((double)p2)/lenght;				/* ratio of T->C C->T */

#if DEBUG_MODE ==1
	printf(" al_len=%d  pT=%d pC=%d  n2=%lf P1=%lf P2=%lf matches =%d gaps=%d\n",al_len,pT,pC,n2,P1,P2,matches, gaps);
#endif

	e1=1-(R*P1/(2*AG))-(Q/(2*R));
	e2=1-(Y*P2/(2*TC))-(Q/(2*Y));
	e3=(R*Y) - (AG*Y/R) - (TC*R/Y);
	e4=1-(Q/(2*Y*R));
	if(e1<0.02) e1=0.02;
	if(e2<0.02) e2=0.02;
	if(e4<0.02) e4=0.02;
	if (alpha<=0) { e1=-log(e1); e2=-log(e2); e4=-log(e4); }
	else   { e1=GammaDis(e1,alpha); e2=GammaDis(e2,alpha); e4=GammaDis(e4,alpha);}


	d=((2*AG/R) * e1) + ((2*TC/Y)* e2) + (2*e3*e4);
	if (alpha>0) 
		d = d - (2*alpha*(AG+TC+(R*Y)));
	if(d<0.00000001) d=0;

#if DEBUG_MODE ==1
	printf("p[0]=%lf p[1]=%lf p[2]=%lf p[3]=%lf P1=%lf P2 = %lf Q=%lf \ne1=%lf e2=%lf e3=%lf e4=%lf Y=%lf R=%lf\n",p[0],p[1],p[2],p[3],P1,P2,Q, e1,e2,e3,e4,Y,R);
#endif




//	if (s==9 && t==5){ /* For debugging */
//		printf("s=%d t=%d  d=%lf\n",s,t,d);
//		printf(" al_len=%d  pT=%d pC=%d pG=%d pA=%d n2=%lf P1=%lf P2=%lf matches =%d gaps=%d\n",al_len,pT,pC,pG,pA,n2,P1,P2,matches, gaps);
//		printf("p[0]=%lf p[1]=%lf p[2]=%lf p[3]=%lf P1=%lf P2 = %lf Q=%lf \ne1=%lf e2=%lf e3=%lf e4=%lf Y=%lf R=%lf\n\n",p[0],p[1],p[2],p[3],P1,P2,Q, e1,e2,e3,e4,Y,R);
//	}

	return(d);

}


/******************************************************************************************/
/*  					Calculate pairwise			  							          */
/******************************************************************************************/
double PairwiseDistance(char **seqsChars,int t, int s, int windowSize, int model)
{
	int i;
	char c1,c2;
	int	P1,P2, pT,pC,pG,pA,gaps,matches;//, test;

				pT=0;pC=0;pG=0;pA=0;P1=0;P2=0;gaps=0;matches=0;

				for(i=0;i<windowSize;i++){
					c1=seqsChars[t][i];
					c2=seqsChars[s][i];


					if (c1==c2){ 
						if (c2=='T') pT+=2;
						if (c2=='C') pC+=2;
						if (c2=='A') pA+=2;
						if (c2=='G') pG+=2;
						if (c1=='-') 
							gaps++;
						else
							matches++;
					}
					else {
						/* Gather nucleotides freqs and transition/transversion freqs */
						if (c1=='-'||c2=='-')
							gaps++;
						else {/* Count only transitions*/
							if (c1=='T') {
								pT++;
								if(c2=='C')P2++;
							}
							if (c1=='C') {
								pC++;
								if(c2=='T')P2++;
							}
							if (c1=='A') {
								pA++;
								if(c2=='G')P1++;
							}
							if (c1=='G') {
								pG++;
								if(c2=='A')P1++;
							}
							if (c2=='T') pT++;
							if (c2=='C') pC++;
							if (c2=='A') pA++;
							if (c2=='G') pG++;
						}
					}

				}/* i loop */

			
				if(model==JC69)
					return(JC69distance(gaps,matches,windowSize,alpha));
				else if (model==K80)
					return(K2P80distance(P1,P2,gaps,matches,windowSize,alpha));
				else
					return(TN93distance( pT,  pC,  pG,  pA,  P1,  P2,  gaps,  matches,windowSize, alpha,s,t));



}

/**********************************************************************/
/*  																  */
/*  	Distance Calculation when Sequences already aligned			  */
/*  																  */
/**********************************************************************/
int DistOnly(Fasta **seqs, double **dist, double ***dist_frags, int maxDim, int n,
		    int win_Count, int model, int windowSize, int step, int numWins)
{
	char		**seqChars;
	int			i,j,k,s,t,start,w;
	/* get all pairs distance */
	if((seqChars = (char **)MatrixInit(sizeof(char), 2, maxDim+1))==NULL)
      return(0);

	for (s=1;s<=n;s++){
		t=s-1;
		/* Now get this sequence's distance from all others before it  */
		for(t;t>=0;t--)
		{
			strcpy(seqChars[0],seqs[s]->Seq);
			strcpy(seqChars[1],seqs[t]->Seq);

   			/* Store Alignment Calculate Distance */
			dist[s][t]=dist[t][s]=PairwiseDistance(seqChars,0,1,maxDim,model);
			start=0;
			i=0;
			for(w=0;w<numWins;w++){

					k=0;
					for (j=start;j<(start+windowSize);j++){
						seqChars[0][k] =seqs[s]->Seq[j];
						seqChars[1][k++] =seqs[t]->Seq[j];
					}

					dist_frags[w][s][t]=dist_frags[w][t][s]=PairwiseDistance(seqChars,0,1,windowSize,model);

					start+=step;
			}
		}
	}
	for(i=0; i<2; i++)   if(seqChars[i]) free(seqChars[i]);
	free(seqChars);
   return(true);

}




/**********************************************************************/
/* 																      */
/*  	Save Minimum distances ancestors							  */
/*  																  */
/**********************************************************************/
int SaveMinResults(Mintaxa **minresults, int *resno, int a, int c, double dist, double div)
{
	if ((minresults[*resno] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
	   printf("SaveMinResults: Error with memory allocation\n");
	   return(false);
	}
	minresults[*resno]->Anc_Idx=a;
	minresults[*resno]->Child_Idx=c;
	minresults[*resno]->dist=dist;
	minresults[*resno]->div=div;
	(*resno)++;
	return(true);

}
/**********************************************************************/
/* 																      */
/*  	OptionalRecPrintout					              		      */
/*  																  */
/**********************************************************************/
void OptionalRecPrintout(FILE *fp4,  RecRes *recRes,Fasta **seqs,int *Frag_Seq,FDisNode (*recSolutions)[3][2], 
						 int i, int l, int r, int p, int q, int f, int index, int last, int BKP, int win_Count,int align,int fr_size)
{
	double d_min;
	d_min=INF9999;

	if(index<last && q<2){ /* 2 here should be variable*/
		index++;
		/* Look for other min-solutions per same BKP in a loop*/
		do{
			if(p==q)
				q++;
			else{/* always (p<=q) */
					if(l<r){
						l=1;
						r=0;
					}
					else{
						p++;
						l=0;
						r=1;
					}
			}
			if(recSolutions[f][p][l].Idx!=-1 && recSolutions[f][q][r].Idx!=-1){
				d_min=(recSolutions[f][p][l].dist+recSolutions[f][q][r].dist)/win_Count;
				if(d_min<recRes[i].dist )
					break;
			}
			else
				break;

		}while(p!=q || q!=2);

		 if(d_min<recRes[i].dist ){
			 /* Between oftions of bkp recSolutions[f] */
				fprintf(fp4,"%s|%d|%s;%lf\n",seqs[Frag_Seq[recSolutions[f][p][l].Idx]]->Id,BKP,
					seqs[Frag_Seq[recSolutions[f][q][r].Idx]]->Id,d_min);

			OptionalRecPrintout(fp4,recRes,seqs,Frag_Seq,recSolutions,i,l,r,p,q,f,index,last,BKP,win_Count,align,fr_size);
		 }
		 else{ /* take new bkp recRes[1] */
			 f=recRes[i].bkp;
			 d_min=recRes[i].dist;
			 l=p=q=0;
			 r=1;
			 if(align)
				 BKP=(win_Count-1-f)*fr_size;
			 else
				 BKP=(f+1)*fr_size;
			fprintf(fp4,"%s|%d|%s;%lf\n",seqs[Frag_Seq[recSolutions[f][p][l].Idx]]->Id,BKP,
				seqs[Frag_Seq[recSolutions[f][q][r].Idx]]->Id,d_min);

			OptionalRecPrintout(fp4,recRes,seqs,Frag_Seq,recSolutions,i+1,l,r,p,q,f,index,last,BKP,win_Count,align,fr_size);

		 }
	}

}
/**********************************************************************/
/* 							FillArrayInOrder					      */
/**********************************************************************/
double FillArrayInOrder(int *arrayInOrder, double ***dist_frags, double **bootVals, int  *Frag_Seq, int s, int k, int noSeqs, int bootOpt)
{
	int t,j,p,q,i;
	double largest,d1,d2;
			arrayInOrder[0]=Frag_Seq[0];/* The actual index in seqs[]*/
			if(bootOpt==0)
				largest=dist_frags[k][s][Frag_Seq[0]];
			else
				largest=bootVals[0][k];
			p=1;
			for(t=1;t<noSeqs;t++){ /* Loop through sequences to sort by distance*/
				if(bootOpt==0){
					if(largest<dist_frags[k][s][Frag_Seq[t]])
						largest=dist_frags[k][s][Frag_Seq[t]];
				}
				for(j=0;j<p;j++){/*loop through array */
					if(bootOpt==0){
						d1=dist_frags[k][s][Frag_Seq[t]];
						d2=dist_frags[k][s][arrayInOrder[j]];
					}
					else{
						i=0;
						while(Frag_Seq[i]!=arrayInOrder[j])
							i++;

						d1=bootVals[i][k];
						d2=bootVals[t][k];
					}
					if( d1<= d2){
						for(q=j;q<p;q++)
							arrayInOrder[q+1]=arrayInOrder[q];
						arrayInOrder[j]=Frag_Seq[t];
						p++;
						break;
					}

				}
				if(j==p){
					arrayInOrder[j]=Frag_Seq[t];
					p++;
				}

			}
			return(largest);

}
/**********************************************************************/
/* 																      */
/*  	Fill BKPNode with results stored in Array	            	  */
/* 																      */
/**********************************************************************/

BKPNode *ArrayToBKPNode( int  *F, int win_Count, int fr_size, 
						  int align, int bootOpt, int step)
{
	int i, k,j,t, bkp, add2BKP;
	BKPNode *RecStorage,*first, *linkBKP;
			j=F[0];//F[win_Count-1][0];

			MALLOC(RecStorage, sizeof(BKPNode));
			RecStorage->Child_Idx=j;
			RecStorage->Next=NULL;
			if(!align)
				first=RecStorage;

			k=1;
			i=0;
			add2BKP=(int)(fr_size/2)-(int)(step/2);
			while(k<win_Count){
				
				while(j==F[k] && k<win_Count)
					k++;

				if(align ||i>0)
					MALLOC(linkBKP, sizeof(BKPNode));
				if(align){
					//if(bootOpt)
						bkp=((win_Count-k)*step)+add2BKP;/*fr_size is window size*/
					//else
					//	bkp=(win_Count-k)*fr_size;
					linkBKP->BKP=bkp;
					linkBKP->Child_Idx=j;
					linkBKP->Next=RecStorage;
					if(i==0)/* In align it's the last BKPnode*/
						RecStorage->BKP=win_Count*fr_size;
					RecStorage=linkBKP;/* RecStorage becomes first now */
				}
				else{
					if(i>0){
						//if(bootOpt)
							bkp=t*step+add2BKP;/*fr_size is window size*/
						//else
						//	bkp=t*fr_size;
						RecStorage->BKP=bkp;
						RecStorage->Next=linkBKP;
						linkBKP->Child_Idx=j;
						RecStorage=linkBKP;/* RecStorage becomes first now */
					}
				}
				if(k<win_Count)
					j=F[k];
				i++;
				t=k;/* t is prev bkp*/
			}
			if(!align){
				RecStorage->BKP=win_Count*fr_size;
				RecStorage->Next=NULL;
			}
			else
				first=RecStorage;

			return(first);

}

/**********************************************************************/
/* 																      */
/*  	Get Recombination Results by browsing	            		  */
/*  	minFrag distances or Bootstrap values						  */
/*      More than one crossover possible	!!!			   			  */
/*  																  */
/**********************************************************************/
BKPNode *GetRecSolutionsII( double ***dist_frags, double **bootVals, Fasta **seqs, int  *Frag_Seq, 
						 int noSeqs, int s, int win_Count, int min_seq, int fr_size, 
						 double min_seq_dist, int align, int bootOpt, int step, int *avgBS)
{
 int i, j,t,k,  q=0,  lastBest, seqBestAfter,bkp=0;
 double *M,   d,d2,  recThresh,  largest=0,frPenal, frPow, temp, powBase;
 int  **prospects,  **F;
 BKPNode *result;

		if(bootOpt)
			recThresh=BootThreshold/100.0;//0.92;//
		else
			recThresh=3/(double)win_Count;//0.125;

		MALLOC(M, sizeof(double) * (win_Count+1));

		if((F = (int **)MatrixInit(sizeof(int), win_Count, win_Count))==NULL)/* dim1 is win_Count */
		  ErrorMessageExit("GetRecSolutionsII: Error with Memory Allocation 1\n",1);
		if((prospects = (int **)MatrixInit(sizeof(int), win_Count, noSeqs))==NULL)/* dim1 is win_Count */
		  ErrorMessageExit("GetRecSolutionsII: Error with Memory Allocation 2\n",1);

		for(k=0;k<win_Count;k++){/* Sort by distance & find largest distance*/
			d=FillArrayInOrder(prospects[k],dist_frags,bootVals,Frag_Seq,s,k,noSeqs,bootOpt);
			if(largest<d)
				largest=d;
		}
		M[0]=0;
		powBase=log(win_Count)/log(2);
		powBase=(powBase-1)/(double)powBase;
			frPenal=powBase;
		if(bootOpt){
			i=0;
			while(Frag_Seq[i]!=prospects[0][0])
				i++;
			temp=bootVals[i][0];/* window# is 2nd array dim */
		//	frPenal=powBase;
			
		}
		else{
			temp=largest-dist_frags[0][s][prospects[0][0]];
			//frPenal=0.5-(1/(double)win_Count);
		}
		M[1]=temp-temp*frPenal;
		


		F[0][0]=prospects[0][0];/* Contains ID of winner of win#0 or frag#0*/
		for(k=1;k<win_Count;k++){
			/* Store previous best solution plus 1 extra fragment */
			if(bootOpt){
				i=0;
				while(Frag_Seq[i]!=prospects[k][0])
					i++;
				temp=bootVals[i][k];
			}
			else
				temp=largest-dist_frags[k][s][prospects[k][0]];
			d=M[k]+(temp-temp*frPenal);

			lastBest=k-1;
			seqBestAfter=prospects[k][0];
			/* Go through all options up to frag# k and compare*/
			for(t=0;t<noSeqs;t++){
				for(j=k-1;j>=0;j--){
					d2=0;

					frPow=pow(powBase,(k-j));
					for(i=k;i>=j;i--){
						if(bootOpt){
							temp=bootVals[t][i];
						}
						else{
							temp=largest-dist_frags[i][s][Frag_Seq[t]];
							//frPow=frPenal*frPow;
						}
						d2=d2+(temp-temp*frPow);
					}
					d2=M[j] + d2;
					if(d2>d){
						lastBest=j-1;
						seqBestAfter=Frag_Seq[t];/* Store the real ID#*/
						d=d2;
					}

				}
			}
			M[k+1]=d;
			for(i=0;i<=lastBest;i++)
				F[k][i]=F[lastBest][i];/* F[] starts at k=0, M[] at k=1*/
			for(i=lastBest+1;i<=k;i++)
				F[k][i]=seqBestAfter;



		}
		d=0;
		for(k=0;k<win_Count;k++){
		//	printf("Frag #%d seq:%s\n",k, seqs[F[win_Count-1][k]]->Id);
			if(bootOpt){
				i=0;
				while(Frag_Seq[i]!=F[win_Count-1][k])
					i++;
				d+=bootVals[i][k];
			}
			else
				d+=dist_frags[k][s][F[win_Count-1][k]];
		}


		d=d/win_Count;
		(*avgBS)=(int)(d*100);
		
		//printf("d(=%lf) < min_dist (=%lf)?\n",d,(min_seq_dist*(1-recThresh)));
	
		/* Do we count it as a recombinant? */
		//return(NULL);
		for(i=0; i<win_Count; i++)   if(prospects[i]) free(prospects[i]);
		free(prospects);
		free(M);
		//printf("RefSeq: %s - recdis:%lf-min_dis:%lf\n",seqs[s]->Id,d,min_seq_dist);
		j=F[win_Count-1][0];
		k=1;
		while(j==F[win_Count-1][k] && k<win_Count)
							k++;
		if(((!bootOpt && d<(min_seq_dist*(1-recThresh))) ||(bootOpt && d>=recThresh))&& k<win_Count) /* Threshold 0.075/0.12 for 8/4 frags should be input by user*/
			result=ArrayToBKPNode(F[win_Count-1],win_Count,fr_size,align,bootOpt,step);
		else
			result=NULL;

		for(i=0; i<win_Count; i++)   if(F[i]) free(F[i]);
			free(F);
		return(result);



}
/**********************************************************************/
/* 			Helper function for GetRecSolutionsI				      */
/**********************************************************************/
double FindBestinStretch( double ***dist_frags, double **bootVals, int  *Frag_Seq,  
					  int bootOpt,int noSeqs, int s, int win_Count, int candidate,int lb,int ub,double largest)
{
	double d;
	int i;
			d=0;
			for(i=lb;i<ub;i++){
				if(bootOpt)
					d=d+bootVals[candidate][i];
				else
					d=d+largest-dist_frags[i][s][Frag_Seq[candidate]];
					//d=d+dist_frags[i][s][Frag_Seq[candidate]];
			}
			return(d);

}

/**********************************************************************/
/* 																      */
/*  	Get Recombination Results by calculating            		  */
/*  	left & right Average Distances (1 crossover only)			  */
/*  																  */
/**********************************************************************/
BKPNode *GetRecSolutionsI(double ***dist_frags, double **bootVals, Fasta **seqs, int  *Frag_Seq, 
						 int noSeqs, int s, int win_Count, int min_seq, int fr_size, 
						 double min_seq_dist, int align, int bootOpt, int step,int *avgBS)
{
 int  left, right,i,bestr=0,bestl=0,l,k,m,r;
 double *M,  largest, d, dl,dr,  recThresh;
 int  **prospects,  **F, *C, *list_bestl,*list_bestr;
 BKPNode *result;
 //FILE *fROC_test;
 //fROC_test=fopen("ROC.txt","a");

		if(bootOpt)
			recThresh=BootThreshold/100.0;//0.92 //(100-(5*noSeqs))/100.0;
		else
			recThresh=3/(double)win_Count;//0.125;


		if((prospects = (int **)MatrixInit(sizeof(int), win_Count, noSeqs))==NULL){/* dim1 is win_Count */
		  ErrorMessageExit("GetRecSolutionsI: Error with Memory Allocation 1!\n",1);
		}

		for(i=0;i<win_Count;i++){/* Sort by distance & find largest distance*/
			d=FillArrayInOrder(prospects[i],dist_frags,bootVals,Frag_Seq,s,i,noSeqs,bootOpt);
			if(largest<d)
				largest=d;
		}
		/*Find all breakpoints */
		MALLOC(list_bestr, sizeof(int) * noSeqs);
		MALLOC(list_bestl, sizeof(int) * noSeqs);
		MALLOC(C, sizeof(int) * win_Count);
		MALLOC(M, sizeof(double) * win_Count);
		if((F = (int **)MatrixInit(sizeof(int), win_Count, 2))==NULL)
		  ErrorMessageExit("GetRecSolutionsI: Error with Memory Allocation 2\n",1);
		for(i=1;i<win_Count;i++){
				dl=0;
			l=0;r=0;
			for(left=0;left<noSeqs;left++){
				d=FindBestinStretch(dist_frags, bootVals, Frag_Seq,  
					  bootOpt, noSeqs,  s,  win_Count,  left,0,i,largest);
				if( d>=dl)
					{dl=d;bestl=left;}
			}
			/* Check if ties to avoid FPs */
			for(left=0;left<noSeqs;left++){
				d=FindBestinStretch(dist_frags, bootVals, Frag_Seq,  
					  bootOpt, noSeqs,  s,  win_Count,  left,0,i,largest);
				if( d==dl)
					list_bestl[l++]=left;
			}
				dr=0;
			for(right=0;right<noSeqs;right++){
					d=FindBestinStretch(dist_frags, bootVals, Frag_Seq,  
						bootOpt, noSeqs,  s,  win_Count,  right,i,win_Count,largest);
				if( d>=dr)
					{dr=d;bestr=right;}
			}
			/* Check if ties to avoid FPs */
			for(right=0;right<noSeqs;right++){
					d=FindBestinStretch(dist_frags, bootVals, Frag_Seq,  
						bootOpt, noSeqs,  s,  win_Count,  right,i,win_Count,largest);
				if( d==dr)
					list_bestr[r++]=right;
			}

			if(bestl!=bestr){/* double check if they're really not the same */
				for(k=0;k<l;k++){
					for(m=0;m<r;m++){/* If ties then pick same left and right to avoid FPs */
						if(list_bestl[k]==list_bestr[m]){
							bestl=list_bestl[k];
							bestr=bestl;
							break;
						}

					}
					if(bestl==bestr) 
						break;
				}
			}
			
			M[i]=(dl+dr)/win_Count;
			F[i][0]=Frag_Seq[bestl];
			F[i][1]=Frag_Seq[bestr];
		}
		d=0;
		/* Find best solution*/
		for(i=1;i<win_Count;i++){
			if(M[i]>d ){
				d=M[i];
				bestl=i;/*here: bestl = best breakpoint*/
			}

		}
		(*avgBS)=(int)(M[bestl]*100);/*here: bestl = best breakpoint*/
		if(F[bestl][0]!=F[bestl][1]){
			//fprintf(fROC_test,"%s;%3.2f\n",seqs[s]->Id,M[bestl]);
			//printf("RefSeq: %s - recdis:%lf-min_dis:%lf\n",seqs[s]->Id,M[bestl],min_seq_dist);
			if((M[bestl]>=recThresh && bootOpt)||(M[bestl]>(largest-(min_seq_dist*(1-recThresh))) && !bootOpt)){
				for(i=0; i<win_Count; i++){
					if(i<bestl)
						C[i]=F[bestl][0];
					else
						C[i]=F[bestl][1];

				}
				result=ArrayToBKPNode(C,win_Count,fr_size,align,bootOpt,step);
			}
			else
				result=NULL;
		}
		else 
			result=NULL;

	

		/* Free memory */
		for(i=0; i<win_Count; i++)   if(prospects[i]) free(prospects[i]);
		free(prospects);
		for(i=0; i<win_Count; i++)   if(F[i]) free(F[i]);
		free(F);
		free(M);
		free(C);
		free(list_bestl);
		free(list_bestr);

//fclose(fROC_test);
		return(result);

}




/**********************************************************************/
/* 																      */
/*  	Get Sequence of Min Avg. Distance under MinSequence			  */
/*  																  */
/**********************************************************************/
int PickSeqofLargerAvgDis(double ***dist_frags, int k_idx, int f_idx, int min_idx,  int s, int win_Count, int k, int f,double min_d)
{
	double Avg_k=0,Avg_f=0;
	int i,j,a,b;
	j=0;
	for(i=0;i<win_Count;i++){
		a=0;b=0;
		if(k_idx!=min_idx && f_idx!=min_idx ){
			if(dist_frags[i][s][k_idx]<=dist_frags[i][s][min_idx])
				a=1;
			if(dist_frags[i][s][f_idx]<=dist_frags[i][s][min_idx])
				b=1;
		}
		else{
			if(dist_frags[i][s][k_idx]<=min_d)
				a=1;
			if(dist_frags[i][s][f_idx]<=min_d)
				b=1;

		}
		//if(a!=b)
			//break;

		if(a==1 && b==1){
			Avg_k=Avg_k + dist_frags[i][s][k_idx];
			Avg_f=Avg_f + dist_frags[i][s][f_idx];
			j++;
		}
		
	}
	if(Avg_k==Avg_f)//(a!=b)
		return(-1);
	else if(Avg_k>Avg_f){
		if(k==-1)
			return(k_idx);
		else
			return(k);
	}
	else{
		if(f==-1)
			return(f_idx);
		else
			return(f);
	}


}

int PickSeqMinDist(double ***dist_frags, int  *Frag_Seq, int min_idx,int noSeqs, int k_idx, int f_idx, int s, int win_Count, int k, int f,double min_d)
{
	 int  **prospects,i,a,b, f_count=0,k_count=0;
	 double Avg_k=0,Avg_f=0;

 	//	MALLOC(prospects, sizeof(int **) * win_Count );

	//	for(i=0;i<noSeqs ;i++){
	//		if ((prospects[i] = ( int *) malloc( sizeof( int)))==NULL) { 
	//					   ErrorMessageExit("PickSeqMinDist: Error with memory allocation\n",1);
	//				}
	//	}


	 if((prospects = (int **)MatrixInit(sizeof(int), win_Count, noSeqs))==NULL){/* dim1 is win_Count */
			printf("win_Count=%d, noSeqs=%d\n",win_Count,noSeqs);
			  		  ErrorMessageExit("PickSeqMinDist: Error with Memory Allocation\n",1);
	 }
		if(k==-1)
			a=Frag_Seq[k_idx];
		else a=k;
		if(f==-1)
			b=Frag_Seq[f_idx];
		else
			b=f;
			

		for(i=0;i<win_Count;i++)/* Sort by distance */
			FillArrayInOrder(prospects[i],dist_frags,NULL,Frag_Seq,s,i,noSeqs,0);
		for(i=0;i<win_Count;i++){
			/* Need to check distance not just frag winner in case others have same distance as winner*/
				if(dist_frags[i][s][a]==dist_frags[i][s][prospects[i][0]] || dist_frags[i][s][b]==dist_frags[i][s][prospects[i][0]] ){
					Avg_k=Avg_k + dist_frags[i][s][a];
					Avg_f=Avg_f + dist_frags[i][s][b];
				}
		}

		for(i=0; i<win_Count; i++)   
			if(prospects[i]) free(prospects[i]);
		free(prospects);

		if( Avg_k>Avg_f){//if(Avg_k>Avg_f){/* remove the one with smaller/larger avg dist.*/
			if(k==-1)
				return(k_idx);
			else
				return(k);
		}
		else{
			if(f==-1)
				return(f_idx);
			else
				return(f);
		}



}


/**********************************************************************/
/* 						Print Fragments							      */
/**********************************************************************/
void PrintFragments(FILE *fp3, int  *Frag_Seq, Fasta **seqs, double **dist, double ***dist_frags, int win_Count, int i, int s, int align )
{
	int t,k;
	fprintf(fp3,"Distance from %s\n",seqs[s]->Id);
	for(k=0;k<i;k++){
		fprintf(fp3,"%s;",seqs[Frag_Seq[k]]->Id);
		if(align){
			for(t=win_Count-1;t>=0;t--)
				fprintf(fp3,"%lf;",dist_frags[t][s][Frag_Seq[k]]);
		}
		else{
			for(t=0;t<win_Count;t++)
				fprintf(fp3,"%lf;",dist_frags[t][s][Frag_Seq[k]]);
		}
		fprintf(fp3,"%lf\n",dist[s][Frag_Seq[k]]);
	}
}
/**********************************************************************/
/* 				Pick Candidates	using PCC						      */
/**********************************************************************/
int PickCandidates( Fasta **seqs, int  *Frag_Seq, double ***dist_frags, int win_Count,
						 int min_idx, int s, int startAS, double min_d)
{
	int i,k,t,min_seq,remove;
	double d;
	double  Cxy,  m_x,m_y,Sx,Sy;
	int   f ;

	i=0;
	for(k=0;k<win_Count;k++){/* Loop through Fragment distances */
		d=INF9999;
		t=startAS;
		min_seq=-1;
		for(t;t>=0;t--){
		//	printf("In PickCandidates: RefSeq=%s: look at t=%s and min_seq=%s\n\n",seqs[s]->Id,seqs[Frag_Seq[t]]->Id,seqs[Frag_Seq[2]]->Id);
		//	fflush(stdout);
			if(d>=dist_frags[k][s][t]) {//&& (dist_frags[k][s][t]<= min_d || t==min_idx)
				if(d==dist_frags[k][s][t] && min_seq!=min_idx && t!=min_idx && min_seq!=-1)
					remove=PickSeqofLargerAvgDis(dist_frags, min_seq,t,min_idx,s,win_Count,-1,-1,min_d);
				else 
					remove=-2;
				if(remove!=t && remove!=-1){//&& remove!=-1
					d=dist_frags[k][s][t];
					min_seq=t;
				}
			}
		}
		if(min_seq>-1){
			for(t=0;t<i;t++){
				if (Frag_Seq[t]==min_seq) 
					break;
			}
			if(t==i) 
				Frag_Seq[i++]=min_seq;
		}
	}

	if(PCCThreshold>0){
	/* Discard sequences that pair up with one another with PCC>=Threshold */

		f=0;
		while(f<(i-1)){
			Sx=SumofSquaredValues(dist_frags,NULL,win_Count,Frag_Seq[f],s,&m_x);
			k=f+1;
			while(k<i){/* Cxy & Sy of the other Frags */
				Sy=SumofSquaredValues(dist_frags,NULL,win_Count,Frag_Seq[k],s,&m_y);
				Cxy=0;
				for(t=0;t<win_Count;t++)
					Cxy=Cxy+((dist_frags[t][s][Frag_Seq[k]]-m_y)*(dist_frags[t][s][Frag_Seq[f]]-m_x));
				Cxy=Cxy/(Sx*Sy);
				remove=-1;
				/* 0.675 for 8 frags, 0.75 for 4 frags */
			
				if(Cxy>=PCCThreshold ){//(Cxy>=PCCThreshold ) {/* Remove seq k or f from minfrag list  */
					//printf("In PCC: RefSeq=%s: remove one of %s and %s\n\n",seqs[s]->Id,seqs[Frag_Seq[k]]->Id,seqs[Frag_Seq[f]]->Id);

					//remove=PickSeqofLargerAvgDis(dist_frags, Frag_Seq[f],Frag_Seq[k],min_idx,s,win_Count,f,k,min_d);
					
					//if(remove==-1){//same distance
						remove=PickSeqMinDist(dist_frags, Frag_Seq,min_idx,i,k,f,s,win_Count,-1,-1,min_d);
					//}

					if(remove<(i-1)) {
						for(t=remove+1;t<i;t++)
							Frag_Seq[t-1]=Frag_Seq[t];
					}
					if(remove==f)
						k=i; /*end loop*/
					i--;
				}
				else /* if(Cxy<threshold)*/
					k++;
			}/* while(k<i) */
			if(remove!=f) f++;

		}/* while(f<i) */
	}







	return(i);

}

/**********************************************************************/
/* 																      */
/*  	Check Data for Recombination Signals						  */
/*  																  */
/**********************************************************************/

BKPNode *Check4Recombination(FILE *fp3, FILE *fp4, int  *Frag_Seq,  Fasta **seqs, double **dist, double ***dist_frags, int startAS, 
				int s, int win_Count,  int min_idx,  int align, int maxDim, int opt, 
				int step, int windowSize , short int bootReps, double min_d)
{
	int i, dummy;

	i=PickCandidates(seqs,Frag_Seq,dist_frags,win_Count,min_idx,s,startAS,min_d);
	

	PrintFragments(fp3,Frag_Seq,seqs, dist, dist_frags, win_Count,i, s , align);

	/* Discard sequences that cross minFragSeq twice */
	if (i>1){
		if(opt==0) 
			return(GetRecSolutionsII(dist_frags,NULL,seqs,Frag_Seq,i,s,win_Count,min_idx,windowSize,dist[s][min_idx],align,0,step,&dummy));
		else if(opt==1) 
			//return(GetRecSolutionsIOLD(fp4,dist_frags,seqs,Frag_Seq,i,s,win_Count,min_idx,fr_size,dist[s][min_idx],align));
			return(GetRecSolutionsI(dist_frags,NULL,seqs,Frag_Seq,i,s,win_Count,min_idx,windowSize,dist[s][min_idx],align,0,step,&dummy));
		else
			return(BootscanCandidates(fp4,seqs,Frag_Seq, step,  windowSize ,  bootReps,i,s,win_Count,min_idx,dist[s][min_idx],align,maxDim,0,&dummy));
	}
	else
		return(NULL);
}

/******************************************************************************************/
/*  	Calculate the bootstrap value for pairs of sequences s and t save in bootVals     */
/******************************************************************************************/

double GetBootstrapValues(Fasta **seqs, double ***bootVals,  int s, int a, int bootreps)
{
	int j,t,tStart,hiSeq;
	double dVal, lowDist, bVal=0;

	/* Fill bootVals with bootstrap value for pairs of sequences s and t */
	/* where t is from previous time period */
	
	tStart=s-1;
	while(seqs[tStart]->time == seqs[s]->time)
		tStart--;
	for(j=0;j<bootreps;j++){
		lowDist=100.0;
		for(t=tStart;t>=0;t--){
			dVal=bootVals[j][s][t];
			if(lowDist>=dVal){  
				if(lowDist!=dVal || hiSeq!=a){/* If duplicate minimum distance pick a as ancestor*/
					hiSeq=t;
				}
				lowDist=dVal;
			}
		}
		if(hiSeq==a)
			bVal++;

	}
	

	
	 return(bVal);
}


/******************************************************************************************/
/*  	Calculate the bootstrap value for clustered sequences						     */
/******************************************************************************************/

double GetClusteredBootstrapValues(Mintaxa *mintaxa, int max_times, Fasta **seqs, double ***bootVals,  int s, int min_idx, int bootreps)
{
	int i, j,t,tStart,hiSeq;
	double dVal, lowDist, bVal=0;

	/* Fill bootVals with bootstrap value for pairs of sequences s and t */
	/* where t is from previous time period */
	
	tStart=s-1;
	while(seqs[tStart]->time == seqs[s]->time)
		tStart--;
	for(j=0;j<bootreps;j++){
		lowDist=100.0;
		for(t=tStart;t>=0;t--){
			for (i=0;i<max_times;i++){
				if(mintaxa[i].Anc_Idx==t)
					break;
			}
			
			dVal=bootVals[j][s][t];
			if(lowDist>=dVal){  
				if(i<max_times){/*found it*/
					hiSeq=min_idx;
				}
				else{
					if (lowDist>dVal || hiSeq!=min_idx){/* If duplicate minimum distance pick a as ancestor*/
					hiSeq=t;
					}
				}
				lowDist=dVal;
			}
		}
		if(hiSeq==min_idx)
			bVal++;

	}
	

	
	 return(bVal);
}

/**********************************************************************/
/*  	CheckClustering:  removes sequences from mintaxa			  */
/*        that are almost identical to the min_idx  sequence		  */
/**********************************************************************/
int CheckClustering(Mintaxa *mintaxa,int max_times,Fasta **seqs,int s,int min_idx)
{

	int i,j,k,c,p;

	for (i=0;i<max_times;i++){
		if(mintaxa[i].Anc_Idx!=min_idx){
			/* check if identical at condon positions*/
			k=mintaxa[i].Anc_Idx;
			for(j=0;j<clustBoot;j++){
				c=codonList[j];
				for(p=c;p<c+3;p++){
					if(seqs[k]->Seq[p]!=seqs[min_idx]->Seq[p])
						break;/* not identical */

				}
				if(p<c+3)
					break;

			}
			if(j<clustBoot){
				/* Remove if not identical*/
				for(j=i;j<max_times-1;j++){
					mintaxa[j].Anc_Idx=mintaxa[j+1].Anc_Idx;
					mintaxa[j].dist=mintaxa[j+1].dist;
					mintaxa[j].div=mintaxa[j+1].div;
				}
				max_times--;
				i--;
				
			}


		}
	}
	return(max_times);

}

/**********************************************************************/
/*  	Store Boostrap Values										  */
/**********************************************************************/
void StoreBootstrapValues(int *boots, Mintaxa **minresults, int resno, int start)
{
	int i,j;
	for(i=0;i<resno;i++)//initialize
		boots[i]=0;
	for(i=start;i<resno+start;i++){
		for(j=0;j<resno;j++){
	//printf("minresults[j]->Child_Idx:%d\n",minresults[j]->Child_Idx);
			if(minresults[j]->Child_Idx==i) 
				break;
		}
		if(j<resno){
			boots[i]=(int)minresults[j]->div;
//	printf("div:%d\n",boots[i]);
		}
	}
}


/**********************************************************************/
/* 																      */
/*  	Output Results	to File										  */
/*  																  */
/**********************************************************************/
int OutputResults(int win_Count, int s, int startAS, double iBase, double min_d, int min_idx, int *times_max, double *div, BKPNode *bkpLL, double ***bootVals, Mintaxa *mintaxa,Mintaxa **minresults, Fasta **seqs, double **dist, int maxDim, int n, FILE *fp1, double mutrate, int fr_size, int *resno, int align, int avgBS,int bootreps)
{
	double	 d,  interval;
	int		i,  j, t,times; 
	static int		boots[NUMSEQS];


		times=0;
		if (bkpLL!=NULL){/*Is recombinant */
			boots[s]=avgBS;
			if(printBoot)
			fprintf(fp1,"Recombinant:%s[%d%%];",seqs[s]->Id,boots[s]);
			else
			fprintf(fp1,"Recombinant:%s;",seqs[s]->Id);

			while(bkpLL!=NULL){
				/* Print and free*/
				if(bkpLL->BKP<fr_size*win_Count)/* last donor segment*/{
					if(printBoot)
						fprintf(fp1,"%s[%d%%]|%d|",seqs[bkpLL->Child_Idx]->Id,boots[bkpLL->Child_Idx], bkpLL->BKP);
		
					else
						fprintf(fp1,"%s|%d|",seqs[bkpLL->Child_Idx]->Id, bkpLL->BKP);
				}
				else{
					if(printBoot)
						fprintf(fp1,"%s[%d%%]",seqs[bkpLL->Child_Idx]->Id,boots[bkpLL->Child_Idx]);
					else
						fprintf(fp1,"%s",seqs[bkpLL->Child_Idx]->Id);
				}

				if(SaveMinResults(minresults, resno, bkpLL->Child_Idx,s,dist[s][bkpLL->Child_Idx],avgBS)==false) 
					return(false);

				bkpLL=bkpLL->Next;
			}

			fprintf(fp1,";%d;\n",avgBS);
		}
		else {/* Not recombinant */
			t=startAS;
			if(clustBoot>0)
				interval=0.001*(maxDim/(double)1000);
//int *codonList;
			else
				interval=0;//iBase * seqs[s]->time;
			
			for(t;t>=0;t--){
				/* Minimum Distance Calculations: */
				if (min_d+interval>=dist[s][t]) {
					if(times>(*times_max)) {
						//if ((mintaxa[times] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL)
						//	ErrorMessageExit("OutputResults: Error with Memory Allocation\n",1);
						
						(*times_max)=times;
					}
					mintaxa[times].dist=dist[s][t];
					mintaxa[times].Anc_Idx=t;
					if(bootVals!=NULL){
						//if((GetBootstrapValues(seqs,dist,bootVals,bootreps))==false)
						//	return(false);
						
						mintaxa[times].div=GetBootstrapValues(seqs,bootVals,s,t,bootreps);
					}
					else{
						if(div!=NULL)
							mintaxa[times].div=div[t];
						else
							mintaxa[times].div=0;
					}

					if(times<19)times++;
				}

			}/*second t loop */

			if( clustBoot>0 && times>1 ){
				//int *codonList;
				/* Delete from mintaxa if not identical at selected codon position*/
				times=CheckClustering(mintaxa,times,seqs,s,min_idx);
				if(times>1){
					mintaxa[0].div=GetClusteredBootstrapValues(mintaxa,times,seqs,bootVals,s,min_idx,bootreps);
					mintaxa[0].dist=min_d;
					mintaxa[0].Anc_Idx=min_idx;
				}
				j=0;

			}
			else{
				d=mintaxa[0].div;j=0;
				/* All Ancestor Candidates With same distance to queried sequence */
				for(i=1;i<times;i++){
					//If(PrintSameDistCandidates)
					//	fprintf(fp1,"%s;%s;%lf;%lf\n",seqs[s]->Id,seqs[mintaxa[i].Anc_Idx]->Id,mintaxa[i].dist,mintaxa[i]->div);
					if (bootVals==NULL && fabs(min_d-mintaxa[i].dist)<= interval){
							if(d>mintaxa[i].div) {d=mintaxa[i].div;j=i;}
					}
					else	if(d<mintaxa[i].div) /*Boostrap values break a tie*/
						{d=mintaxa[i].div;j=i;}
				}
			}
			/* Print to File */
			//if(times>1 && PrintSameDistCandidates) fprintf(fp1,"The Chosen:%s;%s;%lf;%lf\n\n",seqs[s]->Id,seqs[mintaxa[j]->Anc_Idx]->Id,mintaxa[j]->dist,mintaxa[j]->div);
			//else
			boots[s]=(int)mintaxa[j].div;//avgBS;

			if(printBoot)
				fprintf(fp1,"%s[%d%%];%s[%d%%];%lf;%g\n",seqs[s]->Id,boots[s],seqs[mintaxa[j].Anc_Idx]->Id,boots[mintaxa[j].Anc_Idx],mintaxa[j].dist,mintaxa[j].div);
			else
				fprintf(fp1,"%s;%s;%lf;%g\n",seqs[s]->Id,seqs[mintaxa[j].Anc_Idx]->Id,mintaxa[j].dist,mintaxa[j].div);
			/* Save in struct */
			if(SaveMinResults(minresults, resno, mintaxa[j].Anc_Idx,
				s,mintaxa[j].dist, mintaxa[j].div)==false) 
				return (false);


		}
return(true);
}


/**********************************************************************/
/* 																      */
/*  	Get Minimum Distance = possible Ancestor					  */
/*  																  */
/**********************************************************************/
int GetMinDist(Mintaxa **minresults, Fasta **seqs, double **dist, double ***dist_frags, int maxDim, 
			   int n, char *File1, char *File2, double mutrate, int win_Count, int *resno, int align, 
			   int opt, int step, int winSize)
{
	FILE	*fp1,*fp2, *fp3,*fp4;
	int		i, k,  ftime,s,t,startAS,times_max, dummy=0; 
	Mintaxa *mintaxa;
	double	*div, d, d2, min_d, iBase;
	int  *Frag_Seq,    min_idx,fr_size;
	BKPNode *bkpLL;

	fr_size = maxDim/win_Count;

	 if((mintaxa = (Mintaxa *) malloc(sizeof(Mintaxa) * 20))==NULL)
		ErrorMessageExit("GetMinDist 1: Error with Mintaxa memory allocation\n",1);

	
	if((div= (double *) malloc((n+1)* sizeof (double)))==NULL)
		return(0);
	if( win_Count>1){
		if((Frag_Seq= (int *) malloc((win_Count)* sizeof (int)))==NULL)
			return(0);
		if ((fp3 = (FILE *) fopen("Frag_Dists.txt","w")) == NULL) {
			printf("io2: error when opening Frag_Dists.txt files\n");
				return(1);
		}
		if ((fp4 = (FILE *) fopen("Recs.txt","w")) == NULL) {
			printf("io2: error when opening Recs.txt files\n");
				return(1);
		}
	}
	
	times_max=-1;
	/* the longer the sequences the smaller the distances between them: */
	iBase=(mutrate/(double)maxDim);
	printf("maxDim=%d \n",maxDim);
	for(i=0;i<n+1;i++)
		dist[i][i]=0;
	/* printing results to file... */
	printf("printing results to file...\n");
	if ((fp2 = (FILE *) fopen(File2,"w")) == NULL) {
		printf("io2: error when opening SeqDistances.txt files\n");
			return(1);
	}
	if ((fp1 = (FILE *) fopen(File1,"w")) == NULL) 
		  ErrorMessageExit("io2: error when opening MinDists.txt file\n",1);
	

	  
	/* Move to second time period */
	s=1;
	ftime=seqs[0]->time;
	while(ftime==seqs[s]->time)
		s++;
	
	/* Then write distances to file */
	 for (s;s<=n;s++){
		t=s-1;
		while(seqs[t]->time == seqs[s]->time)
			t--;
		startAS=t;
		k=s;
		while(seqs[k]->time == seqs[s]->time && k<n)
			k++;
		if(k!=n)k--;
		min_d=INF9999;
		/* Now get this guy s minimum distance from all others before him starting at t */
		for(t;t>=0;t--){
			d=0;d2=0;
			for(i=0;i<=k;i++)  
				d +=dist[i][t];
			for(i=0;i<=k;i++) 
				d2 +=dist[i][s];
			div[t] =((n-2)*(dist[s][t])) - d - d2;
			if(reportDistances)
				fprintf(fp2,"%s %s %lf %lf \n",seqs[s]->Id, seqs[t]->Id, dist[s][t],div[t]);
			if(min_d>dist[s][t]) {
				min_d=dist[s][t];
				min_idx=t;
			}

		}/* t loop*/


		
		if (win_Count>1 && recOn) /* Recombination Detection */
			bkpLL = Check4Recombination(fp3,fp4, Frag_Seq,seqs,dist,
				dist_frags,startAS,s,win_Count, min_idx,align,maxDim,opt,step,winSize,0,min_d);
		else  /* Not (win_Count=1) */
			bkpLL=NULL;
			
		if(OutputResults(win_Count,s,startAS,iBase,min_d,min_idx,&times_max,div,bkpLL,NULL,&mintaxa[0], minresults,seqs,dist,maxDim,n,fp1,mutrate,fr_size,resno,align,dummy,0)==false)
			return(false);

	 }/*s loop */

/* ExitHere:*/
	if( win_Count>1){
		free(Frag_Seq);
		fclose(fp3);
		fclose(fp4);
	}
	fclose(fp2);
	fclose(fp1);
	free(div);
	free(mintaxa);
	return(1);
}


/**********************************************************************/
/* 																      */
/*  	Build Partial NJ Trees										  */
/*  																  */
/**********************************************************************/

void BuildPartialNJTrees(Fasta **seqs, Mintaxa **minresults, int resno, double **dist, char *File1, int n)
{
	FILE	*fp1;
	int		r, ch, i,j,a, w;
	int		max_w=0,ftime;
	int		children[NUMSEQS],boots[NUMSEQS];
	TNode	**NodeStorage;
	double	**DistMatrix;



	if((DistMatrix = (double **)MatrixInit(sizeof(double), n+1, n+1))==NULL)
		ErrorMessageExit("BuildPartialNJTrees: ERROR with memory allocation!\n",1);
	MALLOC(NodeStorage, sizeof(TNode *) * (2*n));

//for(i=0;i<48;i++)//TEST
//	children[i]=i;
//NJTree(NodeStorage, dist, 48, false, children, &max_w, &w);
//PrintTree(stderr,NodeStorage[w-1],&seqs[0]);
//fprintf(stderr,";\n\n");

	/* Print Newick Trees to File */
	if ((fp1 = (FILE *) fopen(File1,"a")) == NULL) 
		  ErrorMessageExit("io2: error when opening txt file\n",1);
	
	/* Sort by ancestor*/
	fprintf(fp1,"\n");

	for(i=0;i<resno;i++)
		heapify_mintaxa(minresults,i);
	heapsort_mintaxa(minresults,resno-1);
	r=0;
	
    /*** To print bootstrap support ***/
	ftime=seqs[0]->time;
		while(ftime==seqs[r]->time)
			r++;	
	if(printBoot)
		StoreBootstrapValues(boots,minresults,resno,r);

	r=0;
	while(r<resno){
		a=minresults[r]->Anc_Idx;
		i=r;
		ch=0;
		children[ch++]=a;
		children[ch++]=minresults[i++]->Child_Idx;
		/* Store all children of same ancestor */
		while(i<resno){
			if(minresults[i]->Anc_Idx==a)
				children[ch++]=minresults[i++]->Child_Idx;
			else
				break;
		}
		r=i;/* Point to next ancestor */
		if(ch==2){
			if(printBoot)
				fprintf(fp1,"(%s[%d%%]:0.0,%s[%d%%]:%lf);\n\n",seqs[a]->Id,boots[a],seqs[children[ch-1]]->Id,boots[children[ch-1]],dist[a][children[ch-1]]);
			else
				fprintf(fp1,"(%s:0.0,%s:%lf);\n\n",seqs[a]->Id,seqs[children[ch-1]]->Id,dist[a][children[ch-1]]);
		}
		else{
			/* Build NJ tree */
			/* First store distances in smaller NJ matrix*/
			for(i=0; i<ch;i++) {
				for(j=0;j<ch;j++){
					DistMatrix[i][j]=DistMatrix[j][i]=fabs(dist[children[i]][children[j]]);
				}
			}
			NJTree(NodeStorage, DistMatrix, ch, true, children, &max_w, &w);
			/* Now print it */
		//seqs[0]->Id=p;

			PrintTree(fp1,NodeStorage[w-1],&seqs[0],&boots[0],printBoot);
			fprintf(fp1,";\n\n");
			

		}//else build NJ tree


	}//while(r<resno)

fclose(fp1);
for(i=0; i<(n+1); i++)   if(DistMatrix[i]) free(DistMatrix[i]);
free(DistMatrix);

for(i=0;i<max_w;i++)
	free(NodeStorage[i]);
free(NodeStorage);

}


/******************************************************************************************/
/*  					Prepare Bootstrap weights   							          */
/******************************************************************************************/
int PrepareWeights(int *wmod,int bootreps, int windowSize)
{
	double rnd;
	int i,j,rndNum2,rndNum;
	int *Scratch;
	
	if(Bootstrap){

		for(i=0;i<bootreps;i++){
			for(j=0;j<windowSize;j++)
				wmod[j*bootreps + i]=0;
		}

		for(i=1;i<bootreps;i++){/* start with i=1 as in RDP2 VB and bootreps=bootreps+1*/
			for (j = 0; j < windowSize; j++) {
			  rndNum = (int)(windowSize * rndu());
				wmod[rndNum*bootreps +i]++;
			}
		}
	}
	else{/* Jackknife*/
		for(i=0;i<bootreps;i++){
			for(j=0;j<windowSize;j++)
				wmod[j*bootreps + i]=1;
		}


		if((Scratch = ( int *) malloc( sizeof( int) * (windowSize+1)))==NULL){
				   printf("PrepareWeights: Error with memory allocation\n");
				   return(false);
		}

		rnd=rndu();

		for(i=1;i<bootreps;i++){/* start with i=1 as in RDP2 VB and bootreps=bootreps+1*/
			rndNum2 = (int)(windowSize / 4) + (int)(((windowSize / 4) * rnd));
			for(j=0;j<(windowSize+1);j++)
				Scratch[j]=0;

			for(j=0;j<rndNum2;j++){/* Only about half the cols are bootstrapped */
				do{
					rndNum = (int)((windowSize) * rndu());
				} while (Scratch[rndNum ] == 1);

				Scratch[rndNum ]= 1;
				wmod[rndNum*bootreps +i] = 0; /*Col not part of bootstrapped cols*/
				do{
					rndNum = (int)((windowSize) * rndu() );
				} while (wmod[rndNum*bootreps +i ] == 0);

				Scratch[rndNum ]= 1;
				wmod[rndNum*bootreps +i] = wmod[rndNum*bootreps +i]+1; /*Col appears more than once*/

			}

		}
	
		free(Scratch);
	}

		return(true);

}



/******************************************************************************************/
/*  																                      */
/*  	Bootscan RIP with 3 distance models												  */
/*  																					  */
/******************************************************************************************/

int BootRip(short int bootreps,  int model, double alpha, int noSeqs, int windowSize, char *seqs,  double *dist,  int *wmod)
{
	int i,j,b,nos, weight,lastCol,s,t;
	char **seqsBootstrap;
	int bootrepsPlusOne;
	double distVal;

	if((seqsBootstrap = (char **)MatrixInit(sizeof(char), noSeqs, windowSize))==NULL)
      return(0);
	bootrepsPlusOne=bootreps+1;

	for(b=0;b<bootrepsPlusOne;b++){
		lastCol=0;
//for(j=0;j<bootrepsPlusOne;j++){
//		test=0;
//		for(i=0;i<windowSize;i++)
//			test+=wmod[i*bootrepsPlusOne +j];
//		if(test!=200)
//			printf("\n  wmod total ---> %d\n",test);
//}
		i=0;
		while(i<windowSize){/* Fill bootstrapped sequences */
			if(wmod[i*bootrepsPlusOne +b]==0){
				i++;
				if(i>=windowSize) 
					break;
			}
			weight=wmod[i*bootrepsPlusOne +b];
			for(nos=0;nos<noSeqs;nos++){
				for(j=lastCol;j<lastCol+weight;j++)
					seqsBootstrap[nos][j]=seqs[nos*(windowSize) + i];
			}
			lastCol+=weight;
			i++;
		}
				//	printf("Check alignment \n");
				//	for(nos=0;nos<noSeqs;nos++){
				//		for(i=0;i<windowSize;i++){
				//			printf("%c",seqsBootstrap[nos*(windowSize) + i]);
				//		}
				//		printf("\n");
				//	}

		/* get all pairs distance */
		for (s=1;s<noSeqs;s++){
			t=s-1;
			for(t;t>=0;t--)
			{
			//if(s==14 && t==1)
			//	printf("stop\n");
				distVal=PairwiseDistance(seqsBootstrap,t,s,windowSize,model);


				//if(distVal<0 ||distVal>1000){
				//	printf("Problem distVal=%lf, s=%d, t=%d, b=%d \n",distVal,s,t,b);
					//distVal=TN93distance( pT,  pC,  pG,  pA,  P1,  P2,  gaps,  matches,windowSize, alpha,s,t);
					//for(nos=0;nos<noSeqs;nos++){
					//	for(i=0;i<windowSize;i++){
					//		printf("%c",seqsBootstrap[nos*(windowSize) + i]);
					//	}
					//	printf("\n");
					//}

				//}
				*(dist + bootrepsPlusOne*noSeqs*t + bootrepsPlusOne*s + b) = distVal;
				*(dist + bootrepsPlusOne*noSeqs*s + bootrepsPlusOne*t + b) = distVal;
			}/* t loop*/


			*(dist + bootrepsPlusOne*noSeqs*s + bootrepsPlusOne*s + b)=99999.0;
		}/*s loop*/
		*(dist + b)=99999.0;


	}
	for(i=0; i<noSeqs; i++)   if(seqsBootstrap[i]) free(seqsBootstrap[i]);
	free(seqsBootstrap);
	return(1);

}

/******************************************************************************************/
/*  																                      */
/*  	Get NJ topology distances														  */
/*  																					  */
/******************************************************************************************/

int GetBootNJdistances(int bootreps,int noSeqs, double *dstMat,Fasta **seqs, int distPenalty)
{
	double **dist,  distVal=0.0,distVal1=0.0;
	int **intDist,max_w,w,i,k,b,max;
	TNode	**NodeStorage;
	int		children[NUMSEQS];
	int *topoMatrix;
	int bootrepsPlusOne;

	bootrepsPlusOne=bootreps+1;

	if((topoMatrix= (int *) malloc(noSeqs* sizeof (int)))==NULL)
		return(0);
	if((intDist = (int **)MatrixInit(sizeof(int), noSeqs, noSeqs))==NULL)
      return(0);
	if((dist = (double **)MatrixInit(sizeof(double), noSeqs, noSeqs))==NULL)
      return(0);
	MALLOC(NodeStorage, sizeof(TNode *) * (2*noSeqs));


	for(b=0;b<bootrepsPlusOne;b++){
		for(i=0;i<noSeqs;i++){
			for(k=i+1;k<noSeqs;k++){
				dist[i][k]=dist[k][i]=	*(dstMat + k* bootrepsPlusOne * noSeqs + i*bootrepsPlusOne + b);
				//printf("dist[%d][%d]=%lf *** ",i,k,dist[i][k]);
				intDist[i][k]=intDist[k][i]=-1;
			}
			dist[i][i]=0.0;
		}
		for(i=0;i<noSeqs;i++)
			children[i]=i;
		NJTree(NodeStorage, dist, noSeqs, false, children, &max_w, &w);

		for(i=0;i<noSeqs;i++)
			topoMatrix[i]=0;
		if(distPenalty){
			i=0;
			distVal1=GetAvgBrLen(NodeStorage[w-1],&i);
			distVal1=distVal1/(double)i;
		}
		//PrintTree(stderr,NodeStorage[w-1],seqs);
		//fprintf(stderr,";\n\n");
		max=0;
		MatricizeTopology(NodeStorage[w-1], topoMatrix, noSeqs, intDist,distVal1,&max);
		for(i=0;i<noSeqs;i++){
			for(k=i+1;k<noSeqs;k++){
				distVal=(double)intDist[i][k]/max;
				if(distVal<0)
					ErrorMessageExit("problem in GetBootNJdistances:distVal<0!\n",1);
				*(dstMat + bootrepsPlusOne*noSeqs*k + bootrepsPlusOne*i + b) = distVal;		
				*(dstMat + bootrepsPlusOne*noSeqs*i + bootrepsPlusOne*k + b) = distVal;
				
			}
			*(dstMat + b)=99999.0;
		}

		
	}

	for(i=0; i<noSeqs; i++)   if(dist[i]) free(dist[i]);
	free(dist);
	for(i=0; i<noSeqs; i++)   if(dist[i]) free(intDist[i]);
	free(intDist);

	for(i=0;i<(2*noSeqs-2);i++)
		free(NodeStorage[i]);
	//NodeStorage[102]->nodeNo=0;
	free(NodeStorage);
	free(topoMatrix);
	return(1);

}

/******************************************************************************************/
/*  																                      */
/*  	Bootscan when Sequences already aligned	& when using sliding-windows			  */
/*  																					  */
/******************************************************************************************/



int DoBootscan(double ***dist_frags,int **plotVal,int numWins, Fasta **seqsBS,  int maxDim, int noSeqs,
		   int stepSize, int windowSize,int closestRel, int opt, short int bootreps, int fullBootscan)
{
	//short int *seqsInt;
	short int full;
	char *seqChars;
	int start,i,j,k,nos,w;
	double *DstMat,hiVal, dVal;
	int hiSeq, A,noScoreFlag=0;
	int *wmod;
	double minDist;

	short int bootrepsPlusOne;
	double cvi=1.0; /* Coefficient of Variation - only for Jin Nei 90*/
	double tstv=2.0;/* TSTV ratio*/

	bootrepsPlusOne=bootreps+1; /* bootreps+1 because of the VB array dimension */


	if((wmod = ( int *)malloc(bootrepsPlusOne * windowSize * sizeof( int)))==NULL)
		ErrorMessageExit("DoBootscan: Error with memory allocation 1\n",1);

	PrepareWeights(wmod,bootrepsPlusOne, windowSize);


	if((DstMat = (double *)malloc(bootrepsPlusOne * noSeqs * noSeqs * sizeof(double)))==NULL)
		ErrorMessageExit("DoBootscan: Error with memory allocation 2\n",1);
//	if((seqsInt = (short int *)malloc(noSeqs * (windowSize+1) * sizeof(short int)))==NULL)
//		return(0);
	if((seqChars = (char *)malloc(noSeqs * (windowSize) * sizeof(char)))==NULL)
		ErrorMessageExit("DoBootscan: Error with memory allocation 3\n",1);



    // numWins = Int(maxDim / stepSize) + 2;/* If circularity assumed */
	start=0;
	for(w=0;w<numWins;w++){
		/* Fill seqsint */
		for(nos=0;nos<noSeqs;nos++){
			i=0;//i=1;
			for (j=start;j<(start+windowSize);j++){
				//seqsInt[nos*(windowSize+1) + i]=(short int)(seqsBS[nos]->Seq[j])+1;/* Don't know why +1, but that's how's done in RDP2*/
				seqChars[nos*(windowSize) + i]=seqsBS[nos]->Seq[j];
				i++;
			}
		}


		//if(BootDist(bootreps,cvi,tstv,tmodel,noSeqs,windowSize,seqsInt,  DstMat, wmod)!=1)
		//	return(false);
		if(fullBootscan && noSeqs>3)
			full=true;
		else
			full=false;
		if(BootRip(bootreps,model,alpha, noSeqs,windowSize,seqChars,  DstMat, wmod)!=1){
			ErrorMessageExit("BootRip returned false\n",0);
			return(false);
		}
		if(full)
			GetBootNJdistances(bootreps,noSeqs, DstMat,seqs,distPenalty);
		printf(".");
		fflush(stdout);

		if(opt) {/* Fill dist_Frags*/
			for(i=0;i<noSeqs;i++){
				for(k=i+1;k<noSeqs;k++){
					for(j=0;j<bootreps;j++){
					// *(dmat + (bootrepsPlusOne + 1)*(numsp)*(m - 1) + (bootrepsPlusOne +1)*(n - 1) + l) =
						//if(k!=i)
						if(opt==2)/* 1 window */
							dist_frags[j][i][k]=dist_frags[j][k][i]=DstMat[(k* bootrepsPlusOne * noSeqs) + i*bootrepsPlusOne + j];
						else
							dist_frags[w][i][k]+=DstMat[(k* bootrepsPlusOne * noSeqs) + i*bootrepsPlusOne + j];

					}
					if(opt==1)
						dist_frags[w][k][i]=dist_frags[w][i][k]=dist_frags[w][i][k]/(double)bootreps;
				}/* for j<bootrepsPlusOne */
				if(opt==1)
					dist_frags[w][i][i]=99.9;
				else{
					for(j=0;j<bootreps;j++)
						for(k=0;k<noSeqs;k++)
							dist_frags[j][k][k]=99.9;

				}

			}

		}
		else{/* Fill Plot*/
			for(j=0;j<bootreps;j++){
				hiVal=100.0;
				minDist=100.0;//test bootstrap >100

				for(k=1;k<noSeqs;k++){
				// *(dmat + (bootrepsPlusOne + 1)*(numsp)*(m - 1) + (bootrepsPlusOne +1)*(n - 1) + l) =
					dVal=DstMat[(k* bootrepsPlusOne * noSeqs) + 0 + j];/* [j][0][k]*/
					if(BootTieBreaker){
						if(minDist>dVal)
							minDist=dVal;
					}
					
					if(dVal<0)
						ErrorMessageExit("problem:dVal<0!\n",1);
					if(hiVal>=dVal ){//RDP2: hiVal>=dVal
						hiVal=dVal;
						hiSeq=k;
					}
				}
				if(BootTieBreaker){
					for(k=1;k<noSeqs;k++){
						dVal=DstMat[(k* bootrepsPlusOne * noSeqs) + 0 + j];/* [j][0][k]*/
						if(dVal==minDist)
							plotVal[k-1][w]++;
					}
				}
				if(closestRel){
					A=hiSeq;
					for(k=1;k<noSeqs;k++){
						if(k!=A){
							dVal=DstMat[(k* bootrepsPlusOne * noSeqs) + A* bootrepsPlusOne +j];//DstMat[j][A][k]
							if(dVal <hiVal){
								noScoreFlag = 1;
								break;
							}
						}
					}
					if(noScoreFlag==0)
						plotVal[hiSeq-1][w]++;
					else
						noScoreFlag=0;
				}
				else if(!BootTieBreaker)
					plotVal[hiSeq-1][w]++;
				//test bootstrap >100
			}

		}



		start=start+stepSize;



	}



//	free(seqsInt);
	free(seqChars);

	free(wmod);

	free(DstMat);

	return(true);

}

/******************************************************************************************/
/*  																                      */
/*  	Shrink Pool of Parental Candidates of sequence s							      */
/*  																					  */
/******************************************************************************************/

 int BootscanShrinkPool( FILE *fp4, Fasta **seqs, int  *Frag_Seq, int step, int windowSize , short int bootReps,
						 int numSeqs, int s, int numWins, int maxDim, int bootThreshold, int spikeLen)
{
	int **plotVal,i,j,closestRel=0,highB,min_seq,n, spikeLenSoFar, prev_min_seq;
	Fasta **seqsBS;//[NUMSEQS];

		SetSeed(seed);//Seed provided by user....
		MALLOC(seqsBS, sizeof(Fasta **) * (numSeqs+1));

		for(i=0;i<numSeqs+1;i++){
			if ((seqsBS[i] = ( Fasta *) malloc( sizeof( Fasta)))==NULL) { 
						   ErrorMessageExit("BootscanShrinkPool: Error with memory allocation\n",1);
					}
		}
		strcpy(seqsBS[0]->Id,seqs[s]->Id);
		strcpy(seqsBS[0]->Seq,seqs[s]->Seq);
		seqsBS[0]->time=seqs[s]->time;
		for (i=0;i<numSeqs;i++){
			strcpy(seqsBS[i+1]->Id,seqs[Frag_Seq[i]]->Id);
			strcpy(seqsBS[i+1]->Seq,seqs[Frag_Seq[i]]->Seq);
			seqsBS[i+1]->time=seqs[Frag_Seq[i]]->time;
		}
		numSeqs=numSeqs+1;
		if((plotVal = (int **)MatrixInit(sizeof( int), numSeqs, numWins))==NULL)
			return(0);
		/* Add s to seqsInt at 0*/
		for(i=0;i<numSeqs;i++){
			for(j=0;j<numWins;j++)
				plotVal[i][j]=0;
		}
		if((DoBootscan(NULL,plotVal,numWins,seqsBS,  maxDim,  numSeqs, step, windowSize,closestRel,0,bootReps,fullBootscan))==false)
				  return(0);

		fprintf(fp4,"Point;");
		for(j=0;j<numWins;j++)
			fprintf(fp4,"%d;",j*step+(int)(windowSize/2));

		fprintf(fp4,"\n");
		for(i=1;i<numSeqs;i++){/* numSeqs is 1 too many because of RDP VB*/
			fprintf(fp4,"%s;",seqsBS[i]->Id);
			for(j=0;j<numWins;j++)
				fprintf(fp4,"%d;",plotVal[i-1][j]);
			fprintf(fp4,"\n");

		}
		fprintf(fp4,"\n");
		
		
		
		
		n=0;
		prev_min_seq=-1;
		for(j=0;j<numWins;j++){
			min_seq=-1;
			highB=0;
			for(i=0;i<numSeqs-1;i++){
				if(highB<plotVal[i][j] && plotVal[i][j]>=bootThreshold){
					min_seq=Frag_Seq[i];
					highB=plotVal[i][j];
				}
			}
			if(min_seq>-1){
				if(prev_min_seq==min_seq){
					spikeLenSoFar++;
				}
				else
					spikeLenSoFar=1;
				/* Spike should last approx. 100 nucleotides */
				if(spikeLenSoFar>=spikeLen){
						for(i=0;i<n;i++){
							if (Frag_Seq[i]==min_seq) 
								break;
						}
						if(i==n) 
							Frag_Seq[n++]=min_seq;
				}
				prev_min_seq=min_seq;
			}

		}


		for(i=0;i<numSeqs;i++)
			if(seqsBS[i]) free(seqsBS[i]); 
		free(seqsBS);
		for(i=0; i<numSeqs; i++)  
					if(plotVal[i]) free(plotVal[i]);
		free(plotVal);
		return(n);

}
/******************************************************************************************/
/*  																                      */
/*  	Bootscan only Parental Candidates of sequence s								      */
/*  																					  */
/******************************************************************************************/

BKPNode *BootscanCandidates(FILE *fp4,  Fasta **seqs, int  *Frag_Seq, int step, int windowSize , short int bootReps,
						 int numSeqs, int s, int numWins, int min_seq, double min_seq_dist, int align, int maxDim, int opt, int *avgBS)
{
	int **plotVal,*F, BKPs[100], parents[100];
	int i,j,a,prevFj,prevFjswing,bkp,isRec,sw,swingWin,swingLen,swingLenSoFar;
	Fasta **seqsBS;//[NUMSEQS];
	BKPNode *BKPResult;
	double **bootVal,swingTotal[10],bTotal,sumBKPSwing;

		if((F= (int *) malloc((numWins)* sizeof (int)))==NULL)
			ErrorMessageExit("BootscanCandidates: Error with memory allocation 1\n",1);

		SetSeed(seed);//Seed provided by user....
		MALLOC(seqsBS, sizeof(Fasta **) * (numSeqs+1));
		for(i=0;i<numSeqs+1;i++){
			if ((seqsBS[i] = ( Fasta *) malloc( sizeof( Fasta)))==NULL) 
				ErrorMessageExit("BootscanCandidates: Error with memory allocation 1\n",1);
						  
			
		}
		strcpy(seqsBS[0]->Id,seqs[s]->Id);
		strcpy(seqsBS[0]->Seq,seqs[s]->Seq);
		seqsBS[0]->time=seqs[s]->time;
		
		for (i=0;i<numSeqs;i++){
			strcpy(seqsBS[i+1]->Id,seqs[Frag_Seq[i]]->Id);
			strcpy(seqsBS[i+1]->Seq,seqs[Frag_Seq[i]]->Seq);
			seqsBS[i+1]->time=seqs[Frag_Seq[i]]->time;
		}
		numSeqs=numSeqs+1;
		if((plotVal = (int **)MatrixInit(sizeof( int), numSeqs, numWins))==NULL)
			ErrorMessageExit("BootscanCandidates: Error with memory allocation 2\n",1);
		if((bootVal = (double **)MatrixInit(sizeof( double), numSeqs, numWins))==NULL)
			ErrorMessageExit("BootscanCandidates: Error with memory allocation 3\n",1);

		/* Add s to seqsInt at 0*/
		for(i=0;i<numSeqs;i++){
			for(j=0;j<numWins;j++)
				plotVal[i][j]=0;
		}
		if((DoBootscan(NULL,plotVal,numWins,seqsBS,  maxDim,  numSeqs, step, windowSize,closestRel,0,bootReps,fullBootscan))==false)
				  return(NULL);

		fprintf(fp4,"Point;");
		for(j=0;j<numWins;j++)
			fprintf(fp4,"%d;",j*step+(int)(windowSize/2));

		fprintf(fp4,"\n");
		for(i=1;i<numSeqs;i++){/* numSeqs is 1 too many because of RDP VB*/
			fprintf(fp4,"%s;",seqsBS[i]->Id);
			for(j=0;j<numWins;j++)
				fprintf(fp4,"%d;",plotVal[i-1][j]);
			fprintf(fp4,"\n");

		}
		fprintf(fp4,"\n");
		/* Add s to seqsInt at 0*/

		if(opt==1){/* This option measures the breakpoint magnitude within a segment of windows left/right of BKP */
			for(i=0;i<100;i++)
				parents[i]=-1;
			bkp=0;
			swingWin=160/step; //8 for step 20
			swingLen=80/step; //3,4 for step 20
			sumBKPSwing=0;
			swingLenSoFar=1;
			sw=0;
			isRec=false;
			for(j=0;j<numWins;j++){/* Fill F with top parents for BKP */
				a=-1;
				for(i=0;i<numSeqs-1;i++){
					if(a<plotVal[i][j]){
						a=plotVal[i][j];
						F[j]=i;
					}

				}
			}
			for(j=0;j<numWins-swingWin;j++){/* BKP-Window size swingWin=10*/
				
				if(F[j]!=F[j+swingWin]){
					bTotal=((plotVal[F[j]][j]-plotVal[F[j+swingWin]][j])+(plotVal[F[j+swingWin]][j+swingWin]-plotVal[F[j]][j+swingWin]))/2;
					swingTotal[sw++]=bTotal;
					sumBKPSwing=0;
					if(sw==swingLen)
						sw=0;
					if(j>=swingLen){
						if(prevFj==F[j] && prevFjswing==F[j+swingWin]){
							swingLenSoFar++;
							for(i=0;i<swingLen;i++)
								sumBKPSwing+=swingTotal[i];
						}
						else
							swingLenSoFar=1;
					}
					if(sumBKPSwing/(double)swingLen>=BootThreshold && swingLenSoFar>=swingLen){
						if(prevFjswing!=parents[bkp]){/*Not the same parents */
							for(i=j;i<j+swingWin;i++){
								if(F[i]!=prevFj)/* Find breakpoint*/
									break;
							}
							parents[bkp]=prevFj;
							BKPs[bkp++]=i;
							parents[bkp]=prevFjswing;
							isRec=true;
						}
					}
					prevFj=F[j];
					prevFjswing=F[j+swingWin];
					
				}

			}

			if(isRec){
				j=0;
				i=0;
				while(i<bkp){
					for(j;j<BKPs[i];j++)
						F[j]=Frag_Seq[parents[i]];
					i++;
				}
				for(j;j<numWins;j++)
					F[j]=Frag_Seq[parents[bkp]];


				BKPResult=ArrayToBKPNode(F,numWins,windowSize,align,1,step);
			}
			else
				BKPResult=NULL;
		}
		else{
			for(i=0;i<numSeqs-1;i++){
				for(j=0;j<numWins;j++)
					bootVal[i][j]=plotVal[i][j]/(double)bootReps;
			}
			if(crossOpt==1)
				BKPResult=GetRecSolutionsI(NULL,bootVal,seqs,Frag_Seq,numSeqs-1,s,numWins,min_seq,windowSize,min_seq_dist,align,1,step,avgBS);
			else
				BKPResult=GetRecSolutionsII(NULL,bootVal,seqs,Frag_Seq,numSeqs-1,s,numWins,min_seq,windowSize,min_seq_dist,align,1,step,avgBS);
		}
	
		for(i=0; i<numSeqs; i++)  
					if(plotVal[i]) free(plotVal[i]);
		free(plotVal);
		for(i=0; i<numSeqs; i++)  
					if(bootVal[i]) free(bootVal[i]);
		free(bootVal);
		for (i=0;i<numSeqs;i++){	
			if(seqsBS[i]) free(seqsBS[i]);
		}
		free(seqsBS);
		free(F);

		return(BKPResult);
}

/******************************************************************************************/
/*  					copy from seqs to seqsBS								          */
/******************************************************************************************/
int CopyToSeqsBootscan(Fasta **seqsBS, Fasta **seqs,int numSeqs, int seqNo, int *maxBSDim)
{
	//int i=0;
	//	while(strcmp(name,seqs[i]->Id)!=0)
	//		i++;
		//if(i<=n){
			if(*maxBSDim<=numSeqs){
				if ((seqsBS[numSeqs] = ( Fasta *) malloc( sizeof( Fasta)))==NULL) { 
					   printf("CopyToSeqsBootscan: Error with memory allocation\n");
					   return(false);
				}
				(*maxBSDim)++;
			}
			strcpy(seqsBS[numSeqs]->Id,seqs[seqNo]->Id);
			strcpy(seqsBS[numSeqs]->Seq,seqs[seqNo]->Seq);
			seqsBS[numSeqs]->time=seqs[seqNo]->time;
		//}
		return(1);
}

int GetParentsFromAnswer(Fasta **seqs,char *temp,int *parents, int n)
{
	int k, i,p;
	char name[20];

		p=0;
		k=0;i=0;
		while(temp[k]==' ')
			k++;
		while(temp[k]!='\0' && temp[k]!='|' && temp[k]!=';')
			name[i++]=temp[k++];
		name[i]='\0';
			
		i=0;
		while(i<n && strcmp(name,seqs[i]->Id)!=0)
			i++;
		if(i==n)
			return(-1);
		parents[p++]=i;
		parents[p]=-1;
		while(temp[k]!=';'){
			if(temp[k]=='|'){
				k++;
				while(temp[k]!='|')
					k++;
				k++;

				i=0;
				while(temp[k]!='\0' && temp[k]!='|' && temp[k]!=';')
					name[i++]=temp[k++];
				name[i]='\0';
				i=0;
				while(strcmp(name,seqs[i]->Id)!=0)
					i++;
				parents[p++]=i;
			}
		}
		return(p);
}

int Parents_Missed(int *Frag_Seq,int numSeqs,int *parents,int p)
{
	int i,j,pa[10],k;
	for(i=0;i<p;i++)
		pa[i]=false;
	k=0;
	for(i=0;i<numSeqs;i++){
		for(j=0;j<p;j++){
			if(Frag_Seq[i]==parents[j]){
				pa[j]=true;
				k++;
				if(k==p)
					break;
			}
		}
		//if(k==p)
		//	break;
	}
	k=p-k;
	return(k);


}


/******************************************************************************************/
/*  	Calculate the bootstrap value for pairs of sequences s and t save in bootVals     */
/******************************************************************************************/

int GetAvgBootstrapDist(Fasta **seqs,double **dist, double ***bootVals, int n, int bootreps)
{

	int i,j,t;
	/* Fill dist array with average bootstrap distances*/
	for(i=0;i<n;i++){
		for(t=i+1;t<n;t++){
			dist[i][t]=0;
			for(j=0;j<bootreps;j++){
				if(bootVals[j][i][t]<0)
					ErrorMessageExit("Problem debug!\n",1);
				dist[i][t]+=bootVals[j][i][t]; 

			}
				dist[t][i]=dist[i][t]=dist[i][t]/(double)bootreps;
		}/* for j<bootrepsPlusOne */
		dist[i][i]=99.9;

	}
	return(true);
}



/******************************************************************************************/
/*  																                      */
/*  	Bootscan has to select query and parental sequences and then call DoBootscan      */
/*  																					  */
/******************************************************************************************/

int Bootscan(char *File1, char *File2, double ***dist_frags, int numWins, Mintaxa **minresults,Fasta **seqs, double **dist, int maxDim, int n,
		    int step, int windowSize, int closestRel, int *resno, short int bootreps, int align)
{
	int  *Frag_Seq,p,times_max=0,numSeqs,avgBS;
	double min_d, ***bootVals;
	int i,j,k,s,t,c,startAS,ftime,min_idx;
	Fasta *seqsBS[NUMSEQS];
	FILE *fp2,*fp3,*fp1, *fp_d;//,*fp4;
	BKPNode *bkpLL;
	Mintaxa *mintaxa;
	int writeMatrix;

	 if((mintaxa = (Mintaxa *) malloc(sizeof(Mintaxa) * 20))==NULL)
		ErrorMessageExit("Bootscan 1: Error with Mintaxa memory allocation\n",1);
	
		if ((fp2 = (FILE *) fopen("BootResults.txt","w")) == NULL) 
			ErrorMessageExit("io2: error when opening BootResults.txt File\n",1);
		if ((fp3 = (FILE *) fopen("Frag_Dists.txt","w")) == NULL) 
			ErrorMessageExit("io2: error when opening Frag_Dists.txt files\n",1);
		if ((fp1 = (FILE *) fopen(File1,"w")) == NULL) 
			ErrorMessageExit("io2: error when opening output file\n",1);
		if ((fp_d = (FILE *) fopen(File2,"w")) == NULL) 
			ErrorMessageExit("io2: error when opening SeqDistances.txt files\n",1);

	//	if((bootVals = (double **)MatrixInit(sizeof(double), n, n))==NULL)
	//	  return(false);
		MALLOC(bootVals, sizeof(double **) * bootreps);
		for (i = 0; i < bootreps; i++) {
			MALLOC(bootVals[i], sizeof(double *) * n);
			for (j=0;j<n;j++)
				MALLOC(bootVals[i][j], sizeof(double) * n);
		}



		if((Frag_Seq= (int *) malloc((numWins)* sizeof (int)))==NULL)
			return(false);
		for(i=0;i<n;i++){
			if ((seqsBS[i] = ( Fasta *) malloc( sizeof( Fasta)))==NULL) 
						   ErrorMessageExit("Bootscan 2: Error with memory allocation\n",1);
		}

		for(i=0;i<n;i++){
			strcpy(seqsBS[i]->Id,seqs[i]->Id);
			strcpy(seqsBS[i]->Seq,seqs[i]->Seq);
			seqsBS[i]->time=seqs[i]->time;
		}

		SetSeed(seed);//Seed provided by user....
		/* Get whole sequence bootstrap values into dist_frags */
		if((DoBootscan(bootVals,NULL,1,seqsBS,  maxDim,  n, step, maxDim,closestRel,2,bootreps,fullBootscan))==false)
				  return(false);
		if((GetAvgBootstrapDist(seqs,dist,bootVals,n,bootreps))==false)
				  return(false);


		for(i=0;i<numWins;i++){
			for(j=0;j<n;j++)
				for(k=0;k<n;k++)
					dist_frags[i][j][k]=0;
		}
		printf("Scanning Phase - Windows:");
		writeMatrix=true;
		if(writeMatrix){
			if((DoBootscan(dist_frags,NULL,numWins,seqsBS,  maxDim,  n, step, windowSize,closestRel,1,bootreps,fullBootscan))==false)
					  return(false);

		}
//		else{
//			fp2 = (FILE *) fopen("BootMatrix_r0.txt","r");
//			i=0;
//			j=0;
//			while(i<numWins){
//				if((cc =fgets ((char *) temp, LINELIMIT,fp2))==NULL)
//					break;/* ONLY FOR TESTING */
//				t=0;
//				if(temp[0]=='\n'){
//					i++;j=0;
//					if((cc =fgets ((char *) temp, LINELIMIT,fp2))==NULL)
//						break;/* ONLY FOR TESTING */
//				}
//				k=j+1;
//				while(temp[t]!='\n'){
//					c=0;
//					while(temp[t]!=';')
//						read[c++]=temp[t++];
//					read[c]='\0';
//					t++;
//					dist_frags[i][j][k]=dist_frags[i][k][j]=atof(read);
//					k++;
//				}
//				j++;
//			}
//		}
		fp2 = (FILE *) fopen("BootResults.txt","w") ;

		printf("Detection Phase: ");
		s=1;
		ftime=seqs[0]->time;
		while(ftime==seqs[s]->time)
			s++;
		p=0;
		for (s;s<n;s++){
			fprintf(fp2,"BootStrap: %s;\n",seqs[s]->Id);
			t=s-1;
			while(seqs[t]->time == seqs[s]->time)
				t--;
			startAS=t;
			min_d=INF9999;
			/* Now get this guy s minimum distance from all others before him starting at t */
			for(t;t>=0;t--){
				if(reportDistances)
					fprintf(fp_d,"%s %s %lf %d \n",seqs[s]->Id, seqs[t]->Id, dist[s][t],(int)GetBootstrapValues(seqs,bootVals,s,t,bootreps));
				if(dist[s][t]<min_d){
					min_d=dist[s][t];
					min_idx=t;
				}

			}
			if(s==99)
				s=99;
			printf("%s:",seqs[s]->Id);
			fflush(stdout);
			/* Repeated Bootscanning */
			bkpLL=NULL;
			numSeqs=1;
			if(recOn)
				numSeqs=PickCandidates(seqs,Frag_Seq,dist_frags,numWins,min_idx,s,startAS,min_d);

			if(numSeqs>1){
				PrintFragments(fp3,Frag_Seq,seqs, dist, dist_frags, numWins,numSeqs, s , align);

				c=BootscanShrinkPool(fp2,seqs,Frag_Seq,step,windowSize,bootreps,numSeqs,s,numWins,maxDim,100-(numSeqs*5),60/step);
				if(c>1){
					PrintFragments(fp3,Frag_Seq,seqs, dist, dist_frags, numWins,c, s , align);
				}

				if(c>1){

					bkpLL=BootscanCandidates(fp2,seqs,Frag_Seq, step,  windowSize ,  bootreps,c,s,numWins,min_idx,dist[s][min_idx],align,maxDim,0,&avgBS);
				}
			}
			

			//startAS,s,numWins,  min_idx,windowSize,align,maxDim, 2, step,  windowSize ,  bootreps);
			if(OutputResults(numWins,s,startAS,0.0,min_d,min_idx,&times_max,NULL,bkpLL,bootVals,&mintaxa[0], minresults,seqs,dist,maxDim,n,fp1,0.0,maxDim/numWins,resno,align,avgBS,bootreps)==false)
				return(false);
			freeBKPNode(bkpLL);
		}

		for (i=0;i<n;i++){	
		if(seqsBS[i]) free(seqsBS[i]);
	}
	for(i=0; i<bootreps; i++)   {
		for(j=0; j<n; j++) 
			if(bootVals[i][j]) free(bootVals[i][j]);
		if(bootVals[i]) free(bootVals[i]);
	}
	free(bootVals);

	free(mintaxa);
	free(Frag_Seq);


	fclose(fp2);
	fclose(fp1);
	fclose(fp3);
	fclose(fp_d);
	return(true);
}




/**************************************************************/
/*  	Help function for ReadParams						  */
/**************************************************************/
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

void PrintUsage()
{
	fprintf(stderr, "Usage: MinPD <PARAMETER.FILE \n");
	fprintf(stderr, "\tFor a description of options available please\n");
	fprintf(stderr, "\trefer to manual included in package\n\n");
	exit(0);
}

/**************************************************************/
/*  	 Read Program Parameters							  */
/**************************************************************/
void ReadParams( char *inputFile, char *outputFile, int *opt, 
				int *align,  int *wSize, int *stSize)
{
	int  j,k;
	char ch, cch, str[255], *str2;
	FILE *fp1;
	//char *P;
	
	*align=0;
	
	if(feof(stdin)){
		fprintf(stderr, "Unable to read parameters from stdin\n");
		exit(0);
	}
	//str2="BEGIN TVBLOCK";
	fgets(str, 255, stdin);
	str2=strstr(str,"MINPD");

        // GN added next 3 statements
        //j = strlen(str);
       // str[j - 1] = '\0';
       // str[j - 2] = '\0';
	//if(strcmp(str, str2)!=0){//Problems in Linux with newline chars
	if(str2==NULL){
		fprintf(stderr, "Input does not contain MinPD parameters: %s\n",str);
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
	
	while(!feof(stdin)){
		ch=toupper(ch);
		switch (ch) {
			case 'A':
				if (fscanf(stdin, "%lf", &alpha)!=1 || alpha<0.0) {
					fprintf(stderr, "Bad Gamma Shape\n");
					PrintUsage();
				}
			break;
			
			case 'B':
				if (fscanf(stdin, "%d", &Bootstrap)!=1 || (Bootstrap!=0 && Bootstrap!=1)) {
					fprintf(stderr, "Bad [Bootstrap:1,Jackknife:0] value. Must be 0 or 1.\n");
					PrintUsage();
				}
			break;
			case 'C':
				if (fscanf(stdin, "%d", &crossOpt)!=1 || crossOpt<0 ||crossOpt>1) {
					fprintf(stderr, "Bad [crossover option] number. Must be 0 or 1.\n");
					PrintUsage();
				}
			break;
			case 'D':
				if (fscanf(stdin, "%d", &reportDistances)!=1 || reportDistances<0 ||reportDistances>1) {
					fprintf(stderr, "Bad [report all distances] number. Must be 0 or 1.\n");
					PrintUsage();
				}
			break;
			case 'E':
				if (fscanf(stdin, "%d", &seed)!=1 ) {
					fprintf(stderr, "Bad [bootscan seed] number\n");
					PrintUsage();
				}
			break;
			case 'F':
				if (fscanf(stdin, "%d", &recOn)!=1 || recOn<0  || recOn>1 ) {
					fprintf(stderr, "Bad [activate recombination detection] value. Must be 0 or 1.\n\n");
					PrintUsage();
				}
			break;
			case 'G':
				ch=fgetc(stdin);
				if(!isspace(ch)){
					if(*opt!=2){
						fprintf(stderr, "Select clustered bootstrap only with Bootscan RIP option\n\n");
						PrintUsage();
					}
					j=0;
					if(ch=='"'){
						ch=fgetc(stdin);
						do{
							codonFile[j]=ch;
							j++;
							ch=fgetc(stdin);
						} while(ch!='"');
						clustBoot=1;
					}
					else{
						fprintf(stderr, "Codon file needs to be enclosed in quotation marks\n\n");
						PrintUsage();
					}

				
					codonFile[j]='\0';
					if ((fp1 = (FILE *) fopen(codonFile, "r")) == NULL) {
						printf("io2: error when opening file:%s.\n%s\n",codonFile,strerror(errno));
							PrintUsage();
					}
					while(!feof(fp1)){
						cch=fgetc(fp1);
						if(cch==',')
							clustBoot++;
					}
					clustBoot++;
					if((codonList= (int *) malloc(clustBoot* sizeof (int)))==NULL)
						PrintUsage();
					clustBoot--;
					fseek(fp1,0,SEEK_SET);
					j=0;k=0;
					while(!feof(fp1)){
						cch=fgetc(fp1);
						if(cch==','){
							str[k]='\0';
							codonList[j]=atoi(str);
							k=0;
							j++;
						}
						else
							str[k++]=cch;
					}

					str[k]='\0';
					codonList[j]=atoi(str);
					fclose(fp1);
				}

			break;				
			case 'H': 
				if (fscanf(stdin, "%d", &BootThreshold)!=1 || BootThreshold<50 ||BootThreshold>100 ) {
					fprintf(stderr, "Bad [bootstrap threshold] value. Must be between 50 and 100. \n\n");
					PrintUsage();
				}
			break;
			
			case 'K':
				if (fscanf(stdin, "%d", &printBoot)!=1 || (printBoot!=0 && printBoot!=1)) {
					fprintf(stderr, "Bad [show bootstrap values] option. Must be 0 or 1.\n");
					PrintUsage();
				}
			break;
			case 'L': 
				if (fscanf(stdin, "%d", &closestRel)!=1 ) {
					fprintf(stderr, "Bad [bootscan closest relative] option\n\n");
					PrintUsage();
				}
			break;
			case 'I':
				ch=fgetc(stdin);
				if(isspace(ch)){
						fprintf(stderr, "Input file needs to be enclosed in quotation marks\n\n");
						PrintUsage();
				}
				else{
					j=0;
					if(ch=='"'){
						ch=fgetc(stdin);
						do{
							inputFile[j]=ch;
							j++;
							ch=fgetc(stdin);
						}while(ch!='"');
					}
					else{
						fprintf(stderr, "Input file needs to be enclosed in quotation marks\n\n");
						PrintUsage();
					}

				
					inputFile[j]='\0';
				}

			break;				
			case 'O':
				ch=fgetc(stdin);
				if(isspace(ch)){
					fprintf(stderr, "Output file needs to be enclosed in quotation marks\n\n");
					PrintUsage();
				}
				else{
					j=0;
					if(ch=='"'){
						ch=fgetc(stdin);
						do{
							outputFile[j]=ch;
							j++;
							ch=fgetc(stdin);
						}while(ch!='"');
					}
					else{
						fprintf(stderr, "Output file needs to be enclosed in quotation marks\n\n");
						PrintUsage();
					}

				
					outputFile[j]='\0';
				}

			break;
			case 'P':
				if (fscanf(stdin, "%lf", &PCCThreshold)!=1|| PCCThreshold<0.0 || PCCThreshold>1.0) {
					fprintf(stderr, "Bad [PCC threshold] value\n\n");
					PrintUsage();
				}
			break;
			case 'R':
				if (fscanf(stdin, "%d", opt)!=1) {
					fprintf(stderr, "Bad [recombination detection] value\n\n");
					PrintUsage();
				}
			break;
			case 'S':
				if (fscanf(stdin, "%d", stSize)!=1 ||*wSize<50 ||*stSize>*wSize || *stSize<10 ) {
					if(*opt==2){
						fprintf(stderr, "Bad [bootscan step size] value\n\n");
						if(*stSize>*wSize || *stSize<10) 
							fprintf(stderr,"Usage: 10<step-size<=window-size\n");
						if(*wSize<50)
							fprintf(stderr,"Usage: window-size>50!\n");
						exit(0);
					}
				}
			break;
			case 'T':
				if (fscanf(stdin, "%d", &BootTieBreaker)!=1 || BootTieBreaker<0  || BootTieBreaker>1 ) {
					fprintf(stderr, "Bad [[bootstrap recomb. tiebreaker option] ]. Must be 0 or 1.\n\n");
					PrintUsage();
				}
			break;
			case 'V':
				model=-1;
				fgets(str, 5, stdin);
				if (strcmp(str, "JC69")==0)
					model=JC69;
				else if (strncmp(str, "K2P",3)==0)
					model=K80;
				else if (strcmp(str, "TN93")==0)
					model=TN93;
				if (model==-1) {
					fprintf(stderr, "Unknown Model: %s\n\n", str);
					PrintUsage();
					exit(0);
				}
			break;
			case 'W':
				if (fscanf(stdin, "%d", wSize)!=1 && *opt==2 ) {
					fprintf(stderr, "Bad [bootscan window size] value\n\n");
					PrintUsage();
				}
			break;
			default :
				fprintf(stderr, "Incorrect parameter: %c\n\n", ch);
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
		
}

void CheckTimes(Fasta **seqs, int n){
int i, time;
time=seqs[0]->time;
	for (i=1;i<n;i++){
		if(time!=seqs[i]->time){
				break;
		}
	}
	if(i==n){
		printf("\nAll sequences are from the same time point!\n");
		exit(1);
	}
}

/**************************************************************/
/*  					     								  */
/*  					MAIN								  */
/*  														  */
/*    Read in files, run pairwise alignment, calc distances   */
/*  														  */
/**************************************************************/
int main(int argc, char* argv[])
{

	FILE	*fp1;
	char	*cc, inputFile[100], outputFile1[100], outputFile2[100];
	char	temp[LINELIMIT], temp2[MAXLENGHT], timechar[3];
	int		i, k, n,j,win_Count, fr_Dim, align, opt, res_no =0; 
	double	**dist, ***dist_frags;
	double	GOP, GEP, match, mismatch,  mutrate;
	unsigned int	maxDim;
	Mintaxa *minresults[NUMSEQS];
	int		wSize=0, stSize,numWins;
	short int bootreps=100;
	int closestRel=0;

	
	GOP=5; //+ log(len2);
	GEP=1;
	match = 2;
	mismatch=0;
	maxDim=0;

	printf("Reading File:... ");
	fflush(stdout);
	ReadParams(inputFile,outputFile1, &opt,  
				 &align,  &wSize,&stSize);

	if(opt==0)/* Turn off recombination detection*/
		win_Count=1;
	else if (opt>2){
		fullBootscan=true;
		if(opt==4)
			distPenalty=true;
	}


	strcpy(outputFile2,"d.txt\0");//
	mutrate=0;
	if ((fp1 = (FILE *) fopen(inputFile, "r")) == NULL) {
		printf("io: error when opening file:%s.\n%s\n",inputFile,strerror (errno));
			return(1);
	}


	/* FIRST read in all sequences */
	n=-1;
	do{
		cc =fgets ((char *) temp, LINELIMIT,fp1) ;
		if (cc == NULL) break;
		//printf("temp=%s  \n",temp);

		if (temp[0]=='>'){
			/* A1 name line */
			if(n>=0) {
				strcpy(seqs[n]->Seq, temp2);
				if(strlen(temp2)>maxDim) maxDim=strlen(temp2);
			}
			n++;
			if ((seqs[n] = ( Fasta *) malloc( sizeof( Fasta)))==NULL)  
				   ErrorMessageExit("main: Error with memory allocation\n",1);
			k=0;
			i=1;
			timechar[0]=temp[i];
			timechar[1]=temp[i+1];
			timechar[2]=temp[i+2];
			timechar[3]='\0';
			seqs[n]->time = atoi(timechar);
			while (temp[i]!= ' ' && temp[i]!= '\0' && temp[i]!= '\n' && temp[i]!= '\r' && temp[i]!= '\t')
					temp2[k++]=temp[i++];
			temp2[k]='\0';
			if (strlen(temp2)>=NAMELEN){
				   printf("Error: Sequence ID name can't be longer than %d characters\nand must be followed by a space!\n",NAMELEN);
				   exit(1);
			}

			strcpy(seqs[n]->Id, temp2);
			k=0;	
			temp2[k]='\0';
		}
		else if (cc != NULL){
			/* You got a line of nucleotides */
			i=0;
			k=strlen(temp);
			for(i=0;i<k;i++){
				if(isalpha(temp[i]))
					temp[i]=toupper(temp[i]);
			}
			k=0;
			if(temp[0]=='-' ||temp[0]=='A' || temp[0]=='T' || temp[0]=='C' || temp[0]=='G' || temp[0]=='U'){
				strcat(temp2, temp);
				if(temp2[strlen(temp2)-1]=='\n'||temp2[strlen(temp2)-1]=='\r') temp2[strlen(temp2)-1]='\0'; /* remove line break */
			}
			/* else {  ignore it? } */
		}

	}while (cc != NULL);


	strcpy(seqs[n]->Seq, temp2);
	fclose(fp1);

	for(i=0;i<=n;i++)
		heapify_Fasta(seqs,i);
	heapsort_Fasta(seqs,n);

	CheckTimes(seqs,n);
  
	// maxDim++;

	if((dist = (double **)MatrixInit(sizeof(double), n+1, n+1))==NULL)
      return(0);

	if(wSize>0){
		numWins=(int)((maxDim-wSize)/stSize)+1;
		if(bootreps>numWins && opt>1)
			fr_Dim=bootreps;/* dist_frags will be used to store bootstrap results as well */
		else
			fr_Dim=numWins;
	}
	else{
		win_Count=maxDim/100;
		fr_Dim=win_Count;
	}

	MALLOC(dist_frags, sizeof(double **) * fr_Dim);
	for (i = 0; i < fr_Dim; i++) {
		MALLOC(dist_frags[i], sizeof(double *) * (n+1));
		for (j=0;j<(n+1);j++)
			MALLOC(dist_frags[i][j], sizeof(double) * (n+1));

	}
	res_no=1;
	if(opt<2){
		printf("Pairwise distances... ");
		fflush(stdout);
		res_no=DistOnly(seqs, dist, dist_frags,  maxDim, n,  fr_Dim, model,wSize,stSize,numWins);
	}
	if(res_no==0){
			 fprintf(stderr,"ERROR calling Align or Distance Calculation functions!\n");
			 return(false);
	}
	res_no =0;
	if(opt>1){/*Sliding Window analysis*/
		printf("Sliding Window analysis... ");
		fflush(stdout);
		//res_no=BootscanTest(minresults,seqs,dist,maxDim,n,stSize,wSize,0,&res_no,PCCThreshold);
		if( Bootscan(outputFile1,outputFile2,dist_frags,numWins,minresults, seqs, dist, maxDim, n+1,  stSize, wSize,closestRel,&res_no,bootreps,align)!=true){
			fprintf(stderr,"ERROR calling Bootscan function!\n");
			 return(false);
		}
	}
	else{
		if(GetMinDist(minresults, seqs,dist, 
			dist_frags, maxDim, n,outputFile1,outputFile2,
			mutrate,fr_Dim, &res_no, align,crossOpt,stSize,wSize)!=true){
			 fprintf(stderr,"ERROR calling GetMinDist function!\n");
			 return(false);
		}
	}
	BuildPartialNJTrees(seqs,minresults, res_no, dist,outputFile1,n);

/* Free up memory */
	for(i=0;i<res_no;i++) free(minresults[i]);

	for(i=0; i<(n+1); i++)   if(dist[i]) free(dist[i]);
	free(dist);

	for(i=0; i<fr_Dim; i++)   {
		for(j=0; j<(n+1); j++) 
			if(dist_frags[i][j]) free(dist_frags[i][j]);
		if(dist_frags[i]) free(dist_frags[i]);
	}
	free(dist_frags);


	for (i=0;i<=n;i++)
		 free(seqs[i]);
	return (0);
	free(codonList);

}

