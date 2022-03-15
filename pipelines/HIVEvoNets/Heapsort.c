#include "Heapsort.h"
#include <string.h>
#include <stdio.h>

int AlreadySorted(char	**SeqIds, int last)
{
	int i,res;
	res=1;
	for(i=0;i<last-1;i++)
		if(strcmp(SeqIds[i],SeqIds[i+1])>0) {res=0;break;}
		 
	return(res);

}



/* Convert any random array to heap array*/
void heapify_matrix (char	**SeqIds, double **matrix , int newnode, int n )
{
	int done=0, dad,i;
	char *tempID;
	double temp;

	dad = ( newnode - 1 ) / 2;

	while( dad >= 0 && !done)
	{
		if(strcmp(SeqIds[newnode] , SeqIds[dad])>0)
		 {
			   tempID = SeqIds[newnode];
			   SeqIds[newnode]=SeqIds[dad];
			   SeqIds[dad]=tempID;

				for(i=0;i<n;i++){
					if(i!=newnode && i!=dad){
						temp=matrix[newnode][i];
						matrix[newnode][i]=matrix[i][newnode]=matrix[dad][i];
						matrix[dad][i]=matrix[i][dad]=temp;
					}
				}

			   newnode = dad;
			   dad = dad/2;
		 }
		 else
			done = 1;

	}
}
/* Heap sort*/
void heapsort_matrix (char	**SeqIds, double **matrix, int last, int n )
{
	int i;
	double temp;
	char *tempID;

	while(last > 0 )
	{
		tempID = SeqIds[0];
		SeqIds[0]=SeqIds[last];
		SeqIds[last]= tempID;

		for(i=0;i<n;i++){
			if(i!=0 && i!=last){
				temp=matrix[0][i];
				matrix[0][i]=matrix[i][0]=matrix[last][i];
				matrix[last][i]=matrix[i][last]=temp;
			}
		}
		for(i=0;i<last;i++)
			heapify_matrix(SeqIds, matrix,i,n);
		last--;
	}
}

/* Convert any random array to heap array*/
void heapify_mintaxa ( Mintaxa **list , int newnode )
{
	int done=0, dad;
	Mintaxa *temp;

	dad = ( newnode - 1 ) / 2;

	while( dad >= 0 && !done)
	{
		if(list[newnode]->Child_Idx > list[dad]->Child_Idx)
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

