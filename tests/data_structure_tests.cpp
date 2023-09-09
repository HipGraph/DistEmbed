#include "../cpp/core/common.h"
#include "../cpp/core/csr_local.hpp"
#include "../cpp/core/dense_mat.hpp"
#include "../cpp/core/sparse_mat.hpp"
#include "../cpp/algo/algo.hpp"
#include "../cpp/io/parrallel_IO.hpp"
#include "../cpp/net/data_comm.hpp"
#include "../cpp/partition/partitioner.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include "../cpp/core/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace distblas::io;
using namespace distblas::partition;
using namespace distblas::net;

int main(int argc, char **argv) {
  string file_path = argv[1];

  cout << " file_path " << file_path << endl;

  int batch_size = 256 ;
   const  int dimension = 128;


  MPI_Init(&argc, &argv);
  int rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
//  batch_size = batch_size/world_size;

  // Initialize MPI DataTypes
  initialize_mpi_datatypes<int, double, dimension>();

  // Creating reader
  auto reader = unique_ptr<ParallelIO>(new ParallelIO());

  // Creating ProcessorGrid
  auto grid = unique_ptr<Process3DGrid>(new Process3DGrid(world_size, 1, 1, 1));

  auto shared_sparseMat =
      shared_ptr<distblas::core::SpMat<int>>(new distblas::core::SpMat<int>());

  cout << " rank " << rank << " reading data from file path:  " << file_path << endl;

  auto start_io = std::chrono::high_resolution_clock::now();
  reader.get()->parallel_read_MM<int>(file_path, shared_sparseMat.get(),true);
  auto end_io = std::chrono::high_resolution_clock::now();

  cout << " rank " << rank << " reading data from file path:  " << file_path
       << " completed " << endl;

  auto localBRows = divide_and_round_up(shared_sparseMat.get()->gCols,
                                        grid.get()->world_size);
  auto localARows = divide_and_round_up(shared_sparseMat.get()->gRows,
                                        grid.get()->world_size);

  //To enable full batch size
  batch_size = localARows;

  cout << " rank " << rank << " localBRows  " << localBRows << " localARows "
       << localARows << endl;

  shared_sparseMat.get()->batch_size = batch_size;
  shared_sparseMat.get()->proc_row_width = localARows;
  shared_sparseMat.get()->proc_col_width = localBRows;


  cout << " rank " << rank << " gROWs  " << shared_sparseMat.get()->gRows
       << "gCols" << shared_sparseMat.get()->gCols << endl;

  vector<Tuple<int>> copiedVector(shared_sparseMat.get()->coords);
  auto shared_sparseMat_sender = make_shared<distblas::core::SpMat<int>>(
      copiedVector, shared_sparseMat.get()->gRows,
      shared_sparseMat.get()->gCols, shared_sparseMat.get()->gNNz, batch_size, localARows, localBRows, false, true);

  auto shared_sparseMat_receiver = make_shared<distblas::core::SpMat<int>>(
      copiedVector, shared_sparseMat.get()->gRows,
      shared_sparseMat.get()->gCols, shared_sparseMat.get()->gNNz, batch_size, localARows, localBRows, true, false);


  auto partitioner = unique_ptr<GlobalAdjacency1DPartitioner>(
      new GlobalAdjacency1DPartitioner(grid.get()));

  cout << " rank " << rank << " partitioning data started  " << endl;



  partitioner.get()->partition_data(shared_sparseMat_sender.get());
  partitioner.get()->partition_data(shared_sparseMat_receiver.get());
  partitioner.get()->partition_data(shared_sparseMat.get());


  cout << " rank " << rank << " partitioning data completed  " << endl;

  shared_sparseMat.get()->initialize_CSR_blocks();
  shared_sparseMat_sender.get()->initialize_CSR_blocks();
  shared_sparseMat_receiver.get()->initialize_CSR_blocks();

  cout << " rank " << rank << " CSR block initialization completed  " << endl;
  auto dense_mat = shared_ptr<DenseMat<int,double, dimension>>(
      new DenseMat<int, double, dimension>(grid.get() ,localARows));

  unique_ptr<distblas::algo::EmbeddingAlgo<int, double, dimension>>
      embedding_algo =
          unique_ptr<distblas::algo::EmbeddingAlgo<int, double, dimension>>(
              new distblas::algo::EmbeddingAlgo<int, double, dimension>(shared_sparseMat.get(),
                                                                     shared_sparseMat_receiver.get(),
                                                                     shared_sparseMat_sender.get(),
                                                                     dense_mat.get(),
                                                                     grid.get(),0.75,0.5,
                                                                     5,
                                                                     -5));

  MPI_Barrier(MPI_COMM_WORLD);
  cout << " rank " << rank << "  algo started  " << endl;
  embedding_algo.get()->algo_force2_vec_ns(30, batch_size, 5, 0.02);
  cout << " rank " << rank << " async completed  " << endl;
//
//
//  cout << " rank " << rank << " training completed  " << endl;
  ofstream fout;
  fout.open("perf_output", std::ios_base::app);

  json j_obj;
  j_obj["perf_stats"] = json_perf_statistics();
  if(rank == 0) {
    fout << j_obj.dump(4) << "," << endl;
  }

  fout.close();
//
//  reader->parallel_write("embedding.txt", dense_mat.get()->nCoordinates,localARows, dimension);
//    dense_mat.get()->print_matrix_rowptr(0);



  MPI_Finalize();
  return 0;
}
