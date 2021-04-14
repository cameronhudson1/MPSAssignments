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
    switch (data->partitioningMode)
    {
        case PART_MODE_NONE:
            //The slave will do nothing since this means sequential operation.
            break;
        case PART_MODE_STATIC_STRIPS_HORIZONTAL:
            //Call the function that will handle this.
            double startTime = MPI_Wtime();
            slaveStaticStripsHorizontal(data, pixels);
            double stopTime = MPI_Wtime();
            break;
        default:
            std::cout << "This mode (" << data->partitioningMode;
            std::cout << ") is not currently implemented." << std::endl;
            break;
    }
}


void slaveStaticStripsHorizontal(ConfigData* data, float* pixels)
{
    
}