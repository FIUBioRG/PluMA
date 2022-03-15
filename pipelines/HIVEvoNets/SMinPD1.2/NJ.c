#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "NJ.h"

//void PrintTree( FILE *fv, TNode *node, Fasta **seqs, int *boots, int printboot );
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
void PrintTree( FILE *fv, TNode *node, Fasta **seqs, int *boots, int printboot )
{
	if ( node->nodeNo >= 1000 ) {
		fputc( '(', fv );
		if( node->branch1 != NULL ){
			PrintTree( fv, node->branch1,&seqs[0], &boots[0], printboot );
		}
		fputc( ',', fv );
		if( node->branch2 != NULL ){
			PrintTree( fv, node->branch2,&seqs[0], &boots[0], printboot );
		}
		fputc( ')', fv );				
	} 
	else {// tip
		if(printboot)// new: for bootstrap
			fprintf(fv, "%s[%d%%]", seqs[node->nodeNo]->Id,boots[node->nodeNo]);
		else
			fprintf(fv, "%s", seqs[node->nodeNo]->Id);
	}
	if ( node->length0 > -1 )
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


TNode* Reroot(
			TNode* old_root, //old root of the tree
			int id, //id of the new root
			int Branch_Zero, //
			Fasta** seqs //sequences
){
	
	TNode* new_root;
	TNode* parent;
	double old_length;
	
	new_root = find_node( old_root, id );
	if( new_root == NULL ){
		printf( "Error: couldn't find the new root\n" );
	}
	else{
		//printf( "Found the new root with id = %d and length = %f\n", new_root->nodeNo, new_root->length0 );
	}
	
	parent = find_parent( old_root, new_root );
	if( parent == NULL ){
		printf( "Error: could not find parent of new root (%d)\n", new_root->nodeNo );
	}
	else{
		//printf( "Found parent (%d) of new root (%d)\n", parent->nodeNo, new_root->nodeNo );
	}

	/* reverse */
	old_length = new_root->length0;
	reverse( old_root, parent, new_root, old_length );
	new_root->branch1 = parent;
	new_root->branch2 = NULL;
	
	/*remove old root*/
	if( remove_old_root( new_root, old_root ) ){
		//printf( "Error: removing old root\n" );
		exit( 0 );
	}

	new_root->length0 = 0;
	new_root->branch1 = NULL;
	new_root->branch2 = NULL;

	old_root->branch1 = new_root;
	old_root->branch2 = parent;
	if( old_root == parent ){
		old_root->branch2 = NULL;
	}
	return old_root;

}

int remove_old_root( TNode* root_node, TNode* old_root ){
		
		TNode* old_root_child = NULL;
		TNode* old_root_parent;
		TNode** branch;

		old_root_parent = find_parent( root_node, old_root );
		
		if( old_root->branch1 != NULL ){
			old_root_child = old_root->branch1;
		}
		else{
			old_root_child = old_root->branch2;
		}

		if( old_root_parent->branch1 == old_root ){
			branch = &(old_root_parent->branch1);
		}
		else if( old_root_parent->branch2 == old_root ){
			branch = &(old_root_parent->branch2);
		}
		else{
			printf( "Error: could not find which branch of %d leads to %d\n", old_root_parent->nodeNo, old_root->nodeNo );
			return -1;
		}

		*branch = old_root_child;

		old_root_child->length0 += old_root->length0;

		return 0;

}

void reverse( TNode* old_root, TNode* node, TNode* old_child, double new_length ){
	
	TNode* parent;
	TNode** branch;
	double old_length;
	
	//printf( "reversing at %d with with old_child %d...\n", node->nodeNo, old_child->nodeNo );
	
	old_length = node->length0;
	node->length0 = new_length;
	if( node->branch1 == old_child ){
		branch = &(node->branch1);
	}
	else if( node->branch2 == old_child ){
		branch = &(node->branch2);
	}
	else{
		printf( "Error: could not find which branch of %d leads to %d\n", node->nodeNo, old_child->nodeNo );
		exit( 0 );
	}
	parent = find_parent( old_root, node );
	if( parent != NULL ){
		reverse( old_root, parent, node, old_length );
		//printf( "a branch of %d will now point to its parent %d\n", node->nodeNo, parent->nodeNo );
		*branch = parent;
	}
	else{
		//printf( "node %d has no parent, setting it's branch to NULL\n", node->nodeNo );
		*branch = NULL;
	}
	//printf( "done reversing...\n" );
	return;
}
TNode* find_parent( TNode* root_node, TNode* child_node ){
	TNode* temp_node;
	if( root_node == child_node ){
		return NULL;
	}
	//printf( "finding parent of %d in %d\n", child_node->nodeNo, root_node->nodeNo );
	if( root_node->branch1 == child_node ){
		//printf( "found parent of %d in %d (branch 1)\n", child_node->nodeNo, root_node->nodeNo );
		return root_node;
	}
	if( root_node->branch2 == child_node ){
		//printf( "found parent of %d in %d (branch 2)\n", child_node->nodeNo, root_node->nodeNo );
		return root_node;
	}
	if( root_node->branch1 != NULL ){
		temp_node = find_parent( root_node->branch1, child_node );
		if( temp_node != NULL ){
			return temp_node;
		}
	}
	if( root_node->branch2 != NULL ){
		temp_node = find_parent( root_node->branch2, child_node );
		if( temp_node != NULL ){
			return temp_node;
		}
	}
	//printf( "could not find parent of %d\n", child_node->nodeNo );
	return NULL;
}

void print_nodes( TNode* node, int level, Fasta** seqs ){
	int i = 0;
	printf( "\n" );
	printf( "(%d)", level );
	for( i = 0; i < level; i++ ){
		printf( "\t" );
	}
	printf( "%d %f", node->nodeNo, node->length0 );
	printf( "(%d)", node->nodeNo );
	if( node->nodeNo < 20 ){
		printf( "%s", seqs[node->nodeNo]->Id );
	}
	else{
		printf( "." );
	}

	printf( "\n" );
	if( node->branch1 != NULL ){
		print_nodes( node->branch1, level + 1, seqs );
	}
	if( node->branch2 != NULL ){
		print_nodes( node->branch2, level + 1, seqs );
	}
}



TNode* find_node( TNode* node, int id ){
	TNode* found_node = NULL;
	if( node->nodeNo == id ){
		return node;
	}
	if( node->branch1 != NULL ){
		found_node = find_node( node->branch1, id );
		if( found_node != NULL ){
			return found_node;
		}
	}
	if( node->branch2 != NULL ){
		found_node = find_node( node->branch2, id );
		if( found_node != NULL ){
			return found_node;
		}
	}
	return NULL;
}
