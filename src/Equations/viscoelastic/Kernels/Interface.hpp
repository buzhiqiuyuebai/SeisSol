/**
 * @file
 * This file is part of SeisSol.
 *
 * @author Carsten Uphoff (c.uphoff AT tum.de,
 *http://www5.in.tum.de/wiki/index.php/Carsten_Uphoff,_M.Sc.)
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
 **/

#ifndef KERNELS_INTERFACE_H_
#define KERNELS_INTERFACE_H_

#include "Common/configtensor.hpp"
#include <Initializer/tree/InterfaceHelper.hpp>
#include <Initializer/LTS.h>
#include <Kernels/precision.hpp>
#include "Equations/elastic/Kernels/GravitationalFreeSurfaceBC.h"

namespace seissol::kernels {
template <typename Config,
          int Mechanisms,
          std::enable_if_t<std::is_same_v<typename Config::MaterialT,
                                          seissol::model::ViscoElasticMaterial<Mechanisms>>,
                           bool> = true>
struct alignas(Alignment) LocalTmp {
  using RealT = typename Config::RealT;
  alignas(Alignment) RealT timeIntegratedAne[Yateto<Config>::Tensor::Iane::size()]{};
  alignas(Alignment) std::array<
      RealT,
      Yateto<Config>::Tensor::averageNormalDisplacement::size()> nodalAvgDisplacements[4]{};
  GravitationalFreeSurfaceBc<Config> gravitationalFreeSurfaceBc{};
};
LTSTREE_GENERATE_INTERFACE(LocalData,
                           initializers::LTS<Config>,
                           cellInformation,
                           localIntegration,
                           dofs,
                           dofsAne,
                           faceDisplacements)
LTSTREE_GENERATE_INTERFACE(
    NeighborData, initializers::LTS<Config>, cellInformation, neighboringIntegration, dofs, dofsAne)
} // namespace seissol::kernels

#endif
