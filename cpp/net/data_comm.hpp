#pragma once
#include "../core/dense_mat.hpp"
#include "../core/sparse_mat.hpp"
#include "process_3D_grid.hpp"
#include <iostream>
#include <mpi.h>
#include <vector>

using namespace distblas::core;

namespace distblas::net {

/**
 * This class represents the data transfer related operations across processes
 * based on internal data connectivity patterns.
 */

template <typename SPT, typename DENT, size_t embedding_dim> class DataComm {

private:
  distblas::core::SpMat<SPT> *sp_local;
  distblas::core::SpMat<SPT> *sp_local_trans;
  distblas::core::DenseMat<DENT, embedding_dim> *dense_local;
  Process3DGrid *grid;
  vector<int> sdispls;
  vector<int> sendcounts;
  vector<int> rdispls;
  vector<int> receivecounts;
  DataTuple<DENT, embedding_dim> *sendbuf;

  //  DataTuple<DENT, embedding_dim> *receivebuf;

public:
  DataComm(distblas::core::SpMat<SPT> *sp_local,
           distblas::core::SpMat<SPT> *sp_local_trans,
           DenseMat<DENT, embedding_dim> *dense_local, Process3DGrid *grid) {
    this->sp_local = sp_local;
    this->sp_local_trans = sp_local_trans;
    this->dense_local = dense_local;
    this->grid = grid;
    this->sdispls = vector<int>(grid->world_size, 0);
    this->sendcounts = vector<int>(grid->world_size, 0);
    this->rdispls = vector<int>(grid->world_size, 0);
    this->receivecounts = vector<int>(grid->world_size, 0);
  }

  ~DataComm() {
    //    if (receivebuf != nullptr) {
    //      delete[] receivebuf;
    //    }
    //    cout << "successfully executed" << endl;
    //    delete[] sendbuf;
  }

  void async_transfer(int batch_id, bool verify,
                      std::vector<DataTuple<DENT, embedding_dim>> *receivebuf,
                      MPI_Request &request) {
    vector<vector<uint64_t>> receive_col_ids_list(grid->world_size);
    vector<vector<uint64_t>> send_col_ids_list(grid->world_size);

    int total_send_count = 0;
    int total_receive_count = 0;

    // processing chunks
    // calculating receiving data cols
    this->sp_local->fill_col_ids(batch_id, receive_col_ids_list);

    // calculating sending data cols
    vector<uint64_t> col_receiv_ids;
    this->sp_local_trans->fill_col_ids(batch_id, send_col_ids_list);




//
//    for (int i = 0; i < grid->world_size; i++) {
//      std::unordered_set<uint64_t> unique_set_receiv(
//          receive_col_ids_list[i].begin(), receive_col_ids_list[i].end());
//      receive_col_ids_list[i] =
//          vector<uint64_t>(unique_set_receiv.begin(), unique_set_receiv.end());
//
//      receivecounts[i] = receive_col_ids_list[i].size();
//
//      std::unordered_set<uint64_t> unique_set_send(send_col_ids_list[i].begin(),
//                                                   send_col_ids_list[i].end());
//      send_col_ids_list[i] =
//          vector<uint64_t>(unique_set_send.begin(), unique_set_send.end());
//
//      sendcounts[i] = send_col_ids_list[i].size();
//
//      sdispls[i] = (i > 0) ? sdispls[i - 1] + sendcounts[i - 1] : sdispls[i];
//      rdispls[i] = (i > 0) ? rdispls[i - 1] + receivecounts[i - 1] : rdispls[i];
//
//      total_send_count = total_send_count + sendcounts[i];
//      total_receive_count = total_receive_count + receivecounts[i];
//
//    }
//
//
//    sendbuf = new DataTuple<DENT, embedding_dim>[total_send_count];
//
//    receivebuf->resize(total_receive_count);
//    DataTuple<DENT, embedding_dim> *receivebufverify;
//
//    if (verify) {
//      receivebufverify =
//          new DataTuple<DENT, embedding_dim>[total_receive_count];
//    }
//
//    for (int i = 0; i < grid->world_size; i++) {
////#pragma omp parallel
//      for (int j = 0; j < send_col_ids_list[i].size(); j++) {
//        int index = sdispls[i] + j;
//        uint64_t local_key = send_col_ids_list[i][j];
//        sendbuf[index].col = local_key+(this->sp_local->proc_row_width*this->grid->global_rank);
//        sendbuf[index].value = (this->dense_local)->fetch_local_data(local_key);
//      }
//
//      if (verify) {
//        for (int j = 0; j < receive_col_ids_list[i].size(); j++) {
//          int index = rdispls[i] + j;
//          receivebufverify[index].col = receive_col_ids_list[i][j];
//        }
//      }
//    }
//
//    MPI_Ialltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
//                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
//                   DENSETUPLE, MPI_COMM_WORLD, &request);
//
////    MPI_Alltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
////                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
////                   DENSETUPLE, MPI_COMM_WORLD);
//
//    if (verify) {
//      MPI_Status status;
//      MPI_Wait(&request, &status);
//
//      for (int i = 0; i < grid->world_size; i++) {
//        int base_index = rdispls[i];
//        int size = receivecounts[i];
//        for (int k = 0; k < size; k++) {
//          int index = rdispls[i] + k;
//          bool matched = false;
//          for (int m = rdispls[i]; m < rdispls[i] + receivecounts[i]; m++) {
//            if (receivebufverify[m].col == (*receivebuf)[index].col) {
//              matched = true;
//            }
//          }
//          if (!matched) {
//            cout << " rank " << grid->global_rank << "cannot verify value"
//                 << (*receivebuf)[index].col << endl;
//          }
//        }
//      }
//      delete[] receivebufverify;
//    }
  }

  void async_transfer(vector<uint64_t> &col_ids, bool verify,
                      std::vector<DataTuple<DENT, embedding_dim>> *receivebuf,
                      MPI_Request &request) {

    vector<vector<uint64_t>> receive_col_ids_list(grid->world_size);
    vector<uint64_t> send_col_ids_list;

    int total_send_count = 0;
    int total_receive_count = 0;

    for (int i = 0; i < col_ids.size(); i++) {
      int owner_rank = col_ids[i] / (this->sp_local)->proc_row_width;
      if (owner_rank == grid->global_rank) {
        send_col_ids_list.push_back(col_ids[i]);
      } else {
        receive_col_ids_list[owner_rank].push_back(col_ids[i]);
      }
    }

    for (int i = 0; i < grid->world_size; i++) {
      int send_size = send_col_ids_list.size();
      if (i != grid->global_rank) {
        sendcounts[i] = send_size;
      } else {
        sendcounts[i] = 0;
      }
      receivecounts[i] = receive_col_ids_list[i].size();
    }
    sdispls[0] = 0;
    rdispls[0] = 0;
    for (int i = 0; i < grid->world_size; i++) {

      sdispls[i] = 0;
      rdispls[i] = (i > 0) ? rdispls[i - 1] + receivecounts[i - 1] : rdispls[i];

      total_send_count = total_send_count + sendcounts[i];
      total_receive_count = total_receive_count + receivecounts[i];
    }

    DataTuple<DENT, embedding_dim> *sendbuf =
        new DataTuple<DENT, embedding_dim>[total_send_count];
    //     vector<DataTuple<DENT, embedding_dim>>  *sendbuf =
    //         new vector<DataTuple<DENT, embedding_dim>>(total_send_count);

    receivebuf->resize(total_receive_count);

    DataTuple<DENT, embedding_dim> *receivebufverify;

    if (verify) {
      receivebufverify =
          new DataTuple<DENT, embedding_dim>[total_receive_count];
    }

    for (int i = 0; i < grid->world_size; i++) {
      for (int j = 0; j < send_col_ids_list.size(); j++) {
        int index = sdispls[i] + j;
        ((sendbuf)[index]).col = send_col_ids_list[j];
        int local_key = ((sendbuf)[index]).col -
                        (grid->global_rank) * (this->sp_local)->proc_row_width;
        sendbuf[index].value = (this->dense_local)->fetch_local_data(local_key);
      }

      if (verify) {
        for (int j = 0; j < receive_col_ids_list[i].size(); j++) {
          int index = rdispls[i] + j;
          receivebufverify[index].col = receive_col_ids_list[i][j];
        }
      }
    }

    MPI_Ialltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
                   DENSETUPLE, MPI_COMM_WORLD, &request);
//    MPI_Alltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
//                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
//                   DENSETUPLE, MPI_COMM_WORLD);

    if (verify) {
      MPI_Status status;
      MPI_Wait(&request, &status);

      for (int i = 0; i < grid->world_size; i++) {
        int base_index = rdispls[i];
        int size = receivecounts[i];
        for (int k = 0; k < size; k++) {
          int index = rdispls[i] + k;
          bool matched = false;
          for (int m = rdispls[i]; m < rdispls[i] + receivecounts[i]; m++) {
            if (receivebufverify[m].col == (*receivebuf)[index].col) {
              matched = true;
            }
          }
          if (!matched) {
            cout << " rank " << grid->global_rank << "cannot verify value "
                 << (*receivebuf)[index].col << endl;
          }
        }
      }
      delete[] receivebufverify;
    }
  }

  void
  async_re_transfer(std::vector<DataTuple<DENT, embedding_dim>> *receivebuf,
                    MPI_Request &request) {
    int total_receive_count = 0;
    for (int i = 0; i < grid->world_size; i++) {
      total_receive_count += receivecounts[i];
      int sendcount = sendcounts[i];
      int offset = sdispls[i];

      for (int k = 0; k < sendcount; k++) {
        int index = offset + k;
        int local_key = ((sendbuf)[index]).col -
                        (grid->global_rank) * (this->sp_local)->proc_row_width;
        sendbuf[index].value = (this->dense_local)->fetch_local_data(local_key);
      }
    }
    receivebuf->resize(total_receive_count);
    MPI_Ialltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
                   DENSETUPLE, MPI_COMM_WORLD, &request);
//    MPI_Alltoallv(sendbuf, sendcounts.data(), sdispls.data(), DENSETUPLE,
//                   (*receivebuf).data(), receivecounts.data(), rdispls.data(),
//                   DENSETUPLE, MPI_COMM_WORLD);
  }

  void populate_cache(std::vector<DataTuple<DENT, embedding_dim>> *receivebuf,
                      MPI_Request &request) {
    MPI_Status status;
    MPI_Wait(&request, &status);

    // TODO parallaize
    for (int i = 0; i < this->grid->world_size; i++) {
      int base_index = this->rdispls[i];

      int count = this->receivecounts[i];

      for (int j = base_index; j < base_index + count; j++) {
        DataTuple<DENT, embedding_dim> t = (*receivebuf)[j];
//        (this->dense_local)->insert_cache(i, t.col, t.value);
      }
    }
  }

  void cross_validate_batch_from_metadata(int batch_id) {
    int total_nodes = this->sp_local->gCols / this->sp_local->block_col_width;
    for (int i = 0; i < total_nodes; i++) {
      vector<uint64_t> col_ids;
      this->sp_local->fill_col_ids(batch_id, i, col_ids, false, true);
      for (int j = 0; j < col_ids.size(); j++) {
        uint64_t global_col_id = col_ids[j];
        uint64_t local_col_id =
            global_col_id -
            static_cast<uint64_t>(
                ((this->grid)->global_rank * (this->sp_local)->proc_row_width));
        bool fetch_from_cache = false;

        int owner_rank =
            static_cast<int>(global_col_id / (this->sp_local)->proc_row_width);
        if (owner_rank != (this->grid)->global_rank) {
          fetch_from_cache = true;
        }
        if (fetch_from_cache) {
          if (!(this->dense_local)->searchForKey(global_col_id)) {
            cout << " Assert not found my_rank " << grid->global_rank
                 << "  target_rank " << owner_rank << " id " << global_col_id
                 << "batch Id" << batch_id << endl;
          }
        }
      }
    }
  }
};
} // namespace distblas::net
