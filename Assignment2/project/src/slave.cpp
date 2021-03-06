//This file contains the code that the master process will execute.

#include <iostream>
#include <mpi.h>
#include "RayTrace.h"
#include "slave.h"

//primatives
void slaveStaticStripsHorizontal(ConfigData* data, float* pixels);
void slaveStaticBlock(ConfigData* data, float* pixels);
void slaveStaticCyclesVertical(ConfigData* data, float* pixels);

void slaveMain(ConfigData* data)
{
    //Depending on the partitioning scheme, different things will happen.
    //You should have a different function for each of the required 
    //schemes that returns some values that you need to handle.
    float* pixels = new float[3 * data->width * data->height + 1];
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
            //double startTime = MPI_Wtime();
            slaveStaticStripsHorizontal(data, pixels);
            //double stopTime = MPI_Wtime();
            break;
        }
        case PART_MODE_STATIC_BLOCKS:
        {
            //Call the function that will handle blocks.
            //double startTime = MPI_Wtime();
            slaveStaticBlock(data, pixels);
            //double stopTime = MPI_Wtime();
            break;
        }
        case PART_MODE_STATIC_CYCLES_VERTICAL:
        {
            slaveStaticCyclesVertical(data, pixels);
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

    // Start computation timer
    double computationStart = MPI_Wtime();

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
    
    // Stop computation timer
    double computationStop = MPI_Wtime();
    float computationTime = (float)computationStop - (float)computationStart;
    
    // Add computation time to the end of the buffer
    pixels[3 * data->width * data->height] = computationTime;
    
    MPI_Send(pixels, 3 * width * height + 1, MPI_FLOAT, 0, MPI_BUFFER_TAG, MPI_COMM_WORLD);
}

void slaveStaticBlock(ConfigData* data, float* pixels){
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    int factor = sqrt(procs);
    float block_width = (width/procs) * factor;
    float block_height = (height/procs) * factor;
    
    // Start computation timer
    double computationStart = MPI_Wtime();
    
    for( int col = (ceil(block_width) * (rank % factor) ); col < (floor(block_width) * ((rank % factor) + 1) ); ++col ){
        // Iterate over all cols (strips span width)
        for( int row = (ceil(block_height) * floor(rank/factor) ); row < (floor(block_height) * (floor(rank/factor) + 1) ); ++row ){
            //Calculate the index into the array.
            int baseIndex = 3 * ( row * width + col );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]), row, col, data);
        }
    }
    
    // Stop computation timer
    double computationStop = MPI_Wtime();
    float computationTime = (float)computationStop - (float)computationStart;
    
    // Add computation time to the end of the buffer
    pixels[3 * data->width * data->height] = computationTime;

    MPI_Send(pixels, 3 * width * height, MPI_FLOAT, 0, MPI_BUFFER_TAG, MPI_COMM_WORLD);
}

void slaveStaticCyclesVertical(ConfigData* data, float* pixels)
{
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    int cycle_height = data->cycleSize;
    
    // Start computation timer
    double computationStart = MPI_Wtime();

    // Process for this node
    for(int part = rank * cycle_height; part < height; part += (procs * cycle_height))
    {
        for(int col = 0; col < width; ++col)
        {
            for(int row = part; row < ((part + cycle_height) >= height ? height : part + cycle_height); ++row)
            {
                //Calculate the index into the array.
                int baseIndex = 3 * ( row * width + col );

                //Call the function to shade the pixel.
                shadePixel(&(pixels[baseIndex]), row, col, data);
            }
        }
    }
    
    // Stop computation timer
    double computationStop = MPI_Wtime();
    float computationTime = (float)computationStop - (float)computationStart;
    
    // Add computation time to the end of the buffer
    pixels[3 * data->width * data->height] = computationTime;

    MPI_Send(pixels, 3 * width * height, MPI_FLOAT, 0, MPI_BUFFER_TAG, MPI_COMM_WORLD);
}
