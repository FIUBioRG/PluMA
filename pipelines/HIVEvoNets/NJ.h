#ifndef _NJ_H
#define _NJ_H

#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { printf("error: malloc() "); }

typedef struct TNode TNode;
struct TNode{
	TNode *lchild, *rchild, *parent, *longEdgeChild, *prevGenNode;
	double len2parent, lenLongestEdge;
	int IsPrevGen,seqID;
};

void PrintTree(FILE *fv, TNode *node, char **SeqIdArray);
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join, int ispg);
void NJTree(TNode **NodeStorage, double **DistMatrix, int UBound, int OutgroupIn0, int *seqIds, int *max_w, int *w,int escaped);

#endif /* _NJ_H */