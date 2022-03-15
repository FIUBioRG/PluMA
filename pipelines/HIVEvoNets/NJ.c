#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "NJ.h"

void PrintTree(FILE *fv, TNode *node,char **seqs);
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join, int ispg);
void NJTree(TNode **NodeStorage, double **DistMatrix, int UBound, int OutgroupIn0, int *seqIds, int *max_w, int *w,int escaped);

/**********************************************************************/
/* 																      */
/*  	Add Leaf or Node											  */
/*  																  */
/**********************************************************************/
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join, int ispg)
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
		NewNode->prevGenNode=NULL;
		NewNode->lenLongestEdge=-1;
		NewNode->longEdgeChild=NULL;
		NewNode->IsPrevGen=ispg;
		if(Join>0){
			NewNode->seqID=chidx;
			NewNode->lchild=NodeStorage[*p];
			NewNode->rchild=NodeStorage[Join];
			NewNode->lchild->parent=NewNode;
			NewNode->rchild->parent=NewNode;
			NewNode->len2parent=brlen;
		}
		else{
			NewNode->len2parent=brlen;
			NewNode->lchild=NewNode->rchild=NULL;/*Because it's a tip */
			NewNode->seqID=chidx;
			*p=(*w)-1;
		}
	}
	else{
		for(i=0;i<*w;i++)
			if(NodeStorage[i]->seqID==chidx){
				if(brlen<0 && NodeStorage[i]->lchild->len2parent>fabs(brlen) && NodeStorage[i]->rchild->len2parent>fabs(brlen)){
					NodeStorage[i]->len2parent=0;
					NodeStorage[i]->lchild->len2parent+=brlen;
					NodeStorage[i]->rchild->len2parent+=brlen;
				}
				else
					NodeStorage[i]->len2parent=brlen;
				*p=i;
			}
	}
}

/**********************************************************************/
/* 																      */
/*  	Print Tree													  */
/*  																  */
/**********************************************************************/
void PrintTree(FILE *fv, TNode *node, char **seqs)
{
	if (node->seqID>=1000) {
		fputc('(', fv);				
		PrintTree(fv, node->lchild,&seqs[0]);
		fputc(',', fv);			
		PrintTree(fv, node->rchild,&seqs[0]);
		fputc(')', fv);				
	} 
	else {// tip
		fprintf(fv, "%s", seqs[node->seqID]);
	}
	if (node->len2parent>-1)
		fprintf(fv, ":%.6f", node->len2parent);/* node length !!! */
}


/**********************************************************************/
/* 																      */
/*  	Build NJ Tree													  */
/*  																  */
/**********************************************************************/

void NJTree(TNode **NodeStorage, double **DistMatrix, int UB, int OutgroupIn0, int *seqIds, int *max_w, int *w,int escaped)
{
	int i,j, x,y,p,q, ispg;
	int internalnodes;
	double	*div,d,dv;
	int *pg;/* Is previous generation*/

	MALLOC(div, sizeof(double) * UB);
	/* For RenOgi cutlongbranch code*/
	MALLOC(pg, sizeof(double) * UB);
	for(i=0;i<UB;i++){
		if(i<escaped)
			pg[i]=1;
		else
			pg[i]=0;
	}

	internalnodes=1000;
	*w=0;
	while (UB>2){

//if(UB<=5){
//for(i=0;i<UB;i++)
//	for(j=0;j<UB;j++)
//		printf("DistMatrix[%d][%d]=%lf \n",i,j,DistMatrix[i][j]);
//}

		/*start NJ process */
		for(i=0;i<UB;i++){
			div[i]=0;
			for(j=0;j<UB;j++){
				div[i]+=DistMatrix[i][j];
			}
		}
		d=9999;
		if(OutgroupIn0==1) 
			p=1;
		else 
			p=0;
		for(i=p; i<UB;i++) {/* Careful: start with i=1 when 0 is outgroup*/
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

		if(pg[x]){
			ispg=1;
			pg[x]=0;
		}
		else 
			ispg=0;
		AddTipOrNode(w,&p,max_w, seqIds[x], d,NodeStorage,0,ispg);
		if(pg[y]){
			ispg=1;
			pg[y]=0;
		}
		else 
			ispg=0;
		AddTipOrNode(w,&q,max_w, seqIds[y], dv, NodeStorage,0,ispg);
		AddTipOrNode(w,&p,max_w, internalnodes, -1,NodeStorage,q,0);

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
		for(i=y;i<UB-1;i++){
			seqIds[i]=seqIds[i+1];
			pg[i]=pg[i+1];
		}
		UB--;
		internalnodes++;

	}//while (UB>2)
	/* Only 2 entries in matrix left:0 is outgroup if OutgroupIn0, 1 is tree */
	if(pg[0])ispg=1; else ispg=0;
	AddTipOrNode(w,&p,max_w, seqIds[0], 0,NodeStorage,0,ispg);
	if(pg[1])ispg=1; else ispg=0;
	AddTipOrNode(w,&q,max_w, seqIds[1], DistMatrix[0][1],NodeStorage,0,ispg);
	AddTipOrNode(w,&p,max_w, internalnodes, -1,NodeStorage,q,0);
free(div);
}