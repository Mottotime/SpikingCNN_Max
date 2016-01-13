/**
 * Document: Manager Compiler Tutorial (maxcompiler-manager-tutorial.pdf)
 * Chapter: 2      Example: 3      Name: Command Stream
 * MaxFile name: CmdStream
 * Summary:
 *     Writes the values for two input streams to LMem, sets up the correct
 *     data length and burst size for the computation and reads and checks the
 *     results from LMem.
 */

/* *
 * 1, Generate and copy a vector to LMEM
 * 2, Generate a index stream
 * 3, Read vector from LMEM and write back to CPU
 * 	  Calculate i + a in parallel
 * 4, Use special vector item, one for a and the other is b, return a + b.
 * 5, Use producer and consumer, give index stream and valid stream.
 * */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "Maxfiles.h"
#include <MaxSLiCInterface.h>

void generateInputData(int row_num, int vectorSize, int sizeBytes, int32_t *inA, int cmd_num, uint32_t *inB)
{
	for (int i = 0; i < row_num; i++) {
		for(int j = 0; j < vectorSize; j++){
			inA[ i * vectorSize + j ] = i;
		}
	}
	for (int i = 0; i < cmd_num; i++ ){
		inB[i] = i;
	}
}

void CmdStreamCPU(int cmd_num, int vectorSize, int32_t *inA, uint32_t *inB, int32_t *outData)
{
	//for (int i = 0; i < size; i++) {
	//	outData[i] = inA[i] + inB[i];
		//outData[i] = inA[ (int)round(inB[i] * 0.5) ];
	//}
	/*
	// get the row
	for (int i = 0; i < cmd_num; i++){
		for(int j = 0; j < vectorSize; j++){
			outData[ i * vectorSize + j ] = inA[ inB[i] * vectorSize + j ];
		}
	}*/
	// get the first element in the row
	for (int i = 0; i < cmd_num; i++){
		outData[ i ] = inA[ inB[i] * vectorSize ];
	}
}

int check(int size, int32_t *outData, int32_t *expected)
{
	int status = 0;
	for (int i = 0; i < size ; i++) {
		if (outData[i] != expected[i]) {
			fprintf(stderr, "[%d] Verification error, out: %u != expected: %u\n",
				i, outData[i], expected[i]);
			status = 1;
		}
	}
	return status;
}

int main()
{
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

	int status = check(cmd_num, outData, expected);
	if (status)
		printf("Test failed.\n");
	else
		printf("Test passed OK!\n");

	return status;
}
