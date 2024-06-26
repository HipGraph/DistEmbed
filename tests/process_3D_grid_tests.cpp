#include "../core/Process3DGrid.hpp"
#include <iostream>
#include <memory>
#include <mpi.h>
#include <stdlib.h>
#include <vector>

using namespace std;
using namespace distblas::core;

int main(int argc, char **argv) {

  int nr = atoi(argv[1]);
  int nc = atoi(argv[2]);
  int nl = atoi(argv[3]);
  int adj = atoi(argv[4]);

  MPI_Init(&argc, &argv);

  auto grid = shared_ptr<Process3DGrid>(new Process3DGrid(nr, nc, nl, adj));
  grid.get()->gather_and_pretty_print("Global Ranks", grid.get()->global_rank);
  grid.get()->gather_and_pretty_print("i values", grid.get()->i);
  grid.get()->gather_and_pretty_print("j values", grid.get()->j);
  grid.get()->gather_and_pretty_print("k values", grid.get()->k);

  int buf;
  buf = grid.get()->i;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->row_world);
  grid.get()->gather_and_pretty_print("Row Broadcast:", buf);

  buf = grid.get()->j;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->col_world);
  grid.get()->gather_and_pretty_print("Col Broadcast:", buf);

  buf = grid.get()->i + grid.get()->nr * grid.get()->j;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->fiber_world);
  grid.get()->gather_and_pretty_print("Fiber Broadcast:", buf);

  buf = grid.get()->k;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->rowcol_slice);
  grid.get()->gather_and_pretty_print("Row Column Slice Broadcast:", buf);

  buf = grid.get()->i;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->colfiber_slice);
  grid.get()->gather_and_pretty_print("Column Fiber Slice Broadcast:", buf);

  buf = grid.get()->j;
  MPI_Bcast(&buf, 1, MPI_INT, 0, grid.get()->rowfiber_slice);
  grid.get()->gather_and_pretty_print("Row Fiber Slice Broadcast:", buf);

  MPI_Finalize();
}
