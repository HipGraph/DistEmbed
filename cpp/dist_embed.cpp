#include "algo/algo.hpp"
#include "algo/spmm.hpp"
#include "core/common.h"
#include "core/csr_local.hpp"
#include "core/dense_mat.hpp"
#include "core/json.hpp"
#include "core/sparse_mat.hpp"
#include "io/parrallel_IO.hpp"
#include "net/data_comm.hpp"
#include "partition/partitioner.hpp"
#include "core/json.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "algo/spgemm.hpp"

using json = nlohmann::json;

using namespace std;
using namespace distblas::io;
using namespace distblas::partition;
using namespace distblas::net;
using namespace  distblas::core;

int main(int argc, char **argv) {

  const int dimension = 128;

  const int sparse_chunk_size = dimension;

  string input_file = "";
  string output_file = "embedding.txt";
  string data_set_name = "";

  int batch_size = 16384;
  double alpha = 0;
  double beta = 0.25;
  int iterations = 30;
  int ns = 5;
  double lr = 0.02;

  bool spmm = false;

  bool spgemm = false;

  bool col_major = false;
  bool sync_comm  = false;

  bool fix_batch_training = false;

  double density=0.5;

  bool save_results = false;

  string sparse_data_file ="";

  for (int p = 0; p < argc; p++) {
    if (strcmp(argv[p], "-input") == 0) {
      input_file = argv[p + 1];
    } else if (strcmp(argv[p], "-output") == 0) {
      output_file = argv[p + 1];
    } else if (strcmp(argv[p], "-batch") == 0) {
      batch_size = atoi(argv[p + 1]);
    } else if (strcmp(argv[p], "-iter") == 0) {
      iterations = atoi(argv[p + 1]);
    } else if (strcmp(argv[p], "-alpha") == 0) {
      alpha = atof(argv[p + 1]);
    } else if (strcmp(argv[p], "-lr") == 0) {
      lr = atof(argv[p + 1]);
    } else if (strcmp(argv[p], "-nsamples") == 0) {
      ns = atoi(argv[p + 1]);
    } else if (strcmp(argv[p], "-beta") == 0) {
      beta = atof(argv[p + 1]);
    } else if (strcmp(argv[p], "-dataset") == 0) {
      data_set_name = argv[p + 1];
    } else if (strcmp(argv[p], "-col_major") == 0) {
      int val = atoi(argv[p + 1]);
      col_major = (val != 0) ? true : false;
    } else if (strcmp(argv[p], "-sync_comm") == 0) {
      int val = atoi(argv[p + 1]);
      sync_comm = (val != 0) ? true : false;
    } else if (strcmp(argv[p], "-fix_batch_training") == 0) {
      int full_batch_tra = atoi(argv[p + 1]);
      fix_batch_training = full_batch_tra == 1 ? true : false;
    }else if (strcmp(argv[p], "-spmm") == 0) {
      int enable_spmm = atoi(argv[p + 1]);
      spmm = enable_spmm == 1 ? true : false;
    }else if (strcmp(argv[p], "-spgemm") == 0) {
      int enable_spgemm = atoi(argv[p + 1]);
      spgemm = enable_spgemm == 1 ? true : false;
    }else if (strcmp(argv[p], "-density") == 0) {
      density = atof(argv[p + 1]);
    }else if (strcmp(argv[p], "-save_results") == 0) {
      int save_res = atoi(argv[p + 1]);
      save_results = save_res == 1 ? true : false;
    }else if (strcmp(argv[p], "-input_sparse_file") == 0) {
      sparse_data_file = argv[p + 1];
    }
  }

//  }

  MPI_Init(&argc, &argv);
  int rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  if (fix_batch_training) {
    batch_size = batch_size / world_size;
  }


  // Initialize MPI DataTypes

    initialize_mpi_datatypes<int, double, dimension>();


  // Creating reader
  auto reader = unique_ptr<ParallelIO>(new ParallelIO());

  // Creating ProcessorGrid
  auto grid = unique_ptr<Process3DGrid>(new Process3DGrid(world_size, 1, 1, 1));

  auto shared_sparseMat =
      shared_ptr<distblas::core::SpMat<int>>(new distblas::core::SpMat<int>(grid.get()));

  cout << " rank " << rank << " reading data from file path:  " << input_file
       << endl;

  auto start_io = std::chrono::high_resolution_clock::now();

  reader.get()->parallel_read_MM<int>(input_file, shared_sparseMat.get(),true);


  cout << " rank " << rank << " reading data from file path:  " << input_file
       << " completed " << endl;



  auto localBRows = divide_and_round_up(shared_sparseMat.get()->gCols,
                                        grid.get()->col_world_size);
  auto localARows = divide_and_round_up(shared_sparseMat.get()->gRows,
                                        grid.get()->col_world_size);

  // To enable full batch size
  if (spmm or spgemm) {
    batch_size = localARows;
  }

  cout << " rank " << rank << " localBRows  " << localBRows << " localARows "<< localARows << endl;

  vector<Tuple<double>> sparse_coo;
  auto sparse_input = shared_ptr<distblas::core::SpMat<double>>(new distblas::core::SpMat<double>(grid.get()));
  if (spgemm & save_results) {
    reader->build_sparse_random_matrix(localARows, dimension, density, 0,sparse_coo, grid.get());
    uint64_t  gROWs = static_cast<uint64_t>(localARows);
    uint64_t gCols = static_cast<uint64_t>(dimension);
    uint64_t gNNZ =     static_cast<uint64_t>(sparse_coo.size());
    cout<<" rank "<<grid->rank_in_col<<" nnz "<<gNNZ<<endl;
    int localBRows = static_cast<int>(dimension);
    sparse_input =  make_shared<distblas::core::SpMat<double>>(grid.get(),
                                                                   sparse_coo, gROWs,
                                                                   gCols, gNNZ, batch_size,
                                                                   localARows, localBRows, false, false);
  }else if (spgemm){
    reader.get()->parallel_read_MM<double>(sparse_data_file, sparse_input.get(),false);
    sparse_input.get()->batch_size = batch_size;
    sparse_input.get()->proc_row_width = localARows;
    sparse_input.get()->proc_col_width = static_cast<int>(dimension);
  }

  auto end_io = std::chrono::high_resolution_clock::now();

  shared_sparseMat.get()->batch_size = batch_size;
  shared_sparseMat.get()->proc_row_width = localARows;
  shared_sparseMat.get()->proc_col_width = localBRows;

  cout << " rank " << rank << " gROWs  " << shared_sparseMat.get()->gRows
       << "gCols" << shared_sparseMat.get()->gCols << endl;

  vector<Tuple<int>> copiedVector(shared_sparseMat.get()->coords);
  auto shared_sparseMat_sender = make_shared<distblas::core::SpMat<int>>(grid.get(),
      copiedVector, shared_sparseMat.get()->gRows,
      shared_sparseMat.get()->gCols, shared_sparseMat.get()->gNNz, batch_size,
      localARows, localBRows, false, true);

  auto shared_sparseMat_receiver = make_shared<distblas::core::SpMat<int>>(grid.get(),
      copiedVector, shared_sparseMat.get()->gRows,
      shared_sparseMat.get()->gCols, shared_sparseMat.get()->gNNz, batch_size,
      localARows, localBRows, true, false);

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

  if (spgemm){
    sparse_input->initialize_CSR_blocks();
  }

  cout << " rank " << rank << " CSR block initialization completed  " << endl;

//  dense_local->print_cache(i);
//  dense_mat.get()->print_matrix_rowptr(-1);

  if (spmm) {
    auto dense_mat = shared_ptr<DenseMat<int, double, dimension>>(
        new DenseMat<int, double, dimension>(grid.get(), localARows));
    auto dense_mat_output = shared_ptr<DenseMat<int, double, dimension>>(
        new DenseMat<int, double, dimension>(grid.get(), localARows));

    unique_ptr<distblas::algo::SpMMAlgo<int, double, dimension>>

        embedding_algo =
            unique_ptr<distblas::algo::SpMMAlgo<int, double, dimension>>(
                new distblas::algo::SpMMAlgo<int, double, dimension>(
                    shared_sparseMat.get(), shared_sparseMat_receiver.get(),
                    shared_sparseMat_sender.get(), dense_mat.get(),
                    dense_mat_output.get(), grid.get(), alpha, beta, col_major, sync_comm));

    MPI_Barrier(MPI_COMM_WORLD);
    cout << " rank " << rank << " spmm algo started  " << endl;
    embedding_algo.get()->algo_spmm(iterations, batch_size, lr);

  }else if(spgemm){
    auto sparse_out = make_shared<distblas::core::SpMat<double>>(grid.get(),sparse_coo, sparse_input->gRows,
                                                                 sparse_input->gCols, sparse_input->gNNz, batch_size,
                                                                 localARows, localBRows, false, false);

    unique_ptr<distblas::algo::SpGEMMAlgo<int, double, dimension>> spgemm_algo = unique_ptr<distblas::algo::SpGEMMAlgo<int, double, dimension>>(
                new distblas::algo::SpGEMMAlgo<int, double, dimension>(
                    shared_sparseMat.get(), shared_sparseMat_receiver.get(),
                    shared_sparseMat_sender.get(), sparse_input.get(),sparse_out.get(),
                    grid.get(),
                    alpha, beta,col_major,sync_comm));

    MPI_Barrier(MPI_COMM_WORLD);
    cout << " rank " << rank << " spgemm algo started  " << endl;
    spgemm_algo.get()->algo_spgemm(iterations, batch_size,lr);

  }else {
    auto dense_mat = shared_ptr<DenseMat<int, double, dimension>>(
        new DenseMat<int, double, dimension>(grid.get(), localARows));

    unique_ptr<distblas::algo::EmbeddingAlgo<int, double, dimension>>

        embedding_algo =
            unique_ptr<distblas::algo::EmbeddingAlgo<int, double, dimension>>(
                new distblas::algo::EmbeddingAlgo<int, double, dimension>(
                    shared_sparseMat.get(), shared_sparseMat_receiver.get(),
                    shared_sparseMat_sender.get(), dense_mat.get(), grid.get(),
                    alpha, beta, 5, -5,col_major,sync_comm));

    MPI_Barrier(MPI_COMM_WORLD);
    cout << " rank " << rank << " embedding algo started  " << endl;
    embedding_algo.get()->algo_force2_vec_ns(iterations, batch_size, ns, lr);
  }
  cout << " rank " << rank << " algo completed  " << endl;

  ofstream fout;
  fout.open("perf_output", std::ios_base::app);
//
  json j_obj;
  j_obj["alpha"] = alpha;
  j_obj["beta"] = beta;
  j_obj["algo"] = "Embedding";
  j_obj["p"] = world_size;
  j_obj["sparsity"] = density;
  j_obj["data_set"] = data_set_name;
  j_obj["d"] = dimension;
  j_obj["perf_stats"] = json_perf_statistics();
  if (rank == 0) {
    fout << j_obj.dump(4) << "," << endl;
  }
//
  fout.close();
  //
// reader->parallel_write(output_file+"/embedding.txt",dense_mat.get()->nCoordinates,localARows, dimension, grid.get(),shared_sparseMat.get());
 if(spgemm & save_results){
   reader->parallel_write(output_file+"/sparse_local.txt",sparse_coo,grid.get(), localARows,shared_sparseMat.get()->gRows);
 }

  MPI_Finalize();
  return 0;
}
