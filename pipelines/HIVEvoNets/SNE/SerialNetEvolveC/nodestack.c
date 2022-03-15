/* Source module for treevolve */
/* Constant population size    */
/* Population subdivision      */
/* and recombination           */
/* (c) N Grassly 1997          */
/* Dept. Zoology, Oxford.      */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "treevolve.h"
#include "nodestack.h"
#include "mathfuncs.h"

Node *FirstNodePop()
{
Node *child;
int i;

	if(avail==NULL){
		if ( (child=(Node*)malloc(sizeof(Node)))==NULL ) {
			fprintf(stderr, "Out of memory allocating Node (NodePop)\n");
			exit(0);
		}
		if ( (child->ancestral=(short*)malloc((sizeof(short))*numBases))==NULL ) {
			fprintf(stderr, "Out of memory allocating ancestral gamete (IndiPop)\n");
			exit(0);
		}
		if ( (child->sequence=(char*)malloc((sizeof(char))*(numBases+1)))==NULL ) {
			fprintf(stderr, "Out of memory allocating sequence (IndiPop)\n");
			exit(0);
		}
	}else{
		child=avail;
		avail=avail->next;
	}
	for(i=0;i<numBases;i++)
		child->ancestral[i]=1;
	child->next=child;
	child->previous=child;
	child->cutBefore=-1;
	return child;
}
/*---------------------------------------------------------------------------------------*/
Node *NodePop(Node *prev)
{
Node *child;
int i;
	if(avail==NULL){
		if ( (child=(Node*)malloc(sizeof(Node)))==NULL ) {
			fprintf(stderr, "Out of memory allocating Node (NodePop)\n");
			exit(0);
		}
		if ( (child->ancestral=(short*)malloc((sizeof(short))*numBases))==NULL ) {
			fprintf(stderr, "Out of memory allocating ancestral gamete (IndiPop)\n");
			exit(0);
		}
		if ( (child->sequence=(char*)malloc((sizeof(char))*(numBases+1)))==NULL ) {
			fprintf(stderr, "Out of memory allocating sequence (IndiPop)\n");
			exit(0);
		}
	}else{
		child=avail;
		avail=avail->next;
	}

	for(i=0;i<numBases;i++)
		child->ancestral[i]=1;
		
	child->next=prev;
	prev->previous->next=child;
	child->previous=prev->previous;
	prev->previous=child;
	child->cutBefore=-1;
	return child;
}
/*----------------------------------------------------------------------------*/
void Stack(Node *going)
{
	going->next=avail;
	avail=going;
}


