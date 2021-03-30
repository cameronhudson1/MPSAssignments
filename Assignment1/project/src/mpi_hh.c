/*
  Multiple Processor Systems. Spring 2009 (again, Spring 2010)
  Professor Muhammad Shaaban
  Author: Dmitri Yudanov (update: Dan Brandt)
  
  This is a Hodgkin Huxley (HH) simplified compartamental neuron model 
*/

#include "mpi.h"
/*
  Multiple Processor Systems. Spring 2009 (again, Spring 2010)
  Professor Muhammad Shaaban
  Author: Dmitri Yudanov (update: Dan Brandt)
  
  This is a Hodgkin Huxley (HH) simplified compartamental neuron model 
*/

#include "plot.h"
#include "lib_hh.h"
#include "cmd_args.h"
#include "constants.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// Define macros based on compilation options. This is a best practice that
// ensures that all code is seen by the compiler so there will be no surprises
// when a flag is/isn't defined. Any modern compiler will compile out any
// unreachable code.
#ifdef PLOT_SCREEN
  #define ISDEF_PLOT_SCREEN 1
#else
  #define ISDEF_PLOT_SCREEN 0
#endif

#ifdef PLOT_PNG
  #define ISDEF_PLOT_PNG 1
#else
  #define ISDEF_PLOT_PNG 0
#endif

typedef struct
{
    int dendr;
    int step;
    int num_comps;
    double delta_t; // somaparams[0]
    double v_m; // y[0]
    uint8_t done;
} comm_buffer_t;


/**
 * Name: main
 *
 * Description:
 * See usage statement (run program with '-h' flag).
 *
 * Parameters:
 * @param argc    number of command line arguments
 * @param argv    command line arguments
 */
int main( int argc, char **argv )
{
    // Init Variables
    CmdArgs cmd_args;                       // Command line arguments.
    int num_comps, num_dendrs;              // Simulation parameters.
    struct timeval start, stop, diff;       // Values used to measure time.

    double exec_time;  // How long we take.

    // Accumulators used during dendrite simulation.
    // NOTE: We depend on the compiler to handle the use of double[] variables as
    //       double*.
    double **dendr_volt;
    double res[COMPTIME], y[NUMVAR], y0[NUMVAR], dydt[NUMVAR], soma_params[3];

    // Strings used to store filenames for the graph and data files.
    char time_str[14];
    char graph_fname[ FNAME_LEN ];
    char data_fname[ FNAME_LEN ];

    FILE *data_file;  // The output file where we store the soma potential values.
    FILE *graph_file; // File where graph will be saved.

    PlotInfo pinfo;   // Info passed to the plotting functions.

    //////////////////////////////////////////////////////////////////////////////
    // Parse command line arguments.
    //////////////////////////////////////////////////////////////////////////////

    if (!parseArgs( &cmd_args, argc, argv )) {
    // Something was wrong.
    exit(1);
    }

    // Pull out the parameters so we don't need to type 'cmd_args.' all the time.
    num_dendrs = cmd_args.num_dendrs;
    num_comps  = cmd_args.num_comps;

    printf( "Simulating %d dendrites with %d compartments per dendrite.\n",
          num_dendrs, num_comps );

    //////////////////////////////////////////////////////////////////////////////
    // Create files where results will be stored.
    //////////////////////////////////////////////////////////////////////////////

    // Generate the graph and data file names.
    time_t t = time(NULL);
    struct tm *tmp = localtime( &t );
    strftime( time_str, 14, "%m%d%y_%H%M%S", tmp );

    // The resulting filenames will resemble
    //    pWWdXXcYY_MoDaYe_HoMiSe.xxx
    // where 'WW' is the number of processes, 'XX' is the number of dendrites,
    // 'YY' the number of compartments, and 'MoDaYe...' the time at which this
    // simulation was run.
    sprintf( graph_fname, "graphs/p1d%dc%d_%s.png",
           num_dendrs, num_comps, time_str );
    sprintf( data_fname,  "data/p1d%dc%d_%s.dat",
           num_dendrs, num_comps, time_str );

    // Verify that the graphs/ and data/ directories exist. Create them if they
    // don't.
    struct stat stat_buf;
    stat( "graphs", &stat_buf );
    if ((!S_ISDIR(stat_buf.st_mode)) && (mkdir( "graphs", 0700 ) != 0)) {
    fprintf( stderr, "Could not create 'graphs' directory!\n" );
    exit(1);
    }

    stat( "data", &stat_buf );
    if ((!S_ISDIR(stat_buf.st_mode)) && (mkdir( "data", 0700 ) != 0)) {
    fprintf( stderr, "Could not create 'data' directory!\n" );
    exit(1);
    }

    // Verify that we can open files where results will be stored.
    if ((data_file = fopen(data_fname, "wb")) == NULL) {  
    fprintf(stderr, "Can't open %s file!\n", data_fname);
    exit(1);
    } else {
    printf( "\nData will be stored in %s\n", data_fname );
    }

    if (ISDEF_PLOT_PNG && (graph_file = fopen(graph_fname, "wb")) == NULL) {
    fprintf(stderr, "Can't open %s file!\n", graph_fname);
    exit(1);
    } else {
    printf( "Graph will be stored in %s\n", graph_fname );
    fclose(graph_file);
    }

    //////////////////////////////////////////////////////////////////////////////
    // Main Computation
    //////////////////////////////////////////////////////////////////////////////

    // Initialize MPI and MPI Variables
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if(world_rank == 0)
    {
        // Loop over ms
        for (int t_ms = 1; t_ms < COMPTIME; t_ms++) 
        {
            // Loop over steps
            for (int step = 0; step < STEPS; step++) 
            {
                soma_params[2] = 0.0;

                // Loop over dendrites
                for(int dendrite = 0; dendrite < num_dendrs; dendrite += (world_size - 1))
                {
                    // Put a dendrite on each processing node available
                    for(int node = 1; node < world_size; node++)
                    {
                        comm_buffer_t commbuf;
                        commbuf.dendr = dendrite + node - 1;
                        commbuf.step = step;
                        commbuf.num_comps = num_comps;
                        commbuf.delta_t = soma_params[0];
                        commbuf.v_m = y[0];
                        commbuf.done = 0;
    		            MPI_Send(&commbuf, sizeof(comm_buffer_t), MPI_CHAR, node, MPI_ANY_TAG, MPI_COMM_WORLD);
                    } 

                    // Get all the dendrite processing info back
                    for(int node = 1; node < world_size; node++)
                    {
                        double current_c;
                        MPI_Recv(&current_c, 1, MPI_DOUBLE, node, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        soma_params[2] += current_c;
                    }
                }

                // Tell all processing nodes that work is done
                for(int node = 1; node < world_size; node++)
                {
                    comm_buffer_t commbuf;
                    commbuf.done = 1;
                    MPI_Send(&commbuf, sizeof(comm_buffer_t), MPI_CHAR, node, MPI_ANY_TAG, MPI_COMM_WORLD);
                } 

                // Store previous HH model parameters.
                y0[0] = y[0]; y0[1] = y[1]; y0[2] = y[2]; y0[3] = y[3];

                // This is the main HH computation. It updates the potential, Vm, of the
                // soma, injects current, and calculates action potential. Good stuff.
                soma(dydt, y, soma_params);
                rk4Step(y, y0, dydt, NUMVAR, soma_params, 1, soma);
            }
        }
    }
    else
    {
        comm_buffer_t commbuf;
        MPI_Recv(&commbuf, sizeof(comm_buffer_t), MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        while(commbuf.done == 0)
        {
            // This will update Vm in all compartments and will give a new injected
            // current value from last compartment into the soma.
            double current = dendriteStep( dendr_volt[ commbuf.dendr ],
                                           commbuf.step + commbuf.dendr + 1,
                                           commbuf.num_comps,
                                           commbuf.delta_t,
                                           commbuf.v_m );
 
            // ASend back the current generated by the dendrite.
            MPI_Send(&current, 1, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD);
            MPI_Recv(&commbuf, sizeof(comm_buffer_t), MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    MPI_Finalize();

    //////////////////////////////////////////////////////////////////////////////
    // Report results of computation.
    //////////////////////////////////////////////////////////////////////////////

    // Stop the clock, compute how long the program was running and report that
    // time.
    gettimeofday( &stop, NULL );
    timersub( &stop, &start, &diff );
    exec_time = (double) (diff.tv_sec) + (double) (diff.tv_usec) * 0.000001;
    printf("\n\nExecution time: %f seconds.\n", exec_time);

    // Record the parameters for this simulation as well as data for gnuplot.
    fprintf( data_file,
           "# Vm for HH model. "
           "Simulation time: %d ms, Integration step: %f ms, "
           "Compartments: %d, Dendrites: %d, Execution time: %f s, "
           "Slave processes: %d\n",
           COMPTIME, soma_params[0], num_comps - 2, num_dendrs, exec_time,
           0 );
    fprintf( data_file, "# X Y\n");

    for (t_ms = 0; t_ms < COMPTIME; t_ms++) {
        fprintf(data_file, "%d %f\n", t_ms, res[t_ms]);
    }
    fflush(data_file);  // Flush and close the data file so that gnuplot will
    fclose(data_file);  // see it.

    //////////////////////////////////////////////////////////////////////////////
    // Plot results if approriate macro was defined.
    //////////////////////////////////////////////////////////////////////////////
    if (ISDEF_PLOT_PNG || ISDEF_PLOT_SCREEN) {
    pinfo.sim_time = COMPTIME;
    pinfo.int_step = soma_params[0];
    pinfo.num_comps = num_comps - 2;
    pinfo.num_dendrs = num_dendrs;
    pinfo.exec_time = exec_time;
    pinfo.slaves = 0;
    }

    if (ISDEF_PLOT_PNG) {    plotData( &pinfo, data_fname, graph_fname ); }
    if (ISDEF_PLOT_SCREEN) { plotData( &pinfo, data_fname, NULL ); }

    //////////////////////////////////////////////////////////////////////////////
    // Free up allocated memory.
    //////////////////////////////////////////////////////////////////////////////

    for(int i = 0; i < num_dendrs; i++) {
        free(dendr_volt[i]);
    }
    free(dendr_volt);

    return 0;
}
