#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "Maxfiles.h"
#include <MaxSLiCInterface.h>

#define total_neuron 21962
#define time_step 100
#define image_num 100
#define map_width 28
#define branch_num 128

#define table_size 11744512
#define dfe_table_size 11744640

#define v_threshold 1.0

// too large for stack memory.
double weight_table[table_size];
double address_table[table_size];

double begin_synapse_table[total_neuron+1]; // one empty line for end indexing padding

char customized_table[ dfe_table_size * 2 ];

double v[total_neuron]; // we need to break this into branches, and leave one neuron out?
double array1[total_neuron];
double array2[total_neuron];

double input_x[7840000]; // Too large for main function.
double input_y[100];

int output_spike_CPU[ image_num * time_step * 10 ];
int output_spike_DFE[ image_num * time_step * 10 ];

void Read_weight(char * filename, double * buffer, int expect_size)
// expect_size: expect amount of double, not byte.
{
        FILE * pFile;
        long lSize;
        //double * buffer;
        size_t result;
        int i;

        pFile = fopen ( filename, "rb" );
        if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

        // obtain file size:
        fseek (pFile , 0 , SEEK_END);
        lSize = ftell (pFile);
        rewind (pFile);

        if (lSize/8 != expect_size) {fputs ("Size error",stderr); printf("Actual size %ld; Expected size %d.\n", lSize/8, expect_size); exit (3);}

        // copy the file into the buffer:
        result = fread (buffer,1,lSize,pFile);

        // terminate
        fclose (pFile);
        return;
}

int max(int a, int b){
        if(a>b){
                return a;
        }else{
                return b;
        }
}

void CPUSim(int input_length, double* input_spike, int* output_spike){
        int input_idx = 0;
        int output_idx = 0;
        int n; // neuron_idx;
        double * gather1, * gather2, * temp;
        int i;
        int row, table_idx;
        int output_address;

        gather1 = array1;
        gather2 = array2;

        for( i = 0; i < total_neuron; i++ ){v[i] = 0; gather1[i] = 0; gather2[i] = 0;}

        while(input_idx < input_length){

                if( input_idx % ( time_step * map_width * map_width) == 0 )
                        {printf("image %d; \n", input_idx / time_step / map_width / map_width);}

                for( n = 0; n < total_neuron; n++ ){

                        if( n < map_width * map_width ){ // 0~783, first layer
                                v[n] = input_spike[ input_idx ];
                                input_idx ++;
                        }else{
                                v[n] += gather1[n];
                        }

                        // Here comes the buffer

                        if ( n >= total_neuron - 10 ){
                                if (v[n] >= v_threshold)
                                {
                                        output_spike[output_idx] = 1;
                                }else{
                                        output_spike[output_idx] = 0;
                                }
                                output_idx++;
                        }
                        if( v[n] >= v_threshold ){
                                  v[n] = 0;
                                  for ( row = begin_synapse_table[n];\
                                          row < begin_synapse_table[n+1];\
                                          row ++ ){ // here read in the vectors

                                          for( table_idx = 0; table_idx < branch_num; table_idx++ ){
                                                  output_address = (int)address_table[ (row-1) * branch_num + table_idx ] - 1 ;
                                                  if( output_address != 0){
                                                          gather2[output_address] += weight_table[(row-1) * branch_num + table_idx ];
                                                  }
                                          }
                                  }
                          }
                  }// end for n
                  for( i = 0; i < total_neuron; i++ ){ gather1[i] = 0; }
                  temp = gather1;
                  gather1 = gather2;
                  gather2 = temp;
          }// end while input_idx
}

int main()
{
    int accuracy = 0;

    double temp_output[10];
    double temp_max;
    int check_output[image_num];
    int image, t, i;

    FILE * pFile;

    printf("Begin reading spiking image.\n");
    Read_weight("/home/chuyang/Run1/imdb_test_x_100_new.bin",\
    		input_x, map_width * map_width * time_step * image_num);
    Read_weight("/home/chuyang/Run1/imdb_test_y_100_new.bin",\
    		input_y, image_num);
    printf("Finish reading spiking image.\n");

    printf("Begin reading table.\n");
    Read_weight("/home/chuyang/Run1/weight_table.bin",\
     weight_table, table_size);
    Read_weight("/home/chuyang/Run1/address_table.bin",\
     address_table, table_size);
    Read_weight("/home/chuyang/Run1/begin_synapse_table.bin",\
     begin_synapse_table, total_neuron+1);
    printf("Finish reading table.\n");

    printf("Begin simulation.\n");

    CPUSim(image_num * time_step * map_width * map_width, input_x, output_spike_CPU);

/*
	//const int vectorSize = CmdStream_vectorSize; // Why we can not set this?
	const int vectorSize = 96; // ATTENTION: consist with kernel.
	int sizeBytes = sizeof(int32_t);
	const int row_num = 100;
	const int cmd_num = 40;
	int sizeAInBytes = row_num * vectorSize * sizeBytes;
	int sizeBInBytes = cmd_num * sizeBytes;
	int sizeOutputInBytes = cmd_num * sizeBytes;
	printf("Table A size is %d Bytes.\n ", sizeAInBytes);
	int32_t *inA = malloc( sizeAInBytes ); // the big table
	uint32_t *inB = malloc( sizeBInBytes );

	// for clock
	clock_t start, end;
	double duration;

	generateInputData(row_num, vectorSize, sizeBytes, inA, cmd_num, inB);

	int32_t *expected = malloc(sizeOutputInBytes);
	start = clock();
	CmdStreamCPU(cmd_num, vectorSize, inA, inB, expected);
	end = clock();
	duration = (double)(end - start)/CLOCKS_PER_SEC;
	printf("CPU use time: %.8f\n", duration);

	max_file_t *maxfile = CmdStream_init();
	int burstLengthInBytes = max_get_burst_size(maxfile, "cmd_tolmem");
	printf("MAXFILE cmd_tolmem size: %d \n", burstLengthInBytes);

	//max_actions_t *action = max_action_init(maxfile, NULL);
	//max_set_mem_uint32t(action, "CmdStreamKernel", "inB", 0, inB);

	printf("Loading DFE memory.\n");
	CmdStream_writeLMem(sizeAInBytes, 0, inA);
	//CmdStream_writeLMem(sizeBInBytes, sizeAInBytes, inB);

	int32_t *outData = malloc(sizeOutputInBytes);
	printf("Running DFE.\n");
	start = clock();
	//CmdStream(cmd_num, burstLengthInBytes, row_num, inB, outData);
	//CmdStream(cmd_num, inB, outData);
	CmdStream(cmd_num, outData);
	end = clock();
	duration = (double)(end - start)/CLOCKS_PER_SEC;
	printf("DFE use time: %.8f\n", duration);

	/*
	printf("Reading DFE memory.\n");
	int32_t *outData = malloc(sizeOutputInBytes);
	CmdStream_readLMem(sizeOutputInBytes, sizeAInBytes, outData);
	*/
/*
	int status = check(cmd_num, outData, expected);
	if (status)
		printf("Test failed.\n");
	else
		printf("Test passed OK!\n");

	return status;
*/
	return 0;
}
