#include <iostream>
#include <cstdio>
#include <vector>
#include <getopt.h>
#include "io/GLGPU3DDataset.h"
#include "extractor/StochasticExtractor.h"

static std::string filename_in;
static int nogauge = 0,  
           verbose = 0, 
           benchmark = 0, 
           archive = 0,
           gpu = 1,
           nthreads = 0, 
           tet = 1;
static int T0=0, T=1; // start and length of timesteps
static int span=1;

static struct option longopts[] = {
  {"verbose", no_argument, &verbose, 1},  
  {"nogauge", no_argument, &nogauge, 1},
  {"benchmark", no_argument, &benchmark, 1}, 
  {"archive", no_argument, &archive, 1}, 
  {"gpu", no_argument, &gpu, 1}, 
  {"tet", no_argument, &tet, 1},
  {"input", required_argument, 0, 'i'},
  {"output", required_argument, 0, 'o'},
  {"time", required_argument, 0, 't'}, 
  {"length", required_argument, 0, 'l'},
  {"span", required_argument, 0, 's'},
  {"concurrent", required_argument, 0, 'c'},
  {0, 0, 0, 0} 
};

static bool parse_arg(int argc, char **argv)
{
  int c; 

  while (1) {
    int option_index = 0;
    c = getopt_long(argc, argv, "i:t:l:s:c", longopts, &option_index); 
    if (c == -1) break;

    switch (c) {
    case 'i': filename_in = optarg; break;
    case 't': T0 = atoi(optarg); break;
    case 'l': T = atoi(optarg); break;
    case 's': span = atoi(optarg); break;
    case 'c': nthreads = atoi(optarg); break;
    default: break; 
    }
  }

  if (optind < argc) {
    if (filename_in.empty())
      filename_in = argv[optind++]; 
  }

  if (filename_in.empty()) {
    fprintf(stderr, "FATAL: input filename not given.\n"); 
    return false;
  }

  if (verbose) {
    fprintf(stderr, "---- Argument Summary ----\n"); 
    fprintf(stderr, "filename_in=%s\n", filename_in.c_str()); 
    fprintf(stderr, "nogauge=%d\n", nogauge);
    fprintf(stderr, "--------------------------\n"); 
  }

  return true;  
}

static void print_help(int argc, char **argv)
{
  fprintf(stderr, "USAGE:\n");
  fprintf(stderr, "%s -i <input_filename> [-o output_filename] [--nogauge]\n", argv[0]);
  fprintf(stderr, "\n");
  fprintf(stderr, "\t--verbose   verbose output\n"); 
  fprintf(stderr, "\t--benchmark Enable benchmark\n"); 
  fprintf(stderr, "\t--nogauge   Disable gauge transformation\n"); 
  fprintf(stderr, "\n");
}


int main(int argc, char **argv)
{
  if (!parse_arg(argc, argv)) {
    print_help(argc, argv);
    return EXIT_FAILURE;
  }

  GLGPU3DDataset ds;
  ds.OpenDataFile(filename_in);
  // ds.SetPrecomputeSupercurrent(true);
  ds.LoadTimeStep(T0, 0);
  if (tet) ds.SetMeshType(GLGPU3D_MESH_TET);
  else ds.SetMeshType(GLGPU3D_MESH_HEX);
  ds.BuildMeshGraph();
  ds.PrintInfo();
 
  StochasticVortexExtractor ex;
  ex.SetDataset(&ds);
  ex.SetExtentThreshold(1e38);
  ex.SetGPU(true);

  ex.ExtractDeterministicVortices();
  ex.ExtractStochasticVortices();

  return EXIT_SUCCESS; 
}
