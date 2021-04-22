//This file contains the code that the master process will execute.

#include <iostream>
#include <mpi.h>
#include <iomanip>

#include "RayTrace.h"
#include "master.h"

//Primatives

void masterSequential(ConfigData* data, float* pixels);
void masterStaticStripsHorizontal(ConfigData* data, float* pixels);
void masterStaticBlock(ConfigData* data, float* pixels);

void masterMain(ConfigData* data)
{
    //Depending on the partitioning scheme, different things will happen.
    //You should have a different function for each of the required 
    //schemes that returns some values that you need to handle.
    
    //Allocate space for the image on the master.
    float* pixels = new float[3 * data->width * data->height];
    for(int i = 0; i < 3 * data->width * data->height; i++)
    {
        pixels[i] = 0;
    }
    
    //Execution time will be defined as how long it takes
    //for the given function to execute based on partitioning
    //type.
    double renderTime = 0.0, startTime, stopTime;

	//Add the required partitioning methods here in the case statement.
	//You do not need to handle all cases; the default will catch any
	//statements that are not specified. This switch/case statement is the
	//only place that you should be adding code in this function. Make sure
	//that you update the header files with the new functions that will be
	//called.
	//It is suggested that you use the same parameters to your functions as shown
	//in the sequential example below.
    switch (data->partitioningMode)
    {
        case PART_MODE_NONE:
            //Call the function that will handle this.
            startTime = MPI_Wtime();
            masterSequential(data, pixels);
            stopTime = MPI_Wtime();
            break;
        case PART_MODE_STATIC_STRIPS_HORIZONTAL:
            //Call the function that will handle this.
            startTime = MPI_Wtime();
            masterStaticStripsHorizontal(data, pixels);
            stopTime = MPI_Wtime();
            break;
        case PART_MODE_STATIC_STRIPS_VERTICAL:
            break;
        case PART_MODE_STATIC_BLOCKS:
            startTime = MPI_Wtime();
            masterStaticBlock(data, pixels);
            stopTime = MPI_Wtime();
        case PART_MODE_STATIC_CYCLES_HORIZONTAL:
            break;
        case PART_MODE_STATIC_CYCLES_VERTICAL:
            break;
        case PART_MODE_DYNAMIC:
            break;
        default:
            std::cout << "This mode (" << data->partitioningMode;
            std::cout << ") is not currently implemented." << std::endl;
            break;
    }

    renderTime = stopTime - startTime;
    std::cout << "Execution Time: " << renderTime << " seconds" << std::endl << std::endl;

    //After this gets done, save the image.
    std::cout << "Image will be save to: ";
    std::string file = generateFileName(data);
    std::cout << file << std::endl;
    savePixels(file, pixels, data);

    //Delete the pixel data.
    delete[] pixels; 
}

void masterSequential(ConfigData* data, float* pixels)
{
    //Start the computation time timer.
    double computationStart = MPI_Wtime();

    //Render the scene.
    for( int i = 0; i < data->height; ++i )
    {
        for( int j = 0; j < data->width; ++j )
        {
            int row = i;
            int column = j;

            //Calculate the index into the array.
            int baseIndex = 3 * ( row * data->width + column );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]),row,j,data);
        }
    }

    //Stop the comp. timer
    double computationStop = MPI_Wtime();
    double computationTime = computationStop - computationStart;

    //After receiving from all processes, the communication time will
    //be obtained.
    double communicationTime = 0.0;

    //Print the times and the c-to-c ratio
	//This section of printing, IN THIS ORDER, needs to be included in all of the
	//functions that you write at the end of the function.
    std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
    //double c2cRatio = communicationTime / computationTime;
    //std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;
}

void masterStaticStripsHorizontal(ConfigData* data, float* pixels)
{
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    float strip_width = width/procs;

    if(strip_width != (int)strip_width)
    {
        std::cout << "Nodes cannot evenly divide image!" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    // clock_t start = clock();

    /* Render the scene. */
    // Iterate over rows for this partition
    for( int col = ( floor(strip_width) * rank ); col < ( ceil(strip_width) * (rank + 1) ); ++col )
    {
        // Iterate over all cols (strips span width)
        for( int row = 0; row < height; ++row )
        {
            //Calculate the index into the array.
            int baseIndex = 3 * ( row * width + col );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]), row, col, data);
        }
    }

    /* Recieve slave process computations */
    for(int p = 1; p < procs; ++p)
    {
        float* newpixels = new float[3 * width * height];

        MPI_Status status;
        MPI_Recv(newpixels, 3 * width * height, MPI_FLOAT, p, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        for( int col = ceil(strip_width) * p; col < floor(strip_width) * (p + 1); ++col )
        {
            for( int row = 0; row < height; ++row )
            {
                int baseIndex = 3 * ( row * width + col );
                memcpy(&(pixels[baseIndex]), &(newpixels[baseIndex]), 3*sizeof(float));
            }
        }

        delete[] newpixels;
    }

    /*
    //Stop the timing.
    clock_t stop = clock();

    //Figure out how much time was taken.
    float time = (float)(stop - start) / (float)CLOCKS_PER_SEC;
    std::cout << "Execution Time: " << time << " seconds" << std::endl << std::endl;
    */
}

void masterStaticBlock(ConfigData* data, float* pixels){
    int height = data->height;
    int width = data->width;
    int rank = data->mpi_rank;
    int procs = data->mpi_procs;
    int factor = sqrt(procs);
    float block_width = width/procs * factor;
    float block_height = height/procs * factor;
    
     if(block_width != (int)block_width || block_height != (int)block_height)
    {
        std::cout << "Nodes cannot evenly divide image!" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    //clock_t start = clock();
    
    /* Render the scene. */
    // Iterate over rows for this partition
    for( int col = ((block_width) * (rank % factor) ); col < ((block_width) * ((rank % factor) + 1)); ++col )
    {
        // Iterate over all cols (strips span width)
        for( int row = ((block_height) * floor(rank/factor) ); row < ((block_height) * (floor(rank/factor) + 1)); ++row )
        {
            //Calculate the index into the array.
            int baseIndex = 3 * ( row * width + col );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]), row, col, data);
        }
    }

    /* Recieve slave process computations */
    for(int p = 1; p < procs; ++p)
    {
        float* newpixels = new float[3 * width * height];

        MPI_Status status;
        MPI_Recv(newpixels, 3 * width * height, MPI_FLOAT, p, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        for( int col = ((block_width) * (p % factor) ); col < ((block_width) * ((p % factor) + 1)); ++col )
        {
            for( int row = ((block_height) * floor(p/factor) ); row < ((block_height) * (floor(p/factor) + 1)); ++row )
            {
                int baseIndex = 3 * ( row * width + col );
                memcpy(&(pixels[baseIndex]), &(newpixels[baseIndex]), 3*sizeof(float));
            }
        }

        delete[] newpixels;
    }
    
}
