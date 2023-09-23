/**
 * @file
 * This file is part of SeisSol.
 *
 * @author Carsten Uphoff (c.uphoff AT tum.de, http://www5.in.tum.de/wiki/index.php/Carsten_Uphoff,_M.Sc.)
 *
 * @section LICENSE
 * Copyright (c) 2019, SeisSol Group
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 * 
 **/

#include "InitialFieldProjection.h"

#include <Numerical_aux/Quadrature.h>
#include <Numerical_aux/BasisFunction.h>
#include <Numerical_aux/Transformation.h>
#include <generated_code/kernel.h>
#include <generated_code/tensor.h>

GENERATE_HAS_MEMBER(selectAneFull)
GENERATE_HAS_MEMBER(selectElaFull)
GENERATE_HAS_MEMBER(Values)
GENERATE_HAS_MEMBER(Qane)

namespace seissol {
  namespace init {
    class selectAneFull;
    class selectElaFull;
  }
}

template<typename Config>
void seissol::initializers::projectInitialField<Config>(std::vector<std::unique_ptr<physics::InitialField>> const&  iniFields,
                                                GlobalDataStorage const& globalData,
                                                seissol::geometry::MeshReader const& meshReader,
                                                ClusterLTSForest const& cluster) {
  auto const& vertices = meshReader.getVertices();
  auto const& elements = meshReader.getElements();

  cluster.visitLayers([&](auto&& layerview) {
    using Config == typename std::decay_t<decltype(layerview)>::ConfigT;
    using RealT = typename Config::RealT;

    if (layerview.icg == Ghost) {
      return;
    }

  constexpr auto quadPolyDegree = Config::ConvergenceOrder+1;
  constexpr auto numQuadPoints = quadPolyDegree * quadPolyDegree * quadPolyDegree;

  double quadraturePoints[numQuadPoints][3];
  double quadratureWeights[numQuadPoints];
  seissol::quadrature::TetrahedronQuadrature(quadraturePoints, quadratureWeights, quadPolyDegree);

#ifdef _OPENMP
  #pragma omp parallel
  {
#endif
  RealT iniCondData[tensor::iniCond::size()] __attribute__((aligned(Alignment))) = {};
  auto iniCond = Yateto<Config>::Init::iniCond::view::create(iniCondData);

  std::vector<std::array<double, 3>> quadraturePointsXyz;
  quadraturePointsXyz.resize(numQuadPoints);

  typename Yateto<Config>::Kernel::projectIniCond krnl;
  krnl.projectQP = globalData.projectQPMatrix;
  krnl.iniCond = iniCondData;
  kernels::set_selectAneFull(krnl, kernels::get_static_ptr_Values<Yateto<Config>::Init::selectAneFull>());
  kernels::set_selectElaFull(krnl, kernels::get_static_ptr_Values<Yateto<Config>::Init::selectElaFull>());

#ifdef _OPENMP
  #pragma omp for schedule(static)
#endif
  for (unsigned cell = 0; cell < layerview.layer.getNumberOfCells(); ++cell) {
    int meshId = 

    double const* elementCoords[4];
    for (size_t v = 0; v < 4; ++v) {
      elementCoords[v] = vertices[elements[meshId].vertices[ v ] ].coords;
    }
    for (size_t i = 0; i < numQuadPoints; ++i) {
      seissol::transformations::tetrahedronReferenceToGlobal(elementCoords[0], elementCoords[1], elementCoords[2], elementCoords[3], quadraturePoints[i], quadraturePointsXyz[i].data());
    }

    const auto& material = layerview.var(layerview.lts.materialData)[cell];
#ifdef MULTIPLE_SIMULATIONS
    for (int s = 0; s < MULTIPLE_SIMULATIONS; ++s) {
      auto sub = iniCond.subtensor(s, yateto::slice<>(), yateto::slice<>());
      iniFields[s % iniFields.size()]->evaluate(0.0, quadraturePointsXyz, &material, sub);
    }
#else
    iniFields[0]->evaluate(0.0, quadraturePointsXyz, &material, iniCond);
#endif

    krnl.Q = layerview.var(layerview.lts.dofs)[cell];
    if constexpr (kernels::has_size<tensor::Qane>::value) {
      kernels::set_Qane(krnl, layerview.var(layerview.lts.dofsAne)[cell]);
    }
    krnl.execute();
  }
#ifdef _OPENMP
  }
#endif
  });
}
