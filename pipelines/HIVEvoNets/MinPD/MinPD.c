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

#include "NJ.h"
#include "MinPD.h"
#include "Heapsort.h"

#define GammaDis(x,alpha) (alpha*(pow(x,-1/alpha)))


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






/*		MATRIX INITIALIZATION */
char **MatrixInit(int s, int dim1, int dim2)
{
	char  **matrix  = NULL;
	int   i;
   
	/* 1st dim */
	if((matrix = (char **)malloc(dim1 * sizeof(char *))) == NULL)
      return(NULL);
	for(i=0; i<dim1; i++)   matrix[i] = NULL;

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
/*																	 */
/*		Calculate TN93 distance										 */
/*  	Distance is calculated using TN93 + gamma with alpha =0.5	 */
/*********************************************************************/
double TN93distance(int pT, int pC, int pG, int pA, int p1, int p2, int gaps,
					int matches, int al_len, double alpha, int s, int t)
{
	double		p[4],P1,P2,Q,R,Y,TC,AG,e1,e2,e3,e4,d,n2, lenght;


	lenght=al_len-gaps;

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

	if (alpha<=0) { e1=-log(e1); e2=-log(e2); e4=-log(e4); }
	else   { e1=GammaDis(e1,alpha); e2=GammaDis(e2,alpha); e4=GammaDis(e4,alpha);}


	d=((2*AG/R) * e1) + ((2*TC/Y)* e2) + (2*e3*e4);
	if (alpha>0) d = d - (2*alpha*(AG+TC+(R*Y)));

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

/**********************************************************************/
/*  																  */
/*  	Distance Calculation when Sequences already aligned			  */
/*  																  */
/**********************************************************************/
int DistOnly(Fasta **seqs, double **dist, double ***dist_frags, int maxdim, int n,
		   double alpha, int Fr_Count)
{
	char		seq1[MAXLENGHT], seq2[MAXLENGHT];
	int			i,al_len,s,t;
	int			P1,P2, pT,pC,pG,pA,gaps,matches;
	int			P1_fg,P2_fg, pT_fg,pC_fg,pG_fg,pA_fg,gaps_fg,matches_fg,fr_size,fr_dim,fr_counter;
	/* get all pairs distance */
	for (s=1;s<=n;s++){
		t=s-1;
		/* Now get this guy's distance from all others before him  */
		for(t;t>=0;t--)
		{
			strcpy(seq1,seqs[s]->Seq);
			strcpy(seq2,seqs[t]->Seq);
			al_len = strlen(seq1)+1;
   			/* Store Alignment Calculate Distance */
			pT=0;pC=0;pG=0;pA=0;P1=0;P2=0;gaps=0;matches=0;fr_dim=0;
			P1_fg=0;P2_fg=0; pT_fg=0;pC_fg=0;pG_fg=0;pA_fg=0;gaps_fg=0;matches_fg=0;fr_counter=0;
			fr_size=al_len/Fr_Count;
			i=0;
			while(i<al_len){
				if (fr_size==fr_counter && fr_dim<(Fr_Count-1)){
					dist_frags[fr_dim][s][t]=dist_frags[fr_dim][t][s]=TN93distance( pT_fg,  pC_fg,  pG_fg,  pA_fg,  P1_fg,  P2_fg,  gaps_fg,  matches_fg, fr_counter,  alpha,s,t);
					fr_dim++;
					P1_fg=0;P2_fg=0; pT_fg=0;pC_fg=0;pG_fg=0;pA_fg=0;gaps_fg=0;matches_fg=0;fr_counter=0;
				}
				fr_counter++;
				if (seq1[i]==seq2[i]){ 
					if (seq2[i]=='T') {pT+=2;pT_fg+=2;}
					if (seq2[i]=='C') {pC+=2;pC_fg+=2;}
					if (seq2[i]=='A') {pA+=2;pA_fg+=2;}
					if (seq2[i]=='G') {pG+=2;pG_fg+=2;}
					if (seq1[i]=='-') 
						{gaps++;gaps_fg++;}
					else
						{matches++;matches_fg++;}
				}
				else {
					/* Gather nucleotides freqs and transition/transversion freqs */
					if (seq1[i]=='-'||seq2[i]=='-'){
						gaps++;gaps_fg++;
					} else {/* Count only transitions*/
						if (seq1[i]=='T') {
							pT++;pT_fg++;
							if(seq2[i]=='C') {P2++;P2_fg++;}
						}
						if (seq1[i]=='C') {
							pC++;pC_fg++;
							if(seq2[i]=='T') {P2++;P2_fg++;}
						}
						if (seq1[i]=='A') {
							pA++;pA_fg++;
							if(seq2[i]=='G') {P1++;P1_fg++;}
						}
						if (seq1[i]=='G') {
							pG++;pG_fg++;
							if(seq2[i]=='A') {P1++;P1_fg++;}
						}
						if (seq2[i]=='T') {pT++;pT_fg++;}
						if (seq2[i]=='C') {pC++;pC_fg++;}
						if (seq2[i]=='A') {pA++;pA_fg++;}
						if (seq2[i]=='G') {pG++;pG_fg++;}
					}
				}
				i++;
			}/* i<al_len loop*/

			dist_frags[fr_dim][s][t]=dist_frags[fr_dim][t][s]=TN93distance( pT_fg,  pC_fg,  pG_fg,  pA_fg,  P1_fg,  P2_fg,  gaps_fg,  matches_fg, fr_counter,  alpha,s,t);
			dist[s][t]=dist[t][s]=TN93distance( pT,  pC,  pG,  pA,  P1,  P2,  gaps,  matches,al_len,  alpha,s,t);

		}/* End of t loop*/
	 }/* End of s loop*/
   return(true);
}


/**********************************************************************/
/*  																  */
/*  	NEEDLEMAN WUNSCH Pairwise Alignment & Distance Calc.		  */
/*  																  */
/**********************************************************************/
int Align_Dist(Fasta **seqs, double **dist, double ***dist_frags, int maxdim, int n,
		   double GOP, double  GEP, double match, double mismatch,double alpha, int Fr_Count)
{
	char		al1[MAXLENGHT*2], al2[MAXLENGHT*2];
	int			len1,len2,al_len;
	COORD		**trace;
	int			i,j,i2,j2,xMax,yMax,x,y,k,s,t,count,c2;
	double		**matrix,
				diago,left,up,topgap,bottomgap,
				max,MaxScore,
				amatch;
				/*Distance Variables:*/
	int			P1,P2, pT,pC,pG,pA,gaps,matches;
	int			P1_fg,P2_fg, pT_fg,pC_fg,pG_fg,pA_fg,gaps_fg,matches_fg,fr_size,fr_dim,fr_counter;
	char		seq1[MAXLENGHT], seq2[MAXLENGHT];

#if DEBUG_MODE == 1
	FILE		*fp2;
	char		seqName1[20],seqName2[20], aldiff[MAXLENGHT*2];
	int			offset,m;
	if ((fp2 = (FILE *) fopen("Alignment.txt","w")) == NULL) {
		printf("io2: error when opening SeqDistances.txt files\n");
			return(1);
	}
#endif
	if((matrix = (double **)MatrixInit(sizeof(double), maxdim, maxdim))==NULL)
      return(0);
	if((trace   = (COORD **)MatrixInit(sizeof(COORD), maxdim, maxdim))==NULL)
		return(0);

	count=0;c2=0;	

	/* align all pairs */
	 for (s=1;s<=n;s++){
		t=s-1;
		/* Now align this guy i with all others before him  */
		for(t;t>=0;t--)
		{
			count++;c2++;
			if (c2==50){ printf(" %d ",count); c2=0;} /* Progress indicator */
			strcpy(seq1,seqs[s]->Seq);
			strcpy(seq2,seqs[t]->Seq);

	len1 = strlen(seq1)+1;
	len2 = strlen(seq2)+1;

	/* Initialize the matrices  */
	matrix[0][0]=0;
	matrix[1][0]=matrix[0][1]=0 - GOP;
	for (i=2;i<maxdim;i++) matrix[i][0]=matrix[i-1][0] - GEP;
	for (j=2;j<maxdim;j++) matrix[0][j]=matrix[0][j-1] - GEP;
	MaxScore=0;
	xMax=yMax=0;
	for(i=0;i<maxdim;i++)
	{
		for(j=0;j<maxdim;j++)
		{
			trace[i][j].x = -1;
			trace[i][j].y = -1;
		}
	}
	/* ALIGNMENT   */
	/* Fill Matrix */
	for (i=1;i<len1;i++){
		i2=i-1;
		if(seq1[i2]!='A' && seq1[i2]!='T' && seq1[i2]!='C' && seq1[i2]!='G' && seq1[i2]!='U'){
			printf ("Nucleotide data can only be A G C T U, but found '%c' at pos %d\n",seq1[i2],i2);
			return(0);
		}

		for (j=1;j<len2;j++) {
			/*Diagonal */
			j2=j-1;
			if(seq1[i2] == seq2[j2]) amatch=match; else amatch=mismatch;
			diago = matrix[i-1][j-1] + amatch;
			
			/* Left */
			if (i2==0) amatch=mismatch;
			else { if(trace[i-1][j].x == i-2 && trace[i-1][j].y == j) amatch=mismatch; else amatch=match;}
			/* we're reusing amatch, to reveal if left cell points to diagonal */
			if(amatch==match) left = matrix[i-1][j] - GOP; else left = matrix[i-1][j] - GEP;
			/* Point to highest scoring cell when new scores are idettical */
			if (diago==left) if(matrix[i-1][j-1] < matrix[i-1][j]) diago--;
			
			/* Up */
			if (j2==0) amatch=mismatch;
			else { if(trace[i][j-1].x == i && trace[i][j-1].y == j-2) amatch=mismatch; else amatch=match;}
			/* we're reusing amatch, to reveal if upper cell points to diagonal */
			if(amatch==match) up = matrix[i][j-1] - GOP; else up = matrix[i][j-1] - GEP;
			/* Point to highest scoring cell when new scores are idettical */
			if (diago==up) if(matrix[i-1][j-1] < matrix[i][j-1]) diago--;
			if (left==up) if(matrix[i-1][j] < matrix[i][j-1]) left--;
			
			
			/* Get MAX */
			max=MAX3(diago,up,left);
			if (max == diago) {
				matrix[i][j]=diago;
				trace[i][j].x=i-1;
				trace[i][j].y=j-1;
				}
			else if (max == up) {
				matrix[i][j]=up;
				trace[i][j].x =i;
				trace[i][j].y =j-1;
				}
			else  {
				matrix[i][j]=left;
				trace[i][j].x =i-1;
				trace[i][j].y=j;
				}
			if (max >= MaxScore){
				MaxScore=max;
				xMax=i;
				yMax=j;
				}
			}  /* end for j loop */
	}/* end for i loop */


 #if DEBUG_MODE >1

	i=xMax; j=yMax;
	while ( i>0 && j>0 ){
		printf("(%d,%d) - ",i,j);
		i2=i;
		j2=j;
		i=trace[i2][j2].x;
		j=trace[i2][j2].y;
	}

	printf("\n");
	printf("xMax=%d yMax=%d maxdim =%d before TRaceback\n",xMax,yMax,maxdim);
#endif

   	/* Store Alignment Calculate Distance */
	pT=0;pC=0;pG=0;pA=0;P1=0;P2=0;gaps=0;matches=0;
	P1_fg=0;P2_fg=0; pT_fg=0;pC_fg=0;pG_fg=0;pA_fg=0;gaps_fg=0;matches_fg=0;fr_counter=0;
	i=xMax; j=yMax;
	fr_dim=0;
	fr_counter=0;
	if (xMax>yMax)/*for fragments distance matrix*/
		fr_size=xMax/Fr_Count;
	else
		fr_size=yMax/Fr_Count;
	x=y=0;
 	/*	bottomgap */
	i2=len1-1;
	while(i2>xMax){
		al1[x++]=seq1[i2-1];
		al2[y++]='-';
		i2--;gaps++;gaps_fg++;fr_counter++;
	}

	/*	topgap */
	i2=len2-1;
	while(i2>yMax){
		al1[x++]='-';
		al2[y++]=seq2[i2-1]; //seq1 is shorter!
		i2--;gaps++;gaps_fg++;fr_counter++;
	}

	topgap = bottomgap = 1;
	while (i>0 && j>0 ){

		if (fr_size==fr_counter && fr_dim<(Fr_Count-1)){
			//if (count==210){ printf(" dist_frags: fr_dim=%d s=%d t=%d fr_counter=%d\n",fr_dim,s,t,fr_counter); c2=0;} /* Progress indicator */
			dist_frags[fr_dim][s][t]=dist_frags[fr_dim][t][s]=TN93distance( pT_fg,  pC_fg,  pG_fg,  pA_fg,  P1_fg,  P2_fg,  gaps_fg,  matches_fg, fr_counter,  alpha,s,t);
			fr_dim++;fr_counter=0;
			P1_fg=0;P2_fg=0; pT_fg=0;pC_fg=0;pG_fg=0;pA_fg=0;gaps_fg=0;matches_fg=0;fr_counter=0;
		}
		fr_counter++;
		if (topgap && bottomgap) {
			al1[x++]=seq1[i-1];
			al2[y++]=seq2[j-1];
		}
		else if (topgap) {
			al1[x++]='-';
			al2[y++]=seq2[j-1];
		}
		else if (bottomgap) {
			al1[x++]=seq1[i-1];
			al2[y++]='-';
		}
		k=x-1;
		if (al1[k]==al2[k]){ 
			if (al2[k]=='T') {pT+=2;pT_fg+=2;}
			if (al2[k]=='C') {pC+=2;pC_fg+=2;}
			if (al2[k]=='A') {pA+=2;pA_fg+=2;}
			if (al2[k]=='G') {pG+=2;pG_fg+=2;}
			matches++;matches_fg++;
		}
		else {
			/* Gather nucleotides freqs and transition/transversion freqs */
			if (al1[k]=='-'||al2[k]=='-'){
				gaps++;gaps_fg++;
			} else {
				if (al1[k]=='T') {
					pT++;pT_fg++;
					if(al2[k]=='C') {P2++;P2_fg++;}
				}
				if (al1[k]=='C') {
					pC++;pC_fg++;
					if(al2[k]=='T') {P2++;P2_fg++;}
				}
				if (al1[k]=='A') {
					pA++;pA_fg++;
					if(al2[k]=='G') {P1++;P1_fg++;}
				}
				if (al1[k]=='G') {
					pG++;pG_fg++;
					if(al2[k]=='A') {P1++;P1_fg++;}
				}
				if (al2[k]=='T') {pT++;pT_fg++;}
				if (al2[k]=='C') {pC++;pC_fg++;}
				if (al2[k]=='A') {pA++;pA_fg++;}
				if (al2[k]=='G') {pG++;pG_fg++;}
			}

		}
		i2=i;j2=j;
		i=trace[i2][j2].x; 
		j=trace[i2][j2].y; 
  		topgap    = (trace[i2][j2].y>trace[i][j].y);
		bottomgap = (trace[i2][j2].x>trace[i][j].x);
 
	}
		/*	bottomgap */
	while(i>0){
		al1[x++]=seq1[i-1];
		al2[y++]='-';
		i--;gaps++;gaps_fg++;fr_counter++;
	}

	/*	topgap */
	while(j>0){
		al1[x++]='-';
		al2[y++]=seq2[j-1];
		j--;gaps++;gaps_fg++;fr_counter++;
	}
	al1[x]='\0';
	al2[y]='\0';
	al_len=x-1;
	dist_frags[fr_dim][s][t]=dist_frags[fr_dim][t][s]=TN93distance( pT_fg,  pC_fg,  pG_fg,  pA_fg,  P1_fg,  P2_fg,  gaps_fg,  matches_fg, fr_counter,  alpha,s,t);


 #if DEBUG_MODE >1
	printf("AFTER Traceback\n al1=%s\n al2=%s\n",al1,al2);
	printf("Matrix: len2=%d, len1=%d\n-------\n",len2,len1);
	for(j=0; j<len2;j++){
	 for(i=0; i<len1; i++)
		printf("%lf ",matrix[i][j]);
	 printf("\n");
	}

	printf("Path:\n-----\n");
	for(j=0; j<len2;j++){
	 for(i=0; i<len1; i++)
		printf("(%d,%d) ",trace[i][j].x,trace[i][j].y);
	 printf("\n");
	}
#endif
    
/* Debug Code */
#if DEBUG_MODE ==1 	/* Print Alignment */
		offset = al_len;
		len1 = strlen(seqs[s]->Id);
		strcpy(seqName1,seqs[s]->Id);
		if(seqName1[len1-1]=='\n')seqName1[len1-1]=' ';
		for (j=len1; j<19;j++)
			seqName1[j]=' ';
		seqName1[19]='\0';
		/* Learn C: In memory: seqName1[20]= Seqname2[0] */
		len1 = strlen(seqs[t]->Id);
		strcpy(seqName2,seqs[t]->Id);
		if(seqName2[len1-1]=='\n')seqName2[len1-1]=' ';
		for (j=len1; j<19;j++)
			seqName2[j]=' ';
		seqName2[19]='\0';

		fprintf(fp2,"\n\n%s ",seqName1);

		 for(m=0,k=al_len; k>=0; k--){
         /* print seq1  */
            m++;
            if(m>OFFSET){    /* after OFFSET chars print seq2*/
 			fprintf(fp2,"\n%s ",seqName2);
               m=1;
               for(j=offset; j>offset-OFFSET; j--) fprintf(fp2,"%c",al2[j]);
 				fprintf(fp2,"\n                    ");
                for(j=offset; j>offset-OFFSET; j--) fprintf(fp2,"%c",aldiff[j]);
				fprintf(fp2,"\n\n");
                offset -= OFFSET;
			   
 			   fprintf(fp2,"%s ",seqName1);
			}
			fprintf(fp2,"%c",al1[k]);
			if (al1[k]==al2[k]) 
				aldiff[k]='*'; 
			else 
				aldiff[k]=' ';
		 }
		/* print rest of seq2 */
 		fprintf(fp2,"\n%s ",seqName2);
		for(j=offset; j>=0; j--) fprintf(fp2,"%c",al2[j]);
		fprintf(fp2,"\n                    ");
		for(j=offset; j>=0; j--) fprintf(fp2,"%c",aldiff[j]);
#endif
/* End of Debug Code */

		/* Now calculate the actual pairwise distance */
		dist[s][t]=dist[t][s]=TN93distance( pT,  pC,  pG,  pA,  P1,  P2,  gaps,  matches,al_len+1,  alpha,s,t);
		
		}/* For t loop */
	 }/*For s loop */

printf("Count %d\n",count);

      for(i=0; i<maxdim; i++)   if(matrix[i]) free(matrix[i]);
		free(matrix);
      for(i=0; i<maxdim; i++)   if(trace[i]) free(trace[i]);
		free(trace);


   return(true);
}

/**********************************************************************/
/* 																      */
/*  	Save Minimum distances ancestors							  */
/*  																  */
/**********************************************************************/
int SaveMinResults(Mintaxa **minresults, int *resno, int a, int c, double dist)
{
	if ((minresults[*resno] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
	   printf("Error with memory allocation\n");
	   return(false);
	}
	minresults[*resno]->Anc_Idx=a;
	minresults[*resno]->Child_Idx=c;
	minresults[*resno]->dist=dist;
	(*resno)++;
	return(true);

}
/**********************************************************************/
/* 																      */
/*  	OptionalRecPrintout					              		      */
/*  																  */
/**********************************************************************/
void OptionalRecPrintout(FILE *fp4,  RecRes *recRes,Fasta **seqs,int *Frag_Seq,FDisNode (*recSolutions)[3][2], 
						 int i, int l, int r, int p, int q, int f, int index, int last, int BKP, int Fr_Count,int align,int fr_size)
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
				d_min=(recSolutions[f][p][l].dist+recSolutions[f][q][r].dist)/Fr_Count;
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

			OptionalRecPrintout(fp4,recRes,seqs,Frag_Seq,recSolutions,i,l,r,p,q,f,index,last,BKP,Fr_Count,align,fr_size);
		 }
		 else{ /* take new bkp recRes[1] */
			 f=recRes[i].bkp;
			 d_min=recRes[i].dist;
			 l=p=q=0;
			 r=1;
			 if(align)
				 BKP=(Fr_Count-1-f)*fr_size;
			 else
				 BKP=(f+1)*fr_size;
			fprintf(fp4,"%s|%d|%s;%lf\n",seqs[Frag_Seq[recSolutions[f][p][l].Idx]]->Id,BKP,
				seqs[Frag_Seq[recSolutions[f][q][r].Idx]]->Id,d_min);

			OptionalRecPrintout(fp4,recRes,seqs,Frag_Seq,recSolutions,i+1,l,r,p,q,f,index,last,BKP,Fr_Count,align,fr_size);

		 }
	}

}
/**********************************************************************/
/* 							FillArrayInOrder					      */
/**********************************************************************/
void FillArrayInOrder(int *arrayInOrder, double ***dist_frags, int  *Frag_Seq, int s, int k, int rec_idx)
{
	int t,j,p,q;
			arrayInOrder[0]=Frag_Seq[0];
			p=1;
			for(t=1;t<rec_idx;t++){ /* Loop through sequences to sort by distance*/
				for(j=0;j<p;j++){/*loop through array */
					if(dist_frags[k][s][Frag_Seq[t]]<= dist_frags[k][s][arrayInOrder[j]]){
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

}



/**********************************************************************/
/* 																      */
/*  	Get Recombination Results by calculating            		  */
/*  	left & right Average Distances								  */
/*  																  */
/**********************************************************************/
BKPNode *GetRecSolutionsII(FILE *fp4,  double ***dist_frags, Fasta **seqs, int  *Frag_Seq, 
						 int rec_idx, int s, int Fr_Count, int min_seq, int fr_size, double min_seq_dist, int align)
{
 int j, k,  t,left_min,right_min,p,BKP, l=0,r=1;//,crossOvers,leftIsMin;
 double d_lmin, d_rmin,  d, recThresh;
 BKPNode *RecStorage,*first;
 FDisNode recSolutions[NUMSEQS][3][2];
 RecRes recRes[3];

 if(Fr_Count==8)
	 recThresh=0.1;//0.05 with crossover check variant
 else
	 recThresh=0.125;

 for(k=0;k<Fr_Count;k++){
	for(t=0;t<3;t++){
		recSolutions[k][t][l].Idx=-1;
		recSolutions[k][t][r].Idx=-1;
		recSolutions[k][t][l].dist=INF9999;
		recSolutions[k][t][r].dist=INF9999;

	}
 }
	for(t=0;t<3;t++){
		recRes[t].bkp=0;
		recRes[t].dist=INF9999;
	}


 for (j=0;j<Fr_Count-1;j++){
	left_min=0; /* To find/replace entry with smallest dis */
	right_min=0;
	d_lmin=INF9999;
	d_rmin=INF9999;
	for(t=0;t<rec_idx;t++){ /* Loop through all sequences*/
		//printf("%s\n",seqs[Frag_Seq[t]]->Id);
		d=0;
		for(k=0;k<=j;k++)/* Get t's min average distance from j left fragments */
		 d=d+dist_frags[k][s][Frag_Seq[t]];
 
		if(d<=d_lmin){
			d_lmin=d;
			recSolutions[j][left_min][l].dist=d;
			recSolutions[j][left_min++][l].Idx=t;
			if(left_min==3)
				left_min=0;/* Store 3 best results*/

		}


		d=0;
		for(k=Fr_Count-1;k>j;k--)/* Get t's min average distance from Fr_Count-j right fragments */
		 d=d+dist_frags[k][s][Frag_Seq[t]];
		if(d<=d_rmin){
			d_rmin=d;								/* To fix: if t is already in left_min and left_min has more frags ....*/
			recSolutions[j][right_min][r].dist=d;
			recSolutions[j][right_min++][r].Idx=t;
			if(right_min==3)
				right_min=0;/* Store 3 best results*/
		}

	}
	/* recSolutions now contains 3 seqs with minAvg dist up to j and 3 from j on*/
	if(left_min==0) left_min=2;
	else left_min--; /* left_min points to 1st place seq with min distance*/
	if(right_min==0) right_min=2;
	else right_min--;/* right_min points to 1st place seq with min distance*/
	/* reorder as 1st place, runner-up and third place*/
	if(left_min>0){/* Else already in 1st place*/
		k=recSolutions[j][0][l].Idx;
		d=recSolutions[j][0][l].dist;
		recSolutions[j][0][l].Idx=recSolutions[j][left_min][l].Idx;
		recSolutions[j][0][l].dist=recSolutions[j][left_min][l].dist;
		recSolutions[j][left_min][l].Idx=k;
		recSolutions[j][left_min][l].dist=d;
	}
	if(recSolutions[j][1][l].dist>recSolutions[j][2][l].dist){
		k=recSolutions[j][1][l].Idx;
		d=recSolutions[j][1][l].dist;
		recSolutions[j][1][l].Idx=recSolutions[j][2][l].Idx;
		recSolutions[j][1][l].dist=recSolutions[j][2][l].dist;
		recSolutions[j][2][l].Idx=k;
		recSolutions[j][2][l].dist=d;
	}
 
	if(right_min>0){/* Else already in 1st place*/
		k=recSolutions[j][0][r].Idx;
		d=recSolutions[j][0][r].dist;
		recSolutions[j][0][r].Idx=recSolutions[j][right_min][r].Idx;
		recSolutions[j][0][r].dist=recSolutions[j][right_min][r].dist;
		recSolutions[j][right_min][r].Idx=k;
		recSolutions[j][right_min][r].dist=d;
	}
	if(recSolutions[j][1][r].dist>recSolutions[j][2][r].dist){
		k=recSolutions[j][1][r].Idx;
		d=recSolutions[j][1][r].dist;
		recSolutions[j][1][r].Idx=recSolutions[j][2][r].Idx;
		recSolutions[j][1][r].dist=recSolutions[j][2][r].dist;
		recSolutions[j][2][r].Idx=k;
		recSolutions[j][2][r].dist=d;
	}


 }
 /* In recSolutions we have all the Fr_Count-1 best left/right combinations */
 /* Chose the three best (with diff bkp) - start with first place */

	 for (j=1;j<Fr_Count-1;j++){
		 //d_lmin=((recSolutions[j][0][l].dist/(j+1))+(recSolutions[j][0][r].dist/(Fr_Count-(j+1))))/2;
		 d_lmin=(recSolutions[j][0][l].dist+recSolutions[j][0][r].dist)/Fr_Count;
		 for(t=0;t<3;t++){
			 if(d_lmin<recRes[t].dist){
				 for(p=2; p>t;p--){
					 k=recRes[p-1].bkp;
					 d=recRes[p-1].dist;
					 recRes[p].dist= d;
					 recRes[p].bkp=k;
				 }
				 recRes[p].dist= d_lmin;
				 recRes[p].bkp=j;
				 break;
			 }
		 }
	 }
	first=NULL;
	p=recRes[0].bkp;
	left_min=Frag_Seq[recSolutions[p][0][l].Idx];
	right_min=Frag_Seq[recSolutions[p][0][r].Idx];
	if(left_min!=right_min){

		//printf("RefSeq: %s - recdis:%lf-min_dis:%lf\n",seqs[s]->Id,recRes[0].dist,min_seq_dist);
		if(recRes[0].dist<(min_seq_dist-(min_seq_dist*recThresh))){ /* Threshold 0.075/0.12 for 8/4 frags should be input by user*/
		//if(recRes[0].dist<(min_seq_dist-(min_seq_dist*(min_seq_dist-(min_seq_dist*0.1))))) { /* Too many FPs*/
		 /* Fill BKPNode with 1st place info if d< minseq distance! */
			d=recRes[0].dist;
			MALLOC(RecStorage, sizeof(BKPNode));
			 if(align)
				 BKP=(Fr_Count-1-p)*fr_size;
			 else
				 BKP=(p+1)*fr_size;
			RecStorage->BKP=BKP;

			RecStorage->Child_Idx=Frag_Seq[recSolutions[p][0][l].Idx];
			first=RecStorage;
			MALLOC(RecStorage, sizeof(BKPNode));
			RecStorage->BKP=(Fr_Count*fr_size)+999;
			RecStorage->Child_Idx=Frag_Seq[recSolutions[p][0][r].Idx];
			RecStorage->Next=NULL;
			first->Next=RecStorage;

		/* Print 1st place and find 2nd & 3rd place for file printout only*/
			fprintf(fp4,"RefSeq:%s (dist:%lf)\n",seqs[s]->Id,min_seq_dist);
			fprintf(fp4,"%s|%d|%s;%lf\n",seqs[Frag_Seq[recSolutions[p][0][l].Idx]]->Id,BKP,
				seqs[Frag_Seq[recSolutions[p][0][r].Idx]]->Id,d);


				OptionalRecPrintout(fp4,recRes,seqs,Frag_Seq,recSolutions,1,0,1,0,0,p,1,3,BKP,Fr_Count,align,fr_size);



		}
	}


 return(first);

}



/**********************************************************************/
/* 																      */
/*  	Get Sequence of Min Avg. Distance under MinSequence			  */
/*  																  */
/**********************************************************************/
int PickSeqofLargerAvgDis(double ***dist_frags, int k_idx, int f_idx, int min_idx,  int s, int Fr_Count, int k, int f)
{
	double Avg_k=0,Avg_f=0;
	int i,j,a,b;
	j=0;
	for(i=0;i<Fr_Count;i++){
		a=0;b=0;
		if(dist_frags[i][s][k_idx]<=dist_frags[i][s][min_idx])
			a=1;
		if(dist_frags[i][s][f_idx]<=dist_frags[i][s][min_idx])
			b=1;
		if(a!=b)
			break;

		if(a==1 && b==1){
			Avg_k=Avg_k + dist_frags[i][s][k_idx];
			Avg_f=Avg_f + dist_frags[i][s][f_idx];
			j++;
		}
		
	}
	if(a!=b)
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
/**********************************************************************/
/* 						Print Fragments							      */
/**********************************************************************/
void PrintFragments(FILE *fp3, int  *Frag_Seq, Fasta **seqs, double **dist, double ***dist_frags, int Fr_Count, int i, int s, int align )
{
	int t,k;
	fprintf(fp3,"Distance from %s\n",seqs[s]->Id);
	for(k=0;k<i;k++){
		fprintf(fp3,"%s;",seqs[Frag_Seq[k]]->Id);
		if(align){
			for(t=Fr_Count-1;t>=0;t--)
				fprintf(fp3,"%lf;",dist_frags[t][s][Frag_Seq[k]]);
		}
		else{
			for(t=0;t<Fr_Count;t++)
				fprintf(fp3,"%lf;",dist_frags[t][s][Frag_Seq[k]]);
		}
		fprintf(fp3,"%lf\n",dist[s][Frag_Seq[k]]);
	}
}

/**********************************************************************/
/* 																      */
/*  	Check Data for Recombination Signals						  */
/*  																  */
/**********************************************************************/

BKPNode *Check4Recombination(FILE *fp3, FILE *fp4, int  *Frag_Seq, int  *MinCount, Fasta **seqs, double **dist, double ***dist_frags, int StartAncestorSearch, int s, int Fr_Count, double min_d, int min_idx, int fr_size, int align)
{
	int k,t,min_seq, i;
	double d, Cxy,  m_x,m_y,Sx,Sy;
	int   f, remove ;
	double PCCThreshhold;
	
	i=0;
	if(Fr_Count==4)
		PCCThreshhold=0.75;//0.75
	else
		PCCThreshhold=0.675;//0.675;
	for(k=0;k<Fr_Count;k++){/* Loop through Fragment distances */
		d=INF9999;
		t=StartAncestorSearch;
		min_seq=-1;
		for(t;t>=0;t--){
			if(d>=dist_frags[k][s][t]) {//&& (dist_frags[k][s][t]<= min_d || t==min_idx)
				if(d==dist_frags[k][s][t] && min_seq!=min_idx && t!=min_idx && min_seq!=-1){
					remove=PickSeqofLargerAvgDis(dist_frags, min_seq,t,min_idx,s,Fr_Count,-1,-1);
					//printf("RefSeq:%s - keep both seq %s and %s\n  (minSeq is %s)\n",seqs[s]->Id,seqs[min_seq]->Id,seqs[t]->Id,seqs[min_idx]->Id);
				}
				else 
					remove=-2;
				if(remove!=t){
					d=dist_frags[k][s][t];/* only these 2 lines in old code*/
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
	/* Discard sequences that pair up with one another with PCC>=Threshold */

	PrintFragments(fp3,Frag_Seq,seqs, dist, dist_frags, Fr_Count,i, s , align);

	f=0;
	while(f<(i-1)){
		m_x=0;
		for(t=0;t<Fr_Count;t++)/* Sx of the MinAnc Frags */
			m_x = m_x + dist_frags[t][s][Frag_Seq[f]];
		m_x=m_x/Fr_Count;
		Sx=0;
		for(t=0;t<Fr_Count;t++)
			Sx=Sx + pow((dist_frags[t][s][Frag_Seq[f]]-m_x),2);
		Sx=sqrt(Sx);
		k=f+1;
		while(k<i){/* Cxy & Sy of the other Frags */
			m_y=0;
			for(t=0;t<Fr_Count;t++)
				m_y = m_y + dist_frags[t][s][Frag_Seq[k]];
			m_y=m_y/Fr_Count;
			Sy=0;
			for(t=0;t<Fr_Count;t++)
				Sy=Sy+pow((dist_frags[t][s][Frag_Seq[k]]-m_y),2);
			Sy=sqrt(Sy);
			Cxy=0;
			for(t=0;t<Fr_Count;t++)
			Cxy=Cxy+((dist_frags[t][s][Frag_Seq[k]]-m_y)*(dist_frags[t][s][Frag_Seq[f]]-m_x));
			Cxy=Cxy/(Sx*Sy);
			remove=-1;
			//printf("Cxy=%lf Sx=%lf Sy=%lf f=%d i=%d k=%d \n",Cxy,Sx,Sy,f,i, k);
			/* 0.675 for 8 frags, 0.75 for 4 frags */
			if(Cxy>=PCCThreshhold ) {/* Remove seq k or f from minfrag list  */
				//printf("In PCC: RefSeq=%s: remove one of %s and %s\n\n",seqs[s]->Id,seqs[Frag_Seq[k]]->Id,seqs[Frag_Seq[f]]->Id);
				remove=PickSeqofLargerAvgDis(dist_frags, k,f,min_idx,s,Fr_Count,-1,-1);
				if(remove==-1 || k==min_idx || f==min_idx){
					if(dist[s][Frag_Seq[k]]<dist[s][Frag_Seq[f]])/*if dist_k>dist_f or viceversa*/
						remove=f;
					else
						remove=k;
				}
				if(remove<(i-1)) {
					for(t=remove+1;t<i;t++)
						Frag_Seq[t-1]=Frag_Seq[t];
				}
				if(remove==f)k=i; /*end loop*/
				i=i--;
			}
			else /* if(Cxy<threshold)*/
				k++;
		}/* while(k<i) */
		if(remove!=f) f++;
	}/* while(f<i) */

	PrintFragments(fp3,Frag_Seq,seqs, dist, dist_frags, Fr_Count,i, s , align);

	/* Discard sequences that cross minFragSeq twice */
	if (i>1)
		return(GetRecSolutionsII(fp4,dist_frags,seqs,Frag_Seq,i,s,Fr_Count,min_idx,fr_size,dist[s][min_idx],align));
	else
		return(NULL);
}

/**********************************************************************/
/* 																      */
/*  	Save Results in BKP-Link-List before printing it out		  */
/*  																  */
/**********************************************************************/
BKPNode *SaveResultsinBKPLinkList(int  *Frag_Seq, Fasta **seqs, double ***dist_frags,
								  int Fr_Count,int i, int align, int s, int maxdim )
{
int f,k,l,r,t,min_seq,w;
int fr_size;
double d;
BKPNode *RecStorage, *MaxSegment, *first, *loop2;
int oneCrossover;

	fr_size = maxdim/Fr_Count;

	f=-1;
	min_seq=-2;
	for(k=Fr_Count-1,l=0;k>=0,l<Fr_Count;k--,l++){
		d=INF9999;
		if(align)
			r=k;/* left to right direction*/
		else
			r=l;
		for(t=0;t<i;t++){/* find minimum dist */
			if(d>dist_frags[r][s][Frag_Seq[t]]) {
				d=dist_frags[r][s][Frag_Seq[t]];
				w=t;
			}
		}
		if(w!=min_seq &&  min_seq!=-2){/* Change donor only if their distances are different*/
			if(dist_frags[r][s][Frag_Seq[w]]!=dist_frags[r][s][Frag_Seq[min_seq]])
				min_seq=w;
		}
		else
			min_seq=w;
		/* Print to File */
			/* Save in struct */
			if(f!=min_seq){
		
				MALLOC(RecStorage, sizeof(BKPNode));
				RecStorage->BKP=maxdim;
				RecStorage->Next=NULL;
				RecStorage->Child_Idx=Frag_Seq[min_seq];


				if(f==-1){
					first=MaxSegment=RecStorage;
				}
				else{
					MaxSegment->Next=RecStorage;
					if(align)
						MaxSegment->BKP=(Fr_Count-1-r)*fr_size;
					else
						MaxSegment->BKP=r*fr_size;
					MaxSegment=RecStorage;
				}
			}
		

		f=min_seq;
	}
	MaxSegment=first;
	f=0;k=0;l=0;
	/* Find largest segment*/
	oneCrossover=true;
	if(oneCrossover){
		while(MaxSegment!=NULL){
			if(MaxSegment->BKP-f>=k){/* store last longest segment*/
				l=MaxSegment->BKP;
				k=MaxSegment->BKP-f;
			}
			f=MaxSegment->BKP;
			MaxSegment=MaxSegment->Next;
		}
		f=0;
		MaxSegment=first;
		while(MaxSegment!=NULL){
			if(MaxSegment->BKP==l)
				break;
			f=MaxSegment->BKP;
			MaxSegment=MaxSegment->Next;
		}

		if(f<maxdim-MaxSegment->BKP)/* decide if left or right from bkp will belong to other segment */
			MaxSegment=MaxSegment->Next;/* max segment extends to left*/
		
		while(first->Next!=MaxSegment){/* delete everything from left*/
			loop2=first;
			first=first->Next;
			free(loop2);
		}
		MaxSegment->BKP=maxdim;
		RecStorage=MaxSegment;
		RecStorage->Next=NULL;
		/*Delete everything to the right*/
		MaxSegment=MaxSegment->Next;
		while(MaxSegment!=NULL){
			RecStorage=MaxSegment;
			MaxSegment=MaxSegment->Next;
			free(RecStorage);
		}
	}
		





	return (first);
}


/**********************************************************************/
/* 																      */
/*  	Get Minimum Distance = possible Ancestor					  */
/*  																  */
/**********************************************************************/
int GetMinDist(Mintaxa **minresults, Fasta **seqs, double **dist, double ***dist_frags, int maxdim, int n, char *File1, char *File2, double mutrate, int Fr_Count, int *resno, int align)
{
	FILE	*fp1,*fp2, *fp3,*fp4;
	int		i, k, j, ftime,s,t,times,StartAncestorSearch,times_max; 
	Mintaxa *mintaxa[20];
	double	*div, d, d2, min_d,interval, intervalbase;
	int  *Frag_Seq, *MinCount,   min_idx,fr_size;
	BKPNode *bkpLL, *lastbkpLL;

	fr_size = maxdim/Fr_Count;


	
	if((div= (double *) malloc((n+1)* sizeof (double)))==NULL)
		return(0);
	if( Fr_Count>1){
		if((Frag_Seq= (int *) malloc((Fr_Count)* sizeof (int)))==NULL)
			return(0);
		if((MinCount= (int *) malloc((Fr_Count)* sizeof (int)))==NULL)
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
	intervalbase=(mutrate/(double)maxdim);
	printf("maxdim=%d \n",maxdim);
	for(i=0;i<n+1;i++)
		dist[i][i]=0;
	/* printing results to file... */
	printf("printing results to file...\n");
	if ((fp2 = (FILE *) fopen(File2,"w")) == NULL) {
		printf("io2: error when opening SeqDistances.txt files\n");
			return(1);
	}
	if ((fp1 = (FILE *) fopen(File1,"w")) == NULL) {
		printf("io2: error when opening MinDists.txt file\n");
			exit(1);
	}

	  
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
		StartAncestorSearch=t;
		k=s;
		while(seqs[k]->time == seqs[s]->time && k<n)
			k++;
		if(k!=n)k--;
		times=0;min_d=INF9999;
		/* Now get this guy s minimum distance from all others before him starting at t */
		for(t;t>=0;t--){
			d=0;d2=0;
			for(i=0;i<=k;i++)  d +=dist[i][t];
			for(i=0;i<=k;i++) d2 +=dist[i][s];
			div[t] =((n-2)*(dist[s][t])) - d - d2;
			fprintf(fp2,"%s %s %lf %lf \n",seqs[s]->Id, seqs[t]->Id, dist[s][t],div[t]);
			if(min_d>dist[s][t]) {
				min_d=dist[s][t];
				min_idx=t;
			}

		}/* t loop*/
		t=StartAncestorSearch;
		interval=intervalbase * seqs[s]->time;
		for(t;t>=0;t--){
			/* Minimum Distance Calculations: */
			if ((min_d+interval)>=dist[s][t]) {
				if(times>times_max) {
					if ((mintaxa[times] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
					   printf("Error with memory allocation\n");
					   exit(1);
					}
					times_max=times;
				}
				mintaxa[times]->dist=dist[s][t];
				mintaxa[times]->div=div[t];
				mintaxa[times]->Anc_Idx=t;
				if(times<19)times++;
				}

		}/*second t loop */

		
		if (Fr_Count>1) /* Recombination Detection */
			bkpLL = Check4Recombination(fp3,fp4, Frag_Seq,MinCount,seqs,dist,
				dist_frags,StartAncestorSearch,s,Fr_Count, min_d, min_idx,fr_size,align);
		else  /* Not (Fr_Count=1) */
			bkpLL=NULL;
			
		if (bkpLL!=NULL){/*Is recombinant */
			fprintf(fp1,"Recombinant:%s;",seqs[s]->Id);
			//bkpLL=SaveResultsinBKPLinkList(Frag_Seq,seqs,	dist_frags,Fr_Count,i,align,s, maxdim);

			while(bkpLL!=NULL){
				/* Print and free*/
				if(bkpLL->BKP<maxdim)/* last donor segment*/
					fprintf(fp1,"%s|%d|",seqs[bkpLL->Child_Idx]->Id, bkpLL->BKP);
				else
					fprintf(fp1,"%s",seqs[bkpLL->Child_Idx]->Id);

				if(SaveMinResults(minresults, resno, bkpLL->Child_Idx,s,dist[s][bkpLL->Child_Idx])==false) 
					return(false);

				lastbkpLL=bkpLL;
				bkpLL=bkpLL->Next;
				free(lastbkpLL);
			}

			fprintf(fp1,";\n");
		}
		else {
			d=INF9999;j=0;
			/* All Ancestor Candidates With same distance to queried sequence */
			for(i=0;i<times;i++){
				//If(PrintSameDistCandidates)
				//	fprintf(fp1,"%s;%s;%lf;%lf\n",seqs[s]->Id,seqs[mintaxa[i]->Anc_Idx]->Id,mintaxa[i]->dist,mintaxa[i]->div);
				if (fabs(min_d-mintaxa[i]->dist)<= interval)
					if(d>mintaxa[i]->div) {d=mintaxa[i]->div;j=i;}
			}
			/* Print to File */
			//if(times>1 && PrintSameDistCandidates) fprintf(fp1,"The Chosen:%s;%s;%lf;%lf\n\n",seqs[s]->Id,seqs[mintaxa[j]->Anc_Idx]->Id,mintaxa[j]->dist,mintaxa[j]->div);
			//else
				fprintf(fp1,"%s;%s;%lf;%lf\n",seqs[s]->Id,seqs[mintaxa[j]->Anc_Idx]->Id,mintaxa[j]->dist,mintaxa[j]->div);
			/* Save in struct */
			if(SaveMinResults(minresults, resno, mintaxa[j]->Anc_Idx,
				s,mintaxa[j]->dist)==false) 
				return (false);

		}


	 }/*s loop */

/* ExitHere:*/
	if( Fr_Count>1){
		free(Frag_Seq);
		free(MinCount);
		fclose(fp3);
		fclose(fp4);
	}
	fclose(fp2);
	fclose(fp1);
	for(k=0;k<times_max;k++) free(mintaxa[k]);
	free(div);
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
	int r, ch, i,j,a, w;
	int max_w=0;
	int children[NUMSEQS];
	TNode **NodeStorage;
	double **DistMatrix;



	if((DistMatrix = (double **)MatrixInit(sizeof(double), n+1, n+1))==NULL){
		fprintf(stderr,"ERROR with memory allocation!\n");
		exit(EXIT_FAILURE);
	}
	MALLOC(NodeStorage, sizeof(TNode *) * (2*n));


	/* Print Newick Trees to File */
	if ((fp1 = (FILE *) fopen(File1,"a")) == NULL) {
	printf("io2: error when opening txt file\n");
		exit(1);
	}
	/* Sort by ancestor*/
	fprintf(fp1,"\n");

	for(i=0;i<resno;i++)
		heapify_mintaxa(minresults,i);
	heapsort_mintaxa(minresults,resno-1);
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
		if(ch==2)
			fprintf(fp1,"(%s:0.0,%s:%lf);\n\n",seqs[a]->Id,seqs[children[ch-1]]->Id,dist[a][children[ch-1]]);
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

			PrintTree(fp1,NodeStorage[w-1],&seqs[0]);
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
	char	*cc, *filename, *outputfile1, *outputfile2;
	char	temp[LINELIMIT], temp2[MAXLENGHT], timechar[3];
	int		i, k, n,j,Fr_Count, align, res_no =0; 
    Fasta	*seqs[NUMSEQS]; /* Declare an array of Fasta structures */ 
	double	**dist, ***dist_frags;
	double	GOP, GEP, match, mismatch, alpha, mutrate;
	unsigned int	maxdim;
	Mintaxa *minresults[NUMSEQS];


	alpha=0.5;
	GOP=5; //+ log(len2);
	GEP=1;
	match = 2;
	mismatch=0;
	maxdim=0;

	if (argc != 6) { 
	   printf("Usage: [input-file] [output-file] [details-file] [align=0|1] [fragments=1|4|8]\n");
	   exit(1);
	}
	filename = argv[1];//"outNJ4.txt";//
	outputfile1=argv[2];//"m.txt";//
	outputfile2=argv[3];//"d.txt";//
	align=atoi(argv[4]);
	Fr_Count=atoi(argv[5]);/* 4 or 8 for  recombination detection*/
	if(Fr_Count==0){/* Attention: old version required 0*/
		Fr_Count=1;
	}
	if(Fr_Count!=1 && Fr_Count!=4 && Fr_Count!=8){
	   printf("Usage: [input-file] [output-file] [details-file] [align=0|1] [fragments=1|4|8]\n");
	   exit(1);
	}
	mutrate=0;
  
	if ((fp1 = (FILE *) fopen(filename, "r")) == NULL) {
		printf("io2: error when opening file:%s\n",filename);
			return(1);
	}


	/* FIRST read in all sequences */
	n=-1;
	do{
		cc =fgets ((char *) temp, LINELIMIT,fp1) ;
		//printf("temp=%s  \n",temp);

		if (temp[0]=='>'){
			/* A name line */
			if(n>=0) {
				strcpy(seqs[n]->Seq, temp2);
				if(strlen(temp2)>maxdim) maxdim=strlen(temp2);
			}
			n++;
			if ((seqs[n] = ( Fasta *) malloc( sizeof( Fasta)))==NULL) { 
				   printf("Error with memory allocation\n");
				   exit(1);
			}
			k=0;
			i=1;
			timechar[0]=temp[i];
			timechar[1]=temp[i+1];
			timechar[2]=temp[i+2];
			timechar[3]='\0';
			seqs[n]->time = atoi(timechar);
			while (temp[i]!= ' ' && temp[i]!= '\0' && temp[i]!= '\n' && temp[i]!= '\t')
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
			if(temp[0]=='A' || temp[0]=='T' || temp[0]=='C' || temp[0]=='G' || temp[0]=='U'){
				strcat(temp2, temp);
				if(temp2[strlen(temp2)-1]=='\n') temp2[strlen(temp2)-1]='\0'; /* remove line break */
			}
			/* else {  ignore it? } */
		}

	}while (cc != NULL);


	strcpy(seqs[n]->Seq, temp2);
	fclose(fp1);

	for(i=0;i<=n;i++)
		heapify_Fasta(seqs,i);
	heapsort_Fasta(seqs,n);



	// maxdim++;

	if((dist = (double **)MatrixInit(sizeof(double), n+1, n+1))==NULL)
      return(0);


	MALLOC(dist_frags, sizeof(double **) * Fr_Count);
	for (i = 0; i < Fr_Count; i++) {
		MALLOC(dist_frags[i], sizeof(double *) * (n+1));
		for (j=0;j<(n+1);j++)
			MALLOC(dist_frags[i][j], sizeof(double) * (n+1));

	}


	if(align){
		printf("Pairwise alignments... ");
		res_no= Align_Dist(seqs,dist, dist_frags,  maxdim, n, GOP, GEP, match, mismatch, alpha, Fr_Count);
	}
	else{
		printf("Pairwise distances... ");
		res_no=DistOnly(seqs, dist, dist_frags,  maxdim, n, alpha, Fr_Count);
	}
	if(res_no!=true){
			 fprintf(stderr,"ERROR calling Align-Distance function!\n");
			 return(false);
	}
	res_no =0;
	if(GetMinDist(minresults, seqs,dist, 
		dist_frags, maxdim, n,outputfile1,outputfile2,
		mutrate,Fr_Count, &res_no, align)!=true){
		 fprintf(stderr,"ERROR calling GetMinDist function!\n");
		 return(false);
	}
	BuildPartialNJTrees(seqs,minresults, res_no, dist,outputfile1,n);

/* Free up memory */
	for(i=0;i<res_no;i++) free(minresults[i]);

	for(i=0; i<(n+1); i++)   if(dist[i]) free(dist[i]);
	free(dist);

	for(i=0; i<Fr_Count; i++)   {
		for(j=0; j<(n+1); j++) 
			if(dist_frags[i][j]) free(dist_frags[i][j]);
		if(dist_frags[i]) free(dist_frags[i]);
	}
	free(dist_frags);


	for (i=0;i<=n;i++)
		 free(seqs[i]);
	return 0;

}

