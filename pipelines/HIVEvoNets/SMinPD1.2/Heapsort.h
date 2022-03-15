#ifndef _HEAPSORT_H
#define _HEAPSORT_H

#include "MinPD.h"

void heapify_mintaxa ( Mintaxa **list , int newnode );
void heapsort_mintaxa ( Mintaxa **list, int last );

void heapify_Fasta ( Fasta **list , int newnode );
void heapsort_Fasta ( Fasta **list, int last );

#endif /* _HEAPSORT_H */
