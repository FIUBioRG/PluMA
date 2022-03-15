#include "Heapsort.h"
#include <string.h>


/* Convert any random array to heap array*/
void heapify_mintaxa ( Mintaxa **list , int newnode )
{
	int done=0, dad;
	Mintaxa *temp;

	dad = ( newnode - 1 ) / 2;

	while( dad >= 0 && !done)
	{
		if(list[newnode]->Anc_Idx > list[dad]->Anc_Idx)
		 {
			   temp = list[newnode];
			   list[newnode]=list[dad];
			   list[dad]=temp;
			   newnode = dad;
			   dad = dad/2;
		 }
		 else
			done = 1;

	}
}
/* Heap sort*/
void heapsort_mintaxa ( Mintaxa **list, int last )
{
	int i;
	Mintaxa *temp;
	while(last > 0 )
	{
		temp = list[0];
		list[0]=list[last];
		list[last]= temp;
		for(i=0;i<last;i++)
			heapify_mintaxa(&list[0],i);
		last--;
	}
}

/*Now with Fasta type arrays */

/* Convert any random array to heap array*/
void heapify_Fasta ( Fasta **list , int newnode )
{
	int done=0, dad;
	Fasta *temp;

	dad = ( newnode - 1 ) / 2;

	while( dad >= 0 && !done)
	{
		if(strcmp(list[newnode]->Id , list[dad]->Id)>0)
		 {
			   temp = list[newnode];
			   list[newnode]=list[dad];
			   list[dad]=temp;
			   newnode = dad;
			   dad = dad/2;
		 }
		 else
			done = 1;

	}
}
/* Heap sort*/
void heapsort_Fasta ( Fasta **list, int last )
{
	int i;
	Fasta *temp;
	while(last > 0 )
	{
		temp = list[0];
		list[0]=list[last];
		list[last]= temp;
		for(i=0;i<last;i++)
			heapify_Fasta(list,i);
		last--;
	}
}

