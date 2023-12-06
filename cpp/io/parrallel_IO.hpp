#pragma once
#include <mpi.h>
#include <string>
#include <vector>
#include "../core/sparse_mat.hpp"
#include "../net/process_3D_grid.hpp"
#include "CombBLAS/CombBLAS.h"
#include "../core/dense_mat.hpp"

using namespace std;
using namespace distblas::core;
using namespace combblas;
namespace distblas::io {

typedef SpParMat<int64_t, int, SpDCCols<int32_t, int>> PSpMat_s32p64_Int;

/**
 * This class implements IO operations of DistBlas library.
 */
class ParallelIO {
private:

public:

  ParallelIO();
  ~ParallelIO();

  /**
   * Interface for parallel reading of Matrix Market formatted files
   * @param file_path
   */
  template <typename T>
  void parallel_read_MM(string file_path, distblas::core::SpMat<T> *sp_mat, bool copy_col_to_value) {
    MPI_Comm WORLD;
    MPI_Comm_dup(MPI_COMM_WORLD, &WORLD);

    int proc_rank, num_procs;
    MPI_Comm_rank(WORLD, &proc_rank);
    MPI_Comm_size(WORLD, &num_procs);

    shared_ptr<CommGrid> simpleGrid;
    simpleGrid.reset(new CommGrid(WORLD, num_procs, 1));

    unique_ptr<PSpMat_s32p64_Int> G =
        unique_ptr<PSpMat_s32p64_Int>(new PSpMat_s32p64_Int(simpleGrid));

    uint64_t nnz;

    G.get()->ParallelReadMM(file_path, true, maximum<T>());

    nnz = G.get()->getnnz();
    if (proc_rank == 0) {
      cout << "File reader read " << nnz << " nonzeros." << endl;
    }
    SpTuples<int64_t, T> tups(G.get()->seq());
    tuple<int64_t, int64_t, T> *values = tups.tuples;

    vector<Tuple<T>> coords;
    coords.resize(tups.getnnz());

#pragma omp parallel for
    for (int i = 0; i < tups.getnnz(); i++) {
      coords[i].row = get<0>(values[i]);
      coords[i].col = get<1>(values[i]);
      if (copy_col_to_value){
        coords[i].value = get<1>(values[i]);
      }else {
        coords[i].value = get<2>(values[i]);
      }
    }

    int rowIncrement = G->getnrow() / num_procs;


#pragma omp parallel for
    for(int i = 0; i < coords.size(); i++) {
      coords[i].row += rowIncrement * proc_rank;
    }

    sp_mat->coords = coords;
    sp_mat->gRows = G.get()->getnrow();
    sp_mat->gCols = G.get()->getncol();
    sp_mat->gNNz = G.get()->getnnz();

  }

  template <typename T>
  void parallel_write(string file_path, T *nCoordinates, uint64_t rows, uint64_t cols) {
    int proc_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, file_path.c_str(), MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);

    for (uint64_t i = 0; i < rows; ++i) {
       uint64_t   node_id = i + 1+ proc_rank*rows;
       char buffer[1000000];
       int offset = snprintf(buffer, sizeof(buffer), "%d", node_id);
      for (int j = 0; j < cols; ++j) {
        cout<<"nodeId" <<node_id<<" accessing  "<<(i * cols + j)<<"i "<<i<<" j "<<j<<endl;
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " %.5f", nCoordinates[i * cols + j]);
        cout<<"nodeId" <<node_id<<" offset "<<offset<<endl;
      }
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
      MPI_File_write_ordered(fh, buffer, offset, MPI_CHAR, MPI_STATUS_IGNORE);
    }
    MPI_File_close(&fh);
  }

};
} // namespace distblas::io
