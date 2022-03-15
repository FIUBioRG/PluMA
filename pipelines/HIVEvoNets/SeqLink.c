#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "Heapsort.h"
#include "SEQLINK.h"
#include "NJ.h"





//TNode Forest[]; // array of trees 
int treeCount; // number of trees in Forest


void cut(TNode *x,int *f, TNode **Forest)
{
	/* you're here when x->IsPrevGen and one or both 
	NJ_Ids have a prevGenNode.*/
    // delete longest edge and add tree to forest.
	Forest[(*f)++] = x; // create a new tree 
	// NO need to malloc here?

    if(x->parent!=NULL){
		if (x->parent->lchild== x) 
			x->parent->lchild = NULL; // disconnect link to child.
		else 
			x->parent->rchild = NULL;
		x->parent=NULL;
	}
    
}

void Update(TNode *x, TNode *child)
{
	  /* Is there a prev Gen Node in this path? */
	  if(child->prevGenNode!=NULL){
		x->prevGenNode = child->prevGenNode;
		if(child->prevGenNode!=child){
			/* Update lenght if child is not leaf*/
			if(child->len2parent>child->lenLongestEdge ){
				x->lenLongestEdge=child->len2parent;
				x->longEdgeChild = child;
			}
			else{
				x->lenLongestEdge = child->lenLongestEdge;
				x->longEdgeChild = child->longEdgeChild;
			}
		}
		else{
			x->longEdgeChild = NULL;
			x->lenLongestEdge =-1.0;
		}
	  }
	/* else skip as all attributes were initialized to NULL */
}

void visit(TNode *x,int *f, TNode **Forest)
{
	double len;
	TNode *longEdgeNode;
	int left;

	if(x->lchild!=NULL && x->rchild!=NULL){ /* cut right or left */
		if(x->rchild->prevGenNode!=NULL && x->lchild->prevGenNode!=NULL){
			len=x->lchild->len2parent;
			longEdgeNode=x->lchild;
			left=1;
			if(len<x->lchild->lenLongestEdge){
				len=x->lchild->lenLongestEdge;
				longEdgeNode=x->lchild->longEdgeChild;
			}
			if(len<x->rchild->len2parent){
				len=x->rchild->len2parent;
				longEdgeNode=x->rchild;
				left=0;
			}
			if(len<x->rchild->lenLongestEdge){
				len=x->rchild->lenLongestEdge;
				longEdgeNode=x->rchild->longEdgeChild;
				left=0;
			}
			cut(longEdgeNode,f,Forest);
			if(left==1){
				if(x->lchild!=NULL)/* Update will ignore left branch for new prevGenNode*/
					x->lchild->prevGenNode=NULL;
			}
			else{
				if(x->rchild!=NULL)
					x->rchild->prevGenNode=NULL;
			}
		

		}

	}
	/*Update x once there's nothing more to cut*/
	if (x->IsPrevGen){
		x->prevGenNode = x;// self loop
		x->longEdgeChild = NULL;// self loop
		x->lenLongestEdge = -1;
	}else{
		/* Only one of the two NJ_Ids will have prevGenNode!=NULL */
		if(x->lchild!=NULL)
			Update(x,x->lchild);
		if(x->rchild!=NULL)
			Update(x,x->rchild);

	}

}

void postEval(TNode *x,TNode **Forest,int *f) {
	if (x != NULL) {
		postEval(x->lchild,Forest,f);
		postEval(x->rchild,Forest,f);
		visit(x,f,Forest);

	}
}


int GetTime(char * SeqIds){
	int i,j=0,st;
	char  id[20];
	st=strlen(SeqIds);
		for(i=0;i<st;i++){
		if(isdigit(SeqIds[i])) 
			id[j++]=SeqIds[i];
		else
			break;
	}
	return(atoi(id));
}

int LoadNJmatrix(FILE *fp2, double **dist, double **NJdists, int *NJ_Ids, int a,int b, int c,int *ub,  Mintaxa **minresults,int *resno)
{
	int i,j,k,l,p,escaped=0, identsNum,t;
	double d;
	int *Idents, same;
	
	MALLOC(Idents, sizeof(double) * (c-b));
	identsNum=0;

	/* Find escaped variants */
	d=9999.99;
	for(i=a;i<b;i++){
		for(j=b;j<c;j++){
			if(d>dist[i][j] && dist[i][j]!=0){ 
				d=dist[i][j];
			}
			else if(dist[i][j]==0){
				if ((minresults[*resno] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
				   printf("Error with memory allocation\n");
				   return(0);
				}
				minresults[*resno]->Anc_Idx=i;
				minresults[*resno]->Child_Idx=j;
				minresults[*resno]->dist=0.0;
				(*resno)++;

				Idents[identsNum++]=j;
			}
		}
	}
	for(i=a;i<b;i++){
		same=0;
		for(k=0;k<escaped;k++){
			if (dist[NJ_Ids[k]][i]==0){/* Two escaped variants are identical sequences - skip one*/
				same=1;
				break;
			}
				
		}
		if(same==0) {
			for(j=b;j<c;j++){
				if(d==dist[i][j]){
					NJ_Ids[escaped++]=i;
					break;
				}
			}
		}
	}
	p=escaped;
	/* Fill NJmatrix */
	for(i=0;i<escaped;i++){
		/* first the escaped ones */
		for(j=i;j<escaped;j++)
			NJdists[i][j]=NJdists[j][i]=dist[NJ_Ids[i]][NJ_Ids[j]];
		k=escaped;
		for(j=b;j<c;j++){
			for(t=0;t<identsNum;t++){
				if(Idents[t]==j)
					break;
			}
			if(t==identsNum){
				NJdists[i][k]=NJdists[k][i]=dist[NJ_Ids[i]][j];
				k++;
			}
		}

	}
	/* Now the Ti+1 variants */
	l=k=escaped;
	for(i=b;i<c;i++){
		for(t=0;t<identsNum;t++){
			if(Idents[t]==i)
				break;
		}
		if(t==identsNum){

			for(j=i;j<c;j++){
				for(t=0;t<identsNum;t++){
					if(Idents[t]==i)
						break;
				}
				if(t==identsNum){
					NJdists[l][k]=NJdists[k][l]=dist[i][j];
					k++;
				}
			}
			NJ_Ids[p++]=i;
			if(escaped==1){
				if ((minresults[*resno] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
				   printf("Error with memory allocation\n");
				   return(0);
				}
				minresults[*resno]->Anc_Idx=NJ_Ids[0];
				minresults[*resno]->Child_Idx=i;
				minresults[*resno]->dist=dist[0][i];
				(*resno)++;

			}
			l++;
			k=l;
		}
	}
				
		

	*ub=p;

	return(escaped);
}

void PrintAncestors(FILE *fp1,char	**SeqIds,Mintaxa **minresults, int resno)
{
	int i;
	for(i=0;i<resno;i++)
		heapify_mintaxa(minresults,i);
	heapsort_mintaxa(minresults,resno-1);
	for(i=0;i<resno;i++)
			fprintf(fp1,"%s;%s;%lf\n",SeqIds[minresults[i]->Child_Idx],SeqIds[minresults[i]->Anc_Idx],minresults[i]->dist);

}

void StoreAncestorsFromTree( TNode *x,double **dist,int AncID,Mintaxa **minresults, int *resno)
{
	if (x!=NULL){
		if(x->seqID!=AncID && x->seqID<1000){
				if ((minresults[*resno] = (Mintaxa *) malloc( sizeof(Mintaxa)))==NULL){ 
				   printf("Error with memory allocation\n");
				   return;
				}
				minresults[*resno]->Anc_Idx=AncID;
				minresults[*resno]->Child_Idx=x->seqID;
				minresults[*resno]->dist=dist[x->seqID][AncID];
				(*resno)++;
		}
		StoreAncestorsFromTree(x->rchild,dist,AncID,minresults,resno);
		StoreAncestorsFromTree(x->lchild,dist,AncID,minresults,resno);
	}

}


void main(int argc, char* argv[]) {
	FILE	*fp1,*fp2;
	int		n,i,j,k,r,t,a,b,c,ub,max_w,w,f=0, escaped,min_no, SpaceFound;
	char	*filename, * outfile;
	char	ch;
	char	id[20];
	char	**SeqIds;
	double	**dist;
	double	**NJdist;
	int NJ_Ids[NUMSEQS];/* contains id of big matrix dist */
	TNode **NodeStorage;
	TNode **Forest;
	Mintaxa *minresults[NUMSEQS];




	if (argc != 3) { 
	   printf("Usage: [input-file in Phylip format] [output-file] \n");
	   exit(1);
	}
	filename = argv[1];//"outNJ4.txt";//
	outfile = argv[2];


	/* Read in distances */
	if ((fp1 = (FILE *) fopen(filename, "r")) == NULL) {
		printf("io2: error when opening file:%s\n",filename);
			exit(1);
	}
	if ((fp2 = (FILE *) fopen(outfile, "w")) == NULL) {
		printf("io2: error when opening file:%s\n",outfile);
			exit(1);
	}

	n=0;t=0;
	while((ch=getc(fp1)) != EOF){
		if(ch=='\n') break;
		//if (ch!='\t' && t==0) /* Paup matrix has seqIds in first row*/
		//	t=1;
		//else if(ch=='\t' && t==1){
		//	n++;
		//	t=0;
		//}
		if(isdigit(ch)) /* PAML matrix */
			id[t++]=ch;

		 
	}
	//n++;
	id[t]='\0';
	n=atoi(id);

	max_w=0;
	MALLOC(NodeStorage, sizeof(TNode *) * (2*n));
	MALLOC(Forest, sizeof(TNode *) * n);

	MALLOC(SeqIds, sizeof(int *) * n);
	for (i = 0; i < n; i++) 
		MALLOC(SeqIds[i], sizeof(char) * n);


	MALLOC(dist, sizeof(double *) * n);
	for (i = 0; i < n; i++) 
		MALLOC(dist[i], sizeof(double) * n);

	MALLOC(NJdist, sizeof(double *) * n);
	for (i = 0; i < n; i++) 
		MALLOC(NJdist[i], sizeof(double) * n);

SpaceFound=0;

	i=0;j=-1;r=0;k=0;
	while((ch=getc(fp1)) != EOF && i<n){
		if(ch!='\t' && ch!='\n' && ch!=' ') {
				id[r++]=ch;
				SpaceFound=0;
		}
		else if(SpaceFound==0){
			id[r]='\0';
			if(j==-1) /* Read seq name */
				strcpy(SeqIds[k++],id);
			 else 
				dist[i][j]=dist[j][i]=atof(id);
			r=0;
		}
		if(ch=='\n') {
			 /* The diagonal i==j*/
			dist[i][i]=0;
			i++;j=-1;SpaceFound=0;
		}
		else if((ch=='\t' || ch==' ') && SpaceFound==0){ 
				j++;
				SpaceFound=1;
		}

	}
	if ((ch==EOF || i<n) && j>-1 && id[0]!='\0')
			dist[i][j]=dist[j][i]=atof(id);


	if(AlreadySorted(SeqIds, n-1)!=1){
		for(i=0;i<n;i++)
			heapify_matrix(SeqIds,dist,i,n);
		heapsort_matrix(SeqIds,dist,n-1,n);
	}

//for (i = 0; i < n; i++) {
//	printf("%s\t",SeqIds[i]);
//	for (j = 0; j < n; j++) 
//		printf ("\t%lf", dist[i][j]);
//	printf("\n");
//}



	a=0;
	t=GetTime(SeqIds[a]);
	b=a+1;
	while(t==GetTime(SeqIds[b])){
		if(b <n-1) b++; else break;
	}
	t=GetTime(SeqIds[b]);
	c=b+1;
	while(c<n){
		min_no=0;
		while(t==GetTime(SeqIds[c])){
			if(c<n-1) c++; else break;
		}
		if(c<n-1)
			t=GetTime(SeqIds[c]);
		else 
			c=n;

		escaped=LoadNJmatrix(fp2,dist,NJdist,NJ_Ids, a,b,c,&ub, minresults,&min_no);
		if(escaped==1)/* One ancestor for all Ti+1 variants */
			PrintAncestors(fp2,SeqIds,minresults,min_no);
		else{
//for(i=0;i<ub;i++)/*DEBUG LINE*/
//printf("SeqIds[%d]=%s\n", NJ_Ids[i],SeqIds[NJ_Ids[i]]);/*DEBUG LINE*/
			f=0;

			NJTree(NodeStorage, NJdist,ub,0,NJ_Ids, &max_w,&w,escaped);
//fclose(fp1);/*DEBUG LINE*/
//	if ((fp1 = (FILE *) fopen("NJ.tre", "w")) == NULL) {/*DEBUG LINE*/
//		printf("io2: error when opening file:%s\n",outfile);/*DEBUG LINE*/
//			exit(1);/*DEBUG LINE*/
//	}/*DEBUG LINE*/

//PrintTree(fp1, NodeStorage[w-1], SeqIds);/*DEBUG LINE*/
//fprintf(fp1,";\n");/*DEBUG LINE*/
//fclose(fp1);/*DEBUG LINE*/
				
			postEval(NodeStorage[w-1], Forest,&f);
			Forest[f++] = NodeStorage[w-1];/* The tree after long branches have been cut off */
			for(k=0;k<f;k++){
				if(Forest[k]->prevGenNode!=NULL)
					StoreAncestorsFromTree(Forest[k],dist,Forest[k]->prevGenNode->seqID,minresults,&min_no);
			}
			PrintAncestors(fp2,SeqIds,minresults,min_no);
			for(k=0;k<min_no;k++)
				free(minresults[k]);

		}
		a=b;
		b=c;

	}

	fclose(fp1);
	fclose(fp2);

	for (i = 0; i < n; i++) 
		free(SeqIds[i]);
	free(SeqIds);
	for (i = 0; i < n; i++) 
		free(dist[i]);
	free(dist);
	for (i = 0; i < max_w; i++) 
		free(NodeStorage[i]);
	free(NodeStorage);
	free(Forest);
	
}
