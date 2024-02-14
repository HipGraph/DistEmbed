#pragma once

namespace distblas::net {

template <typename INDEX_TYPE, typename VALUE_TYPE, size_t embedding_dim>
class TileDataComm : public DataComm<INDEX_TYPE, VALUE_TYPE, embedding_dim> {

private:
public:
  TileDataComm(distblas::core::SpMat<VALUE_TYPE> *sp_local_receiver,
               distblas::core::SpMat<VALUE_TYPE> *sp_local_sender,
               DenseMat<INDEX_TYPE, VALUE_TYPE, embedding_dim> *dense_local,
               Process3DGrid *grid, int batch_id, double alpha)
      : DataComm<INDEX_TYPE, VALUE_TYPE, embedding_dim>(
            sp_local_receiver, sp_local_sender, dense_local, grid, batch_id,alpha) {}

  TileDataComm(SpMat<VALUE_TYPE> *sp_local_receiver,
               SpMat<VALUE_TYPE> *sp_local_sender,
               SpMat<VALUE_TYPE> *sparse_local, Process3DGrid *grid,
               int batch_id, double alpha)
      : DataComm<INDEX_TYPE, VALUE_TYPE, embedding_dim>(
            sp_local_receiver, sp_local_sender, sparse_local, grid, batch_id,alpha) {}

  ~TileDataComm() {}

  void onboard_data() override {

  }
};

} // namespace distblas::net
