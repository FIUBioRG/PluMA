#ifndef _HEAPSORT_H
#define _HEAPSORT_H


typedef struct {
     int Anc_Idx;
     int Child_Idx;
	 double dist;
}Mintaxa;

int AlreadySorted(char	**SeqIds, int last);

void heapify_matrix (char	**SeqIds, double **matrix , int newnode, int n );
void heapsort_matrix (char	**SeqIds, double **matrix, int last, int n );
void heapify_mintaxa(Mintaxa **list, int newnode);
void heapsort_mintaxa(Mintaxa **list, int last);

#endif /* _HEAPSORT_H */
