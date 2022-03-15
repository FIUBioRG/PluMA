#ifndef _NJ_H
#define _NJ_H

#include "MinPD.h"

typedef struct TNode TNode;
struct TNode {
	TNode *branch1, *branch2;
	double length0;
	int nodeNo;
};

void PrintTree(FILE *fv, TNode *node, Fasta **seqs,int *boots, int printboot );
void AddTipOrNode(int *w,int *p,int *max_w, int chidx, double brlen,TNode **NodeStorage, int Join);
void NJTree(TNode **NodeStorage, double **DistMatrix, int UBound, int OutgroupIn0, int *seqIds, int *max_w, int *w);
int MatricizeTopology(TNode *node, int *topoMatrix, int n, int **dist, double avgBrLen,int *max);
double GetAvgBrLen(TNode *node, int *nodes);


TNode* Reroot( TNode* node, int id, int Branch_Zero, Fasta** seqs );
int remove_old_root( TNode* root_node, TNode* old_root );
void reverse( TNode* old_root, TNode* node, TNode* old_child, double new_length );
TNode* find_parent( TNode* root_node, TNode* child_node );
TNode* find_node( TNode* node, int id );
void print_nodes( TNode* node, int level, Fasta** seqs );
#endif /* _NJ_H */
