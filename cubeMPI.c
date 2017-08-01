#include <mpi.h>
#include <stdio.h>
#include <math.h> 
#include <stdlib.h>

int calculate_rank ( int* , int );
int* comp_exchange_max (int* ,int* , int );
int* comp_exchange_min (int* ,int* , int );


int main(int argc, char** argv) {
    // Initialize the MPI environment
    MPI_Init(&argc, &argv);
	
    // Get the number of processes
    int np;
    MPI_Comm_size(MPI_COMM_WORLD, &np);

    // Get the rank of the process
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Get the name of the processor
	char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;

	MPI_Status Stat;
	int tagA = 0;
	int tagB = 1;
	int token = 1;
    MPI_Get_processor_name(processor_name, &name_len);

	// create the lable for each processor
	int* label = (int*) malloc(3*sizeof(int)); //000, 001, 010, 011, 100, ...
	int binary = rank ;
	int i = 0 ;
	int j = 0 ;
	int k = 0;
	
	for (i=0;i<3;i++){
		label [2-i] = binary % 2 ;
		binary = binary / 2 ;
	}

    // Print off the labels
    //printf("processor %s, rank %d, out of %d processors, with label %d%d%d\n", processor_name, rank, np, label[0], label[1], label[2]);
	MPI_Barrier(MPI_COMM_WORLD); 
		
		
	// now assign 8 element to each processor
	int* elements = (int*) malloc(8*sizeof(int)); // the elements
	int* exchange =(int*) malloc(8*sizeof(int)); // the result that should exchange with partner
	
	// for convinient we assign the numbers in reverse from 64 to 1
	// the initialization is descending, the output should be ascending
	int startnum = 64 - rank * 8 ;
	for (i=0; i<8; i++){
		elements [i] = startnum;
		exchange [i] = 0 ;
		startnum = startnum - 1 ;
	}

	// locally bobble sort on each processor only on blocks of 8 elements
	int swap = 0 ;
	for (i = 0 ; i < 8 ; i++)
	{
		for (j = i+1 ; j < 8 ; j++)
		{
			if (elements[j] < elements[i]) 
			{
				swap       = elements[i];
				elements[i]   = elements[j];
				elements[j] = swap;
			}
		}
	}

	
	// HERE THE MAIN CODE FOR HYPER CUBE STARTED
	// d = 3 (because np=8, 2^d=8) number of digits we need is 3
	int d = 3;
	for ( i=0 ; i < d ; i++ )
	{
		for (j=i ; j>=0 ; j--)
		{
			//for (k=0;k<=0;k++) 
			{
				int* partner = (int*) malloc(3*sizeof(int));;
				int partner_rank ;
				partner [0] = label [0]; partner [1] = label [1]; partner [2] = label [2];
				if (label [j] == 0) // the TOP processor send and receive
				{
					partner[j] = 1; // the neighbor processor
					partner_rank = calculate_rank (partner,3);
					
					printf ("proc %d and proc %d are paired\n", rank , partner_rank);
					
					MPI_Send(elements, 8 , MPI_INT, partner_rank, tagA, MPI_COMM_WORLD);
					MPI_Recv(exchange, 8 , MPI_INT, partner_rank, tagB , MPI_COMM_WORLD, &Stat);
				
					for (k=0;k<8;k++)
						elements[k]=exchange[k];	

					
				}				
				else // if (label [j] == 0) //the BOTTOM processor receive and send
				{
					partner [j] = 0 ; // the neighbor processor
					partner_rank = calculate_rank (partner,3);
					MPI_Recv(exchange, 8 , MPI_INT, partner_rank, tagA , MPI_COMM_WORLD, &Stat);
		
					int* merge ;
					if (label[i+1] != label[j]) //comp_exchange_max
					//merge = comp_exchange_max (elements,exchange,8);
						merge = comp_exchange_min (elements,exchange,8);
					else //comp_exchange_min
						//merge = comp_exchange_max (elements,exchange,8);
						merge = comp_exchange_min (elements,exchange,8);	
					
					for (k=0; k<8; k++){
						elements [k]= merge[k];
						exchange [k] = merge [k+8];
					}
					
					
					MPI_Send(exchange, 8 , MPI_INT, partner_rank, tagB, MPI_COMM_WORLD);
					
				}
			}			
			MPI_Barrier(MPI_COMM_WORLD); // wait for all processors to finish this level 
		}
		MPI_Barrier(MPI_COMM_WORLD); // wait for all processors to finish this level 
	}
	// HERE THE MAIN CODE FOR HYPER CUBE IS FINISHED

	
	
	MPI_Barrier(MPI_COMM_WORLD); 
	// now print all elements of all processors
	// printing should be serial. so processor 0 will start it
	// and processor 7 will finish it
	if (rank==0)
	{
		printf("output proc %d is finished\n", rank);
		for (i=0; i<8; i++)
			printf ("%d %d\n", rank , elements[i]);
		// send token to next processor to print the result
		MPI_Send(&token, 1 , MPI_INT, rank+1, tagA, MPI_COMM_WORLD);
	}
	else if (rank < (np-1) ) // all other processors but not the last one
	{
		MPI_Recv(&token, 1 , MPI_INT, rank-1, tagA , MPI_COMM_WORLD, &Stat);
		printf("output proc %d is finished\n", rank);
		for (i=0; i<8; i++)
			printf ("%d %d\n", rank, elements[i]);
		// send token to next processor to print the result
		MPI_Send(&token, 1 , MPI_INT, rank+1, tagA, MPI_COMM_WORLD);
	}
	else // the last processor
	{
		MPI_Recv(&token, 1 , MPI_INT, rank-1, tagA , MPI_COMM_WORLD, &Stat);
		printf("\noutput proc %d is finished\n", rank);
		for (i=0; i<8; i++)
			printf ("%d %d\n", rank , elements[i]);		
	}
	
	MPI_Barrier(MPI_COMM_WORLD); 
    // Finalize the MPI environment.
    MPI_Finalize();
	return 0 ;
}

int calculate_rank ( int* label, int size)
{
	int i = 0 ;
	int rank = 0 ;
	for ( i=0; i<size ; i++)
		rank = rank * 2 + label [i];
	return rank;
}

int* comp_exchange_max (int* first,int* sec, int size)
{
	int i =0; // counter on first
	int j =0; // counter on second
	int k = 0; // counter on merge result
	int* merge = (int*) malloc(2*size*sizeof(int)) ; 
	// merge two arrays in one sorted array 
	for (;i<size && j<size;){
		if (first[i]<sec[j]){
			merge[k]=first[i];
			k++;
			i++;
		}
		else{
			merge[k]=sec[j];
			k++;
			j++;		
		}
	}
	if (i<size){
		for (;i<size;i++){
			merge[k]=first[i];
			k++;			
		}
	}
	if (j<size){
		for (;j<size;j++){
			merge[k]=sec[j];
			k++;			
		}
	}

	
	return merge ;
}

int* comp_exchange_min (int* first,int* sec, int size)
{
	int i =0; // counter on first
	int j =0; // counter on second
	int k = 0; // counter on merge result
	int* merge = (int*) malloc(2*size*sizeof(int)); 
	// merge two arrays in one sorted array 
	for (;i<size && j<size;){
		if (first[i]>sec[j]){
			merge[k]=first[i];
			k++;
			i++;
		}
		else{
			merge[k]=sec[j];
			k++;
			j++;		
		}
	}
	if (i<size){
		for (;i<size;i++){
			merge[k]=first[i];
			k++;			
		}
	}
	if (j<size){
		for (;j<size;j++){
			merge[k]=sec[j];
			k++;			
		}
	}
	
	return merge ;	
}
