#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "NJ.h"

void PrintTree(FILE *fv, TNode *node,Fasta **seqs,int *boots, int printboot);
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
		if(*w<0||(*p<0&&Join>0))
			ErrorMessageExit("problem 1 in AddTipOrNode\n",1);


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
			NewNode->branch1=NewNode->branch2=NULL; /*Because it's a tip */
			NewNode->nodeNo=chidx;
			*p=(*w)-1;
		if((*p<0))
			ErrorMessageExit("problem 2 in AddTipOrNode\n",1);
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
		if((*p<0))
			ErrorMessageExit("problem 3 in AddTipOrNode\n",1);

	}
}
/**********************************************************************/
/* 	Get avergae tree branch length								      */
/**********************************************************************/
double GetAvgBrLen(TNode *node, int *nodes)
{
	double lenL, lenR;
	int nodesL=0,nodesR=0;
	if(node->branch1==NULL && node->branch2==NULL)/* leaf */
		return(fabs(node->length0));
	else{
		lenL=GetAvgBrLen(node->branch1,&nodesL);
		lenR=GetAvgBrLen(node->branch2,&nodesR);
		if(lenL<0)
			printf("lenL<0\n");
		if(lenR<0)
			printf("lenR<0\n");
		*nodes=nodesL+nodesR+1;
		lenR=(lenR+lenL)/2;
		return(lenR+fabs(node->length0));

	}

}

/**********************************************************************/
/* 																      */
/*		encode the topology of a tree in a distance matrix		*/
/*  																  */
/**********************************************************************/
int MatricizeTopology(TNode *node, int *topoMatrix, int n, int **dist, double avgBrLen,int *max)
{
	int left, right,i,j,brProp,addL=0,addR=0;
	int *rightMatrix;

	if(node->branch1==NULL && node->branch2==NULL)/* leaf */
		return(node->nodeNo);
	else{
		if((rightMatrix= (int *) malloc(n* sizeof (int)))==NULL)
			exit(0);
		for(i=0;i<n;i++)
			rightMatrix[i]=0;
		left = MatricizeTopology(node->branch1,topoMatrix,n,dist,avgBrLen,max);
		right = MatricizeTopology(node->branch2,rightMatrix,n,dist,avgBrLen,max);
		if (avgBrLen>0){
			brProp=(int)(fabs(node->branch1->length0)/avgBrLen);
			//brProp=brProp/4;
			addL=brProp;
			brProp=(int)(fabs(node->branch2->length0)/avgBrLen);
			//brProp=brProp/2;
			addR=brProp;
		//	if(addR>2 || addL>2)
		//		printf("addR=%d, addL=%d\n",addR,addL);
		}

		if(left==-1 || right ==-1){
			if(right!=-1){ /* leaf */
				for(i=0;i<n;i++){
					if(topoMatrix[i]!=0){
						//topoMatrix[i]++;

						dist[i][right]=dist[right][i]=topoMatrix[i]+1+addL+addR;
						if(dist[i][right]>(*max))
							*max=dist[i][right];
						topoMatrix[i]=topoMatrix[i]+1+addL;
						//printf("dist[%d][%d]=%d, ",i,right,dist[i][right]);
					}
				}
				//topoMatrix[right]=1;
				topoMatrix[right]=topoMatrix[right]+1+addR;
			}
			else if (left!=-1){
				for(i=0;i<n;i++){
					if(rightMatrix[i]!=0){
						//rightMatrix[i]++;
						dist[i][left]=dist[left][i]=rightMatrix[i]+1+addR+addL;
						if(dist[i][left]>(*max))
							*max=dist[i][left];
						rightMatrix[i]=rightMatrix[i]+1+addR;
					//printf("dist[%d][%d]=%d, ",i,left,dist[i][left]);
						topoMatrix[i]=rightMatrix[i];
					}
				}
				//topoMatrix[left]=1;
				topoMatrix[left]=topoMatrix[left]+1+addL;

			}
			else{/* both are internal nodes */
				for(i=0;i<n;i++){
					if(topoMatrix[i]!=0 ){
						for(j=0;j<n;j++){
							if( rightMatrix[j]!=0){
								if(dist[i][j]>0)
									ErrorMessageExit("problem in MatricizeTopology:dist[i][j]>0!\n",1);
								dist[i][j]=dist[j][i]=topoMatrix[i]+rightMatrix[j]+1+addR+addL;
								if(dist[i][j]>(*max))
									*max=dist[i][j];
								//printf("dist[%d][%d]=%d, ",i,j,dist[i][j]);
							}
						}
					}
				}
				for(i=0;i<n;i++){
					if(topoMatrix[i]!=0)
						topoMatrix[i]=topoMatrix[i]+1+addL;
					if(rightMatrix[i]!=0)
						topoMatrix[i]=rightMatrix[i]+1+addR;
				}
			}

		}
		else{/* both are leaves */
			topoMatrix[left]=1+addL;
			topoMatrix[right]=1+addR;
			dist[left][right]=dist[right][left]=1+addR+addL;
			//printf("dist[%d][%d]=%d, ",left,right,dist[left][right]);
		}
	//	printf("\n");
	//	for(i=0;i<n;i++){
	//		if(topoMatrix[i]!=-1)
	//			printf("topoMatrix[%d]=%d\n",i,topoMatrix[i]);
	//	}

	//	printf("END OF CALL\n");
		free(rightMatrix);
		return(-1);
	}


}
/**********************************************************************/
/* 																      */
/*  	Print Tree													  */
/*  																  */
/**********************************************************************/
void PrintTree(FILE *fv, TNode *node,Fasta **seqs,int *boots, int printboot)
{
	if (node->nodeNo>=1000) {
		fputc('(', fv);				
		PrintTree(fv, node->branch1,&seqs[0],&boots[0],printboot);
		fputc(',', fv);			
		PrintTree(fv, node->branch2,&seqs[0],&boots[0],printboot);
		fputc(')', fv);				
	} 
	else {// tip
		if(printboot)// new: for bootstrap
			fprintf(fv, "%s[%d%%]", seqs[node->nodeNo]->Id,boots[node->nodeNo]);
		else
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
	int i,j, x,y,p,q,start;
	int internalnodes;
	double	*div,d,dv;

	MALLOC(div, sizeof(double) * UB);

	internalnodes=1000;
	*w=0;
	while (UB>2){

		//for(i=0;i<UB;i++)
//	for(j=0;j<UB;j++)
//		printf("DistMatrix[%d][%d]=%lf \n",i,j,DistMatrix[i][j]);

		if(OutgroupIn0)
			start=1;
		else
			start=0;
		/*start NJ process */
		for(i=0;i<UB;i++){
			div[i]=0;
			for(j=0;j<UB;j++){
				div[i]+=DistMatrix[i][j];
			}
		}
		d=9999;

		for(i=start; i<UB;i++) {
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
		if(p<0)
			ErrorMessageExit("NJTree:problem\n",1);
		AddTipOrNode(w,&q,max_w, seqIds[y], dv, NodeStorage,0);
		if(p<0)
			ErrorMessageExit("NJTree:problem\n",1);
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
