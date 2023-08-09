#pragma once
#include "common.h"
#include "csr_linked_list.hpp"
#include "csr_local.hpp"
#include "distributed_mat.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <parallel/algorithm>
#include <unordered_set>
#include <vector>

using namespace std;

namespace distblas::core {

/**
 * This class represents the Sparse Matrix
 */
template <typename T> class SpMat : public DistributedMat {

private:
  vector<uint64_t> block_col_starts;
  vector<uint64_t> block_row_starts;

  vector<shared_ptr<CSRLinkedList<T>>> csr_linked_lists;

public:
  uint64_t gRows, gCols, gNNz;
  vector<Tuple<T>> coords;
  int block_row_width, block_col_width;
  int proc_col_width, proc_row_width;
  int number_of_local_csr_nodes;
  bool col_merged = false;

  /**
   * Constructor for Sparse Matrix representation of  Adj matrix
   * @param coords  (src, dst, value) Tuple vector loaded as input
   * @param gRows   total number of Rows in Distributed global Adj matrix
   * @param gCols   total number of Cols in Distributed global Adj matrix
   * @param gNNz     total number of NNz in Distributed global Adj matrix
   */
  SpMat(vector<Tuple<T>> &coords, uint64_t &gRows, uint64_t &gCols,
        uint64_t &gNNz, int &block_row_width, int &block_col_width,
        int &proc_row_width, int &proc_col_width, bool col_merged) {
    this->gRows = gRows;
    this->gCols = gCols;
    this->gNNz = gNNz;
    this->coords = coords;
    this->block_row_width = block_row_width;
    this->block_col_width = block_col_width;
    this->proc_col_width = proc_col_width;
    this->proc_row_width = proc_row_width;
    this->col_merged = col_merged;
//    if (col_merged) {
#pragma omp parallel for
      for (int i = 0; i < coords.size(); i++) {
        this->coords[i].value = static_cast<T>(coords[i].col);
      }
//    }
  }

  SpMat() {}

  void divide_block_cols(int batch_size, bool mod_ind, bool trans) {
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    block_col_starts.clear();

    int current_start = 0;
    int next_start = batch_size;
    if (trans) {
      current_start = proc_col_width * rank;
      next_start = current_start + batch_size;
    }

    if (!col_merged) {
      // TODO: introduce atomic capture
      bool divided_equallaly = true;
      int last_proc_batch_size = batch_size;
      int batch_count = proc_col_width / batch_size;
      if (!trans and proc_col_width % batch_size != 0) {

        divided_equallaly = false;
        last_proc_batch_size = proc_col_width - batch_size * batch_count;
        batch_count = batch_count + 1;
      }

      for (uint64_t i = 0; i < coords.size(); i++) {
        while (coords[i].col >= current_start) {
          block_col_starts.push_back(i);

          if (coords[i].col >= next_start) {
            while (coords[i].col >= next_start) {
              block_col_starts.push_back(i);
              if (!divided_equallaly) {
                if (i > 0 and block_col_starts.size() % batch_count == 0) {
                  current_start += last_proc_batch_size;
                  next_start += batch_size;
                } else if (i > 0 and
                           (block_col_starts.size() + 1) % batch_count == 0) {
                  current_start += batch_size;
                  next_start += last_proc_batch_size;
                } else {
                  current_start += batch_size;
                  next_start += batch_size;
                }
              } else {
                current_start += batch_size;
                next_start += batch_size;
              }
            }
          } else {
            if (!divided_equallaly) {
              if (i > 0 and block_col_starts.size() % batch_count == 0) {
                current_start += last_proc_batch_size;
                next_start += batch_size;
              } else if (i > 0 and
                         (block_col_starts.size() + 1) % batch_count == 0) {
                current_start += batch_size;
                next_start += last_proc_batch_size;
              } else {
                current_start += batch_size;
                next_start += batch_size;
              }
            } else {
              current_start += batch_size;
              next_start += batch_size;
            }
          }
          //          if (rank == 0) {
          //            cout << " current start: " << current_start
          //                 << " size: " << block_col_starts.size() << endl;
          //          }
        }

        // This modding step helps indexing.
        if (mod_ind) {
          coords[i].col %= batch_size;
        }
      }

    } else {
      int start_index = 0;
      int end_index = 0;
      uint64_t checking_index = rank * proc_col_width;
      //      uint64_t checking_index = 1;
      uint64_t checking_end_index = (rank + 1) * proc_col_width - 1;

      if (rank == world_size - 1) {
        checking_end_index = std::max(
            static_cast<uint64_t>((rank + 1) * proc_col_width) - 1, gCols - 1);
      }

      auto startIt = std::find_if(coords.begin(), coords.end(),
                                  [&checking_index](const auto &tuple) {
                                    return tuple.col >= checking_index;
                                  });

      auto endIt = std::find_if(coords.rbegin(), coords.rend(),
                                [&checking_end_index](const auto &tuple) {
                                  return tuple.col <= checking_end_index;
                                });

      //      cout << "checking_index" << checking_index << "checking_end_index"
      //           << checking_end_index << endl;
      //      std::cout << "Start value: (" << (*startIt).row << ", " <<
      //      (*startIt).col
      //                << ")" << std::endl;
      //      std::cout << "End value: (" << (*endIt).row << ", " <<
      //      (*endIt).col << ")"
      //                << std::endl;
      std::rotate(coords.begin(), startIt, std::next(endIt).base());
      uint64_t startIndex = std::distance(coords.begin(), startIt);
      uint64_t endIndex =
          std::distance(coords.begin(), std::next(endIt).base());
      uint64_t first_batch_len = (endIndex + 1) - startIndex;
      uint64_t second_batch_len = coords.size() - first_batch_len;

      int considered_col_width = proc_col_width;
      if (rank == world_size - 1) {
        considered_col_width = gCols - proc_col_width * (world_size - 1);
      }

      if (mod_ind) {
        std::transform(coords.begin(), coords.begin() + first_batch_len,
                       coords.begin(),
                       [&considered_col_width](const auto &tuple) {
                         const auto &[row, col, value] = tuple;
                         //                         int64_t modifiedCol = col %
                         //                         (first_batch_len);
                         int64_t modifiedCol = col % (considered_col_width);
                         return Tuple<T>{row, modifiedCol, value};
                       });
        std::transform(coords.begin() + first_batch_len, coords.end(),
                       coords.begin() + first_batch_len,
                       [&](const auto &tuple) {
                         const auto &[row, col, value] = tuple;
                         int64_t modifiedCol = col;
                         return Tuple<T>{row, modifiedCol, value};
                       });
      }

      block_col_starts.push_back(0);
      block_col_starts.push_back(first_batch_len);
      //      std::cout << " first_batch_len " << first_batch_len << " size "
      //                << coords.size() << std::endl;
    }
    block_col_starts.push_back(coords.size());
    //    if (!col_merged) {
    //      std::cout << " trans " << trans << "col_blocks" <<
    //      block_col_starts.size()
    //                << std::endl;
    //    }
  }

  void sort_by_rows() {
    for (int i = 0; i < block_col_starts.size() - 1; i++) {
      __gnu_parallel::sort(coords.begin() + block_col_starts[i],
                           coords.begin() + block_col_starts[i + 1],
                           row_major<T>);
    }
  }

  void divide_block_rows(int batch_size, bool mod_ind, bool trans) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    block_row_starts.clear();

    int expected_batch_count = (trans)?
                                       ((proc_row_width % batch_size ==0)?(gRows/batch_size):(((proc_row_width/batch_size)+1)*world_size)):((proc_row_width % batch_size ==0)?(proc_row_width / batch_size):(proc_row_width / batch_size)+1);

    bool divided_equallaly = true;
    int last_proc_batch_size = batch_size;
    int batch_count = proc_row_width / batch_size;

    if (proc_row_width % batch_size != 0) {
      divided_equallaly = false;
      last_proc_batch_size = proc_row_width - batch_size * batch_count;
      batch_count = batch_count + 1;
    }

//    if (rank==0){
//      cout<<" trans "<<trans<< " expected_batch_count "<<expected_batch_count<< " batch_count "<<batch_count<<endl;
//    }
    for (uint64_t i = 0; i < block_col_starts.size() - 1; i++) {

      int current_start = proc_row_width * rank;
      int next_start = current_start + batch_size;
      if (trans) {
        current_start = 0;
        next_start = batch_size;
      }

      // TODO: introduce atomic capture
      int matched_count = 0;
      for (uint64_t j = block_col_starts[i]; j < block_col_starts[i + 1]; j++) {
        while (coords[j].row >= current_start) {
          block_row_starts.push_back(j);
          ++matched_count;
          if (coords[j].row >= next_start) {
            while (coords[j].row >= next_start) {
              block_row_starts.push_back(j);
              ++matched_count;
              if (!divided_equallaly) {
                if (j > block_col_starts[i] and (matched_count) % (batch_count) == 0) {
                  current_start += last_proc_batch_size;
                  next_start += batch_size;
                } else if (j > block_col_starts[i] and
                           (matched_count + 1) % (batch_count) == 0) {
                  current_start += batch_size;
                  next_start += last_proc_batch_size;
                } else {
                  current_start += batch_size;
                  next_start += batch_size;
                }
              } else {
                current_start += batch_size;
                next_start += batch_size;
              }
            }
          } else {
            if (!divided_equallaly) {
              if (j > block_col_starts[i] and matched_count % (batch_count) == 0) {
                current_start += last_proc_batch_size;
                next_start += batch_size;
              } else if (j > block_col_starts[i] and
                         (matched_count + 1) % (batch_count) == 0) {
                current_start += batch_size;
                next_start += last_proc_batch_size;
              } else {
                current_start += batch_size;
                next_start += batch_size;
              }
            } else {
              current_start += batch_size;
              next_start += batch_size;
            }
          }
//          if (rank == 0 ) {
//            cout << " current row start: " << current_start
//                 << " size: " << matched_count << endl;
//          }
        }

        // This modding step helps indexing.
        if (mod_ind) {
          coords[j].row %= batch_size;
        }
      }

      //      if (col_merged) {
      //        std::cout << " expected_matched_count " <<
      //        expected_matched_count
      //                  << " matched_count " << matched_count << std::endl;
      //      }



      while (matched_count < expected_batch_count) {
        block_row_starts.push_back(block_col_starts[i + 1]);
        matched_count++;
      }
//      if ( rank == 0) {
//        std::cout << " i th batch " << i << " row_blocks"
//                  << block_row_starts.size() << std::endl;
//      }
    }
    block_row_starts.push_back(coords.size());
  }

  void initialize_CSR_blocks(int block_rows, int block_cols, bool mod_ind,
                             bool transpose) {
    auto ini_csr_start = std::chrono::high_resolution_clock::now();

    this->divide_block_cols(block_cols, mod_ind, transpose);
    this->sort_by_rows();
    this->divide_block_rows(block_rows, mod_ind, transpose);

    auto ini_csr_end = std::chrono::high_resolution_clock::now();
    auto train_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                              ini_csr_end - ini_csr_start)
                              .count();

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);


    cout << "rank"<<rank<< " initialization time " << train_duration / 1000 << endl;

    int col_block = 0;

    this->number_of_local_csr_nodes =
        (transpose) ? (gRows / block_rows)
                    : ((proc_col_width %block_cols)==0)? (((proc_col_width/block_cols)+1)*world_size):(gCols/block_cols); // This assumes 1D partitioning, we
                                            // need to generalized this


    int no_of_lists = (transpose) ? ((proc_col_width % block_cols == 0)
                                         ? (proc_col_width / block_cols)
                                         : (proc_col_width / block_cols) + 1)
                                  : ((proc_row_width % block_rows == 0)
                                         ? (proc_row_width / block_rows)
                                         : (proc_row_width / block_rows) + 1);


//    cout << "rank"<<rank<< " no_of_lists  "<<no_of_lists << " number_of_local_csr_nodes " << this->number_of_local_csr_nodes << endl;
    csr_linked_lists =
        std::vector<std::shared_ptr<CSRLinkedList<T>>>(no_of_lists);

    for (int i = 0; i < no_of_lists; i++) {
      csr_linked_lists[i] =
          std::make_shared<CSRLinkedList<T>>();
    }

    auto ini_csr_end_while = std::chrono::high_resolution_clock::now();
    auto train_duration_init =
        std::chrono::duration_cast<std::chrono::microseconds>(
            ini_csr_end_while - ini_csr_end)
            .count();
//    cout << " train duration while " << train_duration_init / 1000 << endl;

    int node_index = 0;
    for (int j = 0; j < block_row_starts.size() - 1; j++) {
      int current_vector_pos = 0;
      if (!transpose) {
        current_vector_pos = j % no_of_lists;
        if (j > 0 and current_vector_pos == 0) {
          ++col_block;
          ++node_index;
        }
      } else {
        current_vector_pos = j / this->number_of_local_csr_nodes;
        col_block = current_vector_pos;
        if (node_index >= this->number_of_local_csr_nodes) {
          node_index = 0;
        }
        ++node_index;
      }

      int num_coords = block_row_starts[j + 1] - block_row_starts[j];

      Tuple<T> *coords_ptr = (coords.data() + block_row_starts[j]);

      //TODO change
//      (csr_linked_lists[current_vector_pos].get())
//          ->insert(block_rows, (col_merged) ? gCols : block_cols, num_coords,
//                   coords_ptr, num_coords, false, node_index);

      (csr_linked_lists[current_vector_pos].get())
          ->insert(gRows,  gCols , num_coords,
                   coords_ptr, num_coords, false, node_index);
    }
  }

  void fill_col_ids(int block_row_id, int block_col_id,
                    vector<uint64_t> &col_ids, bool transpose,
                    bool return_global_ids) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int csr_linked_list_id = (transpose) ? block_col_id : block_row_id;
    int batch_id = (transpose) ? block_row_id : block_col_id;

    auto linkedList = csr_linked_lists[csr_linked_list_id];

    auto head = (linkedList.get())->getHeadNode();
    int count = 0;
    while (count < batch_id && (head.get())->next != nullptr) {
      head = (head.get())->next;
      ++count;
    }
    if (count == batch_id) {
      auto csr_data = (head.get())->data;
      //      cout << " rank  " << rank << " inside fill_col_ids coords ( "
      //           << block_row_id << " ," << block_col_id << ")"
      //           << (csr_data.get())->num_coords << endl;
      if ((csr_data.get())->num_coords > 0) {
        int block_row_width = this->block_row_width;
        int block_col_width = this->block_col_width;
        int proc_row_width = this->proc_row_width;
        int proc_col_width = this->proc_col_width;
        distblas::core::CSRHandle *handle = (csr_data.get())->handler.get();
//        col_ids = vector<uint64_t>((handle->col_idx).size());
        col_ids = vector<uint64_t>((handle->values).size());
//        std::transform(
//            std::begin((handle->col_idx)), std::end((handle->col_idx)),
//            std::begin(col_ids),
//            [&return_global_ids, &rank, &transpose, &batch_id, &block_col_id,
//             &block_row_width, &block_col_width, &proc_col_width,
//             &proc_row_width](MKL_INT value) {
//              if (!return_global_ids) {
//                return static_cast<uint64_t>(value);
//              } else {
//                int starting_index = (transpose) ? rank * proc_col_width : 0;
//                uint64_t base_id =
//                    static_cast<uint64_t>(block_col_id * block_col_width);
//                uint64_t g_index = static_cast<uint64_t>(value) + base_id +
//                                   static_cast<uint64_t>(starting_index);
//                //TODO: do proper transformation here
//                return g_index;
//              }
//            });

        std::transform(
            std::begin((handle->values)), std::end((handle->values)),
            std::begin(col_ids),
            [&return_global_ids, &rank, &transpose, &batch_id, &block_col_id,
             &block_row_width, &block_col_width, &proc_col_width,
             &proc_row_width](double value) {
              return static_cast<uint64_t>(value);
            });
      }
    }
    //    }
  }

  CSRLinkedList<T> *get_batch_list(int batch_id) {
    return csr_linked_lists[batch_id].get();
  }

  void print_blocks_and_cols(bool trans) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //    cout << " rank " << rank << "_" << trans
    //         << " printing print_blocks_and_cols" << endl;
    int current_col_block = 0;
    for (int j = 0; j < csr_linked_lists.size(); j++) {
      cout << " rank " << rank << " j " << j << endl;
      auto linkedList = csr_linked_lists[j];

      auto head = (linkedList.get())->getHeadNode();

      int count = 0;
      while (head != nullptr) {
        cout << " rank " << rank << " j " << j << " count " << count
             << " node id " << (head.get())->id << endl;
        string output_path = "blocks_rank" + to_string(rank) + "_trans" +
                             to_string(trans) + "_col_" +
                             to_string((trans) ? j : count) + "_row_" +
                             to_string((trans) ? count : j) + ".txt";
        char stats[500];
        strcpy(stats, output_path.c_str());
        ofstream fout(stats, std::ios_base::app);

        auto csr_data = (head.get())->data;

        int num_coords = (csr_data.get())->num_coords;

        distblas::core::CSRHandle *handle = (csr_data.get())->handler.get();
        int numRows = handle->rowStart.size() - 1;

        for (int i = 0; i < numRows; i++) {
          int start = handle->rowStart[i];
          int end = handle->rowStart[i + 1];
          fout << "Row " << i << ": ";
          if (num_coords > 0) {
            for (int k = start; k < end; k++) {

              int col = handle->col_idx[k];
              int value = handle->values[k];

              if (value > 60000) {
                cout << "Rank " << rank << " j " << j
                     << " Large value encountered "
                     << " Row " << i << " col " << col << " value " << value
                     << endl;
              }
              fout << "(" << col << ", " << value << ") ";
            }
          }
          fout << endl;
        }
        head = (head.get())->next;
        ++count;
      }
    }
  }

  void print_coords(bool trans) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    string output_path =
        "coords" + to_string(rank) + "trans" + to_string(trans) + ".txt";
    char stats[500];
    strcpy(stats, output_path.c_str());
    ofstream fout(stats, std::ios_base::app);

    for (int i = 0; i < coords.size(); i++) {
      fout << coords[i].row << " " << coords[i].col << " " << endl;
    }
  }

  ~SpMat() {}
};

} // namespace distblas::core
