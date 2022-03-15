#ifndef _NJ_H
#define _NJ_H

#include "MinPD.h"

typedef struct TNode TNode;
struct TNode {
	TNode *branch1, *branch2;
	double length0;
	int nodeNo;
};

void PrintTree(FILE *fv, TNode *node, Fasta **seqs);
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join);
void NJTree(TNode **NodeStorage, double **DistMatrix, int UBound, int OutgroupIn0, int *seqIds, int *max_w, int *w);

#endif /* _NJ_H */