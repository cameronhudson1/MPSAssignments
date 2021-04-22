//This file contains the code that the master process will execute.

#include <iostream>
#include <mpi.h>
#include "RayTrace.h"
#include "slave.h"

//primatives
void slaveStaticStripsHorizontal(ConfigData* data, float* pixels);

void slaveMain(ConfigData* data)
{
    //Depending on the partitioning scheme, different things will happen.
    //You should have a different function for each of the required 
    //schemes that returns some values that you need to handle.
    float* pixels = new float[3 * data->width * data->height];
    for(int i = 0; i < 3 * data->width * data->height; i++)
    {
        pixels[i] = 0;
    }

    switch (data->partitioningMode)
    {
        case PART_MODE_NONE:
            //The slave will do nothing since this means sequential operation.
            break;
        case PART_MODE_STATIC_STRIPS_HORIZONTAL:
        {
            //Call the function that will handle this.
            double startTime = MPI_Wtime();
            slaveStaticStripsHorizontal(data, pixels);
            double stopTime = MPI_Wtime();
            break;
	      }
        default:
        {
            std::cout << "This mode (" << data->partitioningMode;
            std::cout << ") is not currently implemented." << std::endl;
            break;
        }
    }

    delete[] pixels; 
}


void slaveStaticStripsHorizontal(ConfigData* data, float* pixels)
{
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    float strip_width = width/procs;

    // Iterate over rows for this partition
    for( int col = ( ceil(strip_width) * rank ); col < ( floor(strip_width) * (rank + 1) ); ++col )
    {
        // Iterate over all cols (strips span width)
        for( int row = 0; row < data->height; ++row )
        {
            //Calculate the index into the array.
            int baseIndex = 3 * ( row * width + col );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]), row, col, data);
        }
    }
    MPI_Send(pixels, 3 * width * height, MPI_FLOAT, 0, MPI_BUFFER_TAG, MPI_COMM_WORLD);
}

void slaveStaticBlock(ConfigData* data, float* pixels){
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    float block_width = width/procs;
    float block_height = height/procs;
    
    for( int col = ( (width/procs) * rank ); col < ( (width/procs) * (rank + 1) ); ++col ){
        // Iterate over all cols (strips span width)
        for( int row = ( (height/procs) * rank ); row < ( (width/procs) * (rank + 1) ); ++row ){
            //Calculate the index into the array.
            int baseIndex = 3 * ( row * width + col );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]), row, col, data);
        }
    }
    MPI_Send(pixels, 3 * width * height, MPI_FLOAT, 0, MPI_BUFFER_TAG, MPI_COMM_WORLD);
}
