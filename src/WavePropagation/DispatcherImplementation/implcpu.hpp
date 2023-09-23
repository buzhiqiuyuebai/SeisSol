#pragma once

#include "WavePropagation/dispatcher.hpp"
#include "Initializer/tree/Layer.hpp"
#include "Equations/Kernels.hpp"
#include "Kernels/Plasticity.h"
#include "Kernels/TimeCommon.h"
#include "Model/plasticity.hpp"
#include "implbase.hpp"

#include <WavePropagation/Kernel/Interaction/Interaction.hpp>
#include <vector>

namespace seissol::waveprop {

template <typename Config>
class WavePropDispatcherCPU : public WavePropDispatcherPre<Config> {
  public:
  using RealT = typename Config::RealT;
  WavePropDispatcherCPU(const seissol::initializers::LTS& lts, seissol::initializers::Layer& layer)
      : WavePropDispatcherPre<Config>(lts, layer) {}

  void dispatchPredict(double timeStepSize, double correctionTime, bool resetBuffers) override {
    // local integration buffer
    RealT l_integrationBuffer[Yateto<Config>::Tensor::I::size()] alignas(Alignment);

    // pointer for the call of the ADER-function
    RealT* l_bufferPointer;

    RealT** buffers = this->layer.var(this->lts.buffers);
    RealT** derivatives = this->layer.var(this->lts.derivatives);
    CellMaterialData* materialData = this->layer.var(this->lts.material);

    typename kernels::LocalData<Config>::Loader loader;
    loader.load(this->lts, this->layer);
    kernels::LocalTmp<Config> tmp{};

#ifdef _OPENMP
#pragma omp parallel for private(l_bufferPointer, l_integrationBuffer, tmp) schedule(static)
#endif
    for (unsigned int l_cell = 0; l_cell < this->layer.getNumberOfCells(); l_cell++) {
      auto data = loader.entry(l_cell);

      // We need to check, whether we can overwrite the buffer or if it is
      // needed by some other time cluster.
      // If we cannot overwrite the buffer, we compute everything in a temporary
      // local buffer and accumulate the results later in the shared buffer.
      const bool buffersProvided =
          (data.cellInformation.ltsSetup >> 8) % 2 == 1; // buffers are provided
      const bool resetMyBuffers =
          buffersProvided &&
          ((data.cellInformation.ltsSetup >> 10) % 2 == 0 || resetBuffers); // they should be reset

      if (resetMyBuffers) {
        // assert presence of the buffer
        assert(buffers[l_cell] != nullptr);

        l_bufferPointer = buffers[l_cell];
      } else {
        // work on local buffer
        l_bufferPointer = l_integrationBuffer;
      }

      this->timeKernel.computeAder(
          timeStepSize, data, tmp, l_bufferPointer, derivatives[l_cell], true);

      // Compute local integrals (including some boundary conditions)
      CellBoundaryMapping(*boundaryMapping)[4] = this->layer.var(this->lts.boundaryMapping);
      this->localKernel.computeIntegral(l_bufferPointer,
                                        data,
                                        tmp,
                                        &materialData[l_cell],
                                        &boundaryMapping[l_cell],
                                        correctionTime,
                                        timeStepSize);

      for (unsigned face = 0; face < 4; ++face) {
        auto& curFaceDisplacements = data.faceDisplacements[face];
        // Note: Displacement for freeSurfaceGravity is computed in Time.cpp
        if (curFaceDisplacements != nullptr &&
            data.cellInformation.faceTypes[face] != FaceType::freeSurfaceGravity) {
          typename Yateto<Config>::Kernel::addVelocity addVelocityKrnl;

          addVelocityKrnl.V3mTo2nFace = this->globalData.onHost->V3mTo2nFace;
          addVelocityKrnl.selectVelocity = Yateto<Config>::Init::selectVelocity::Values;
          addVelocityKrnl.faceDisplacement = data.faceDisplacements[face];
          addVelocityKrnl.I = l_bufferPointer;
          addVelocityKrnl.execute(face);
        }
      }

      // TODO: Integrate this step into the kernel
      // We've used a temporary buffer -> need to accumulate update in
      // shared buffer.
      if (!resetMyBuffers && buffersProvided) {
        assert(buffers[l_cell] != nullptr);

        for (unsigned int l_dof = 0; l_dof < Yateto<Config>::Tensor::I::size(); ++l_dof) {
          buffers[l_cell][l_dof] += l_integrationBuffer[l_dof];
        }
      }
    }
  }

  void dispatchNeighborCorrect(double timeStepSize, double subTimeStart) override {
    if (this->layer.getNumberOfCells() == 0) {
      return;
    }

    RealT*(*faceNeighbors)[4] = this->layer.var(this->lts.faceNeighbors);
    CellDRMapping(*drMapping)[4] = this->layer.var(this->lts.drMapping);
    CellLocalInformation* cellInformation = this->layer.var(this->lts.cellInformation);
    auto* plasticity = this->layer.var(this->lts.plasticity);
    auto* pstrain = this->layer.var(this->lts.pstrain);
    unsigned numberOTetsWithPlasticYielding = 0;

    typename kernels::NeighborData<Config>::Loader loader;
    loader.load(this->lts, this->layer);

    RealT* l_timeIntegrated[4];
    RealT* l_faceNeighbors_prefetch[4];

#ifdef _OPENMP
#pragma omp parallel for schedule(static) private(l_timeIntegrated, l_faceNeighbors_prefetch)
#endif
    for (unsigned int l_cell = 0; l_cell < this->layer.getNumberOfCells(); l_cell++) {
      auto data = loader.entry(l_cell);
      seissol::waveprop::kernel::TimeIntegrator<Config>::timeIntegrateCell(
          l_timeIntegrated,
          faceNeighbors[l_cell],
#ifdef _OPENMP
          *reinterpret_cast<RealT(*)[4][Yateto<Config>::Tensor::I::size()]>(
              &(this->globalData.onHost->integrationBufferLTS[omp_get_thread_num() * 4 *
                                                              Yateto<Config>::Tensor::I::size()])),
#else
          *reinterpret_cast<RealT(*)[4][Yateto<Config>::Tensor::I::size()]>(
              this->globalData.onHost->integrationBufferLTS),
#endif
          data.cellInformation,
          subTimeStart,
          subTimeStart + timeStepSize);

      l_faceNeighbors_prefetch[0] =
          (cellInformation[l_cell].faceTypes[1] != FaceType::dynamicRupture)
              ? faceNeighbors[l_cell][1]
              : drMapping[l_cell][1].godunov;
      l_faceNeighbors_prefetch[1] =
          (cellInformation[l_cell].faceTypes[2] != FaceType::dynamicRupture)
              ? faceNeighbors[l_cell][2]
              : drMapping[l_cell][2].godunov;
      l_faceNeighbors_prefetch[2] =
          (cellInformation[l_cell].faceTypes[3] != FaceType::dynamicRupture)
              ? faceNeighbors[l_cell][3]
              : drMapping[l_cell][3].godunov;

      // fourth face's prefetches
      if (l_cell < (this->layer.getNumberOfCells() - 1)) {
        l_faceNeighbors_prefetch[3] =
            (cellInformation[l_cell + 1].faceTypes[0] != FaceType::dynamicRupture)
                ? faceNeighbors[l_cell + 1][0]
                : drMapping[l_cell + 1][0].godunov;
      } else {
        l_faceNeighbors_prefetch[3] = faceNeighbors[l_cell][3];
      }

      this->neighborKernel.computeNeighborsIntegral(
          data, drMapping[l_cell], l_timeIntegrated, l_faceNeighbors_prefetch);

      if constexpr (Config::Plasticity) {
        auto oneMinusIntegratingFactor =
            (this->tv > 0.0) ? 1.0 - exp(-timeStepSize / this->tv) : 1.0;
        numberOTetsWithPlasticYielding +=
            seissol::kernels::Plasticity::computePlasticity(oneMinusIntegratingFactor,
                                                            timeStepSize,
                                                            this->tv,
                                                            this->globalData.onHost,
                                                            &plasticity[l_cell],
                                                            data.dofs,
                                                            pstrain[l_cell]);
      }
#ifdef INTEGRATE_QUANTITIES
      seissol::SeisSol::main.postProcessor().integrateQuantities(
          timeStepSize, this->layer, l_cell, dofs[l_cell]);
#endif // INTEGRATE_QUANTITIES
    }
  }
};
} // namespace seissol::waveprop