#ifndef _SEQLINK_H
#define _SEQLINK_H



#define NUMSEQS 200
#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { printf("error: malloc() "); }

enum  {FALSE, TRUE} Bool;

typedef struct node node;
struct node{
	node *lchild, *rchild, *parent, *longEdgeChild, *prevGenNode;
	double len2parent, lenLongestEdge;
	int IsPrevGen,seqID;
};


#endif /* _SEQLINK_H */