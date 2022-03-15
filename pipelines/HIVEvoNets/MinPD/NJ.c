#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "NJ.h"

void PrintTree(FILE *fv, TNode *node,Fasta **seqs);
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join);
void NJTree(TNode **NodeStorage, double **DistMatrix, int UBound, int OutgroupIn0, int *seqIds, int *max_w, int *w);

/**********************************************************************/
/* 																      */
/*  	Add Leaf or Node											  */
/*  																  */
/**********************************************************************/
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join)
{
	int i;
	TNode *NewNode;


	if(chidx<1000 || Join>0){/*New tip*/
		if (*w>=*max_w){
			MALLOC(NewNode, sizeof(TNode)) ;
			(*max_w)++;
			NodeStorage[(*w)++]=NewNode;
		}
		else
			NewNode=NodeStorage[(*w)++];

		if(Join>0){
			NewNode->nodeNo=chidx;
			NewNode->branch1=NodeStorage[*p];
			NewNode->branch2=NodeStorage[Join];
			NewNode->length0=brlen;
		}
		else{
			NewNode->length0=brlen;
			NewNode->branch1=NewNode->branch2=NULL;/*Because it's a tip */
			NewNode->nodeNo=chidx;
			*p=(*w)-1;
		}
	}
	else{
		for(i=0;i<*w;i++)
			if(NodeStorage[i]->nodeNo==chidx){
				if(brlen<0 && NodeStorage[i]->branch1->length0>fabs(brlen) && NodeStorage[i]->branch2->length0>fabs(brlen)){
					NodeStorage[i]->length0=0;
					NodeStorage[i]->branch1->length0+=brlen;
					NodeStorage[i]->branch2->length0+=brlen;
				}
				else
					NodeStorage[i]->length0=brlen;
				*p=i;
			}
	}
}

/**********************************************************************/
/* 																      */
/*  	Print Tree													  */
/*  																  */
/**********************************************************************/
void PrintTree(FILE *fv, TNode *node,Fasta **seqs)
{
	if (node->nodeNo>=1000) {
		fputc('(', fv);				
		PrintTree(fv, node->branch1,&seqs[0]);
		fputc(',', fv);			
		PrintTree(fv, node->branch2,&seqs[0]);
		fputc(')', fv);				
	} 
	else {// tip
		fprintf(fv, "%s", seqs[node->nodeNo]->Id);
	}
	if (node->length0>-1)
		fprintf(fv, ":%.6f", node->length0);/* node length !!! */
}


/**********************************************************************/
/* 																      */
/*  	Build NJ Tree													  */
/*  																  */
/**********************************************************************/

void NJTree(TNode **NodeStorage, double ** DistMatrix, int UB, int OutgroupIn0, int *seqIds, int *max_w, int *w)
{
	int i,j, x,y,p,q;
	int internalnodes;
	double	*div,d,dv;

	MALLOC(div, sizeof(double) * UB);

	internalnodes=1000;
	*w=0;
	while (UB>2){

		//for(i=0;i<UB;i++)
//	for(j=0;j<UB;j++)
//		printf("DistMatrix[%d][%d]=%lf \n",i,j,DistMatrix[i][j]);


		/*start NJ process */
		for(i=0;i<UB;i++){
			div[i]=0;
			for(j=0;j<UB;j++){
				div[i]+=DistMatrix[i][j];
			}
		}
		d=9999;

		for(i=1; i<UB;i++) {
			for(j=i+1;j<UB;j++){
				dv=((UB-2)*DistMatrix[i][j]) - div[i]-div[j];
				if(d>dv && i!=j ) {
					d=dv;
					x=i;y=j;
				}
			}
		}
		if(x>y) {p=x;x=y;y=p;} //for later
		/* Branch Lengths */
		d=(DistMatrix[x][y]/2) + ((div[x]-div[y])/(double)(2*UB-4));
		dv=DistMatrix[x][y] - d;

		if(dv<0){d+=fabs(dv);dv=0;}
		if(d<0){dv+=fabs(d);d=0;}

		AddTipOrNode(w,&p,max_w, seqIds[x], d,NodeStorage,0);
		AddTipOrNode(w,&q,max_w, seqIds[y], dv, NodeStorage,0);
		AddTipOrNode(w,&p,max_w, internalnodes, -1,NodeStorage,q);

		/* Fill Matrix with new distances to new node */
		for(i=0;i<UB;i++){
			if(i!=x && i!=y)// [x] now represents new node
				DistMatrix[x][i]=DistMatrix[i][x]=(DistMatrix[x][i]+DistMatrix[y][i]-DistMatrix[x][y])/2;
		}
		/* Shrink Matrix by replacing two entries with one node */
		for(i=y;i<UB-1;i++){
			for(j=0;j<UB-1;j++){
				if(j<y)
					DistMatrix[i][j]=DistMatrix[j][i]=DistMatrix[i+1][j];
				else if(j>i)
					DistMatrix[i][j]=DistMatrix[j][i]=DistMatrix[i+1][j+1];
			}
		}
		

		seqIds[x]=internalnodes;
		for(i=y;i<UB-1;i++)
			seqIds[i]=seqIds[i+1];
		UB--;
		internalnodes++;

	}//while (UB>2)
	/* Only 2 entries in matrix left:0 is a, 1 is tree */
	AddTipOrNode(w,&p,max_w, seqIds[0], 0,NodeStorage,0);
	AddTipOrNode(w,&q,max_w, seqIds[1], DistMatrix[0][1],NodeStorage,0);
	AddTipOrNode(w,&p,max_w, internalnodes, -1,NodeStorage,q);
free(div);

}