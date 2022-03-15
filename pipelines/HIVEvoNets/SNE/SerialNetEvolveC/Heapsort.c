#include "Heapsort.h"
#include <stdlib.h>




/* Convert any random array to heap array*/
void heapify_Nodelist ( Node **list , int newnode )
{
	int done=0, dad;
	Node *temp;
	double nNodeID, dadID;

	dad = ( newnode - 1 ) / 2;

	while( dad >= 0 && !done)
	{
		nNodeID=atof(list[newnode]->ID);
		dadID=atof(list[dad]->ID);
		if(nNodeID> dadID)
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
void heapsort_Nodelist ( Node **list, int last )
{
	int i;
	Node *temp;
	while(last > 0 )
	{
		temp = list[0];
		list[0]=list[last];
		list[last]= temp;
		for(i=0;i<last;i++)
			heapify_Nodelist(list,i);
		last--;
	}
}

