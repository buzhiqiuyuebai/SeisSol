#ifndef SEISSOL_FRICTIONSOLVER_COMMON_H
#define SEISSOL_FRICTIONSOLVER_COMMON_H

#include "DynamicRupture/Misc.h"
#include "DynamicRupture/Parameters.h"
#include "Initializer/DynamicRupture.h"
#include "Kernels/DynamicRupture.h"
#include "Numerical_aux/GaussianNucleationFunction.h"
#include "Common/constants.hpp"
#include <type_traits>

/**
 * Contains common functions required both for CPU and GPU impl.
 * of Dynamic Rupture solvers. The functions placed in
 * this class definition (of the header file) result
 * in the function inlining required for GPU impl.
 */
namespace seissol::dr::friction_law::common {

template <size_t Start, size_t End, size_t Step>
struct ForLoopRange {
  static constexpr size_t start{Start};
  static constexpr size_t end{End};
  static constexpr size_t step{Step};
};

enum class RangeType { CPU, GPU };

template <typename Config, RangeType Type>
struct NumPoints {
  private:
  using CpuRange = ForLoopRange<0, dr::misc::numPaddedPoints<Config>, 1>;
  using GpuRange = ForLoopRange<0, 1, 1>;

  public:
  using Range = typename std::conditional<Type == RangeType::CPU, CpuRange, GpuRange>::type;
};

template <typename Config, RangeType Type>
struct QInterpolated {
  private:
  using CpuRange = ForLoopRange<0, Yateto<Config>::Tensor::QInterpolated::size(), 1>;
  using GpuRange =
      ForLoopRange<0, Yateto<Config>::Tensor::QInterpolated::size(), misc::numPaddedPoints<Config>>;

  public:
  using Range = typename std::conditional<Type == RangeType::CPU, CpuRange, GpuRange>::type;
};

/**
 * Asserts whether all relevant arrays are properly aligned
 */
template <typename Config>
inline void checkAlignmentPreCompute(
    const typename Config::RealT qIPlus[Config::ConvergenceOrder][dr::misc::numQuantities<Config>]
                                       [dr::misc::numPaddedPoints<Config>],
    const typename Config::RealT qIMinus[Config::ConvergenceOrder][dr::misc::numQuantities<Config>]
                                        [dr::misc::numPaddedPoints<Config>],
    const FaultStresses<Config>& faultStresses) {
  using namespace dr::misc::quantity_indices;
  for (unsigned o = 0; o < Config::ConvergenceOrder; ++o) {
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][U]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][V]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][W]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][N]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][T1]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][T2]) % Alignment == 0);

    assert(reinterpret_cast<uintptr_t>(qIMinus[o][U]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][V]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][W]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][N]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][T1]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][T2]) % Alignment == 0);

    assert(reinterpret_cast<uintptr_t>(faultStresses.normalStress[o]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(faultStresses.traction1[o]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(faultStresses.traction2[o]) % Alignment == 0);
  }
}

/**
 * Calculate traction and normal stress at the interface of a face.
 * Using equations (A2) from Pelties et al. 2014
 * Definiton of eta and impedance Z are found in Carsten Uphoff's dissertation on page 47 and in
 * equation (4.51) respectively.
 *
 * @param[out] faultStresses contains normalStress, traction1, traction2
 *             at the 2d face quadrature nodes evaluated at the time
 *             quadrature points
 * @param[in] impAndEta contains eta and impedance values
 * @param[in] qInterpolatedPlus a plus side dofs interpolated at time sub-intervals
 * @param[in] qInterpolatedMinus a minus side dofs interpolated at time sub-intervals
 */
template <typename Config, RangeType Type = RangeType::CPU>
inline void precomputeStressFromQInterpolated(
    FaultStresses<Config>& faultStresses,
    const ImpedancesAndEta<Config>& impAndEta,
    const typename Config::RealT qInterpolatedPlus[Config::ConvergenceOrder]
                                                  [Yateto<Config>::Tensor::QInterpolated::size()],
    const typename Config::RealT qInterpolatedMinus[Config::ConvergenceOrder]
                                                   [Yateto<Config>::Tensor::QInterpolated::size()],
    unsigned startLoopIndex = 0) {

  static_assert(Yateto<Config>::Tensor::QInterpolated::Shape[0] ==
                    Yateto<Config>::Tensor::resample::Shape[0],
                "Different number of quadrature points?");

  const auto etaP = impAndEta.etaP;
  const auto etaS = impAndEta.etaS;
  const auto invZp = impAndEta.invZp;
  const auto invZs = impAndEta.invZs;
  const auto invZpNeig = impAndEta.invZpNeig;
  const auto invZsNeig = impAndEta.invZsNeig;

  using QInterpolatedShapeT =
      const typename Config::RealT(*)[misc::numQuantities<Config>][misc::numPaddedPoints<Config>];
  auto* qIPlus = (reinterpret_cast<QInterpolatedShapeT>(qInterpolatedPlus));
  auto* qIMinus = (reinterpret_cast<QInterpolatedShapeT>(qInterpolatedMinus));

  using namespace dr::misc::quantity_indices;

#ifndef ACL_DEVICE
  checkAlignmentPreCompute<Config>(qIPlus, qIMinus, faultStresses);
#endif

  for (unsigned o = 0; o < Config::ConvergenceOrder; ++o) {
    using Range = typename NumPoints<Config, Type>::Range;

#ifndef ACL_DEVICE
#pragma omp simd
#endif
    for (auto index = Range::start; index < Range::end; index += Range::step) {
      auto i{startLoopIndex + index};
      faultStresses.normalStress[o][i] =
          etaP * (qIMinus[o][U][i] - qIPlus[o][U][i] + qIPlus[o][N][i] * invZp +
                  qIMinus[o][N][i] * invZpNeig);

      faultStresses.traction1[o][i] =
          etaS * (qIMinus[o][V][i] - qIPlus[o][V][i] + qIPlus[o][T1][i] * invZs +
                  qIMinus[o][T1][i] * invZsNeig);

      faultStresses.traction2[o][i] =
          etaS * (qIMinus[o][W][i] - qIPlus[o][W][i] + qIPlus[o][T2][i] * invZs +
                  qIMinus[o][T2][i] * invZsNeig);
    }
  }
}

/**
 * Asserts whether all relevant arrays are properly aligned
 */
template <typename Config>
inline void checkAlignmentPostCompute(
    const typename Config::RealT qIPlus[Config::ConvergenceOrder][dr::misc::numQuantities<Config>]
                                       [dr::misc::numPaddedPoints<Config>],
    const typename Config::RealT qIMinus[Config::ConvergenceOrder][dr::misc::numQuantities<Config>]
                                        [dr::misc::numPaddedPoints<Config>],
    const typename Config::RealT imposedStateP[Config::ConvergenceOrder]
                                              [dr::misc::numPaddedPoints<Config>],
    const typename Config::RealT imposedStateM[Config::ConvergenceOrder]
                                              [dr::misc::numPaddedPoints<Config>],
    const FaultStresses<Config>& faultStresses,
    const TractionResults<Config>& tractionResults) {
  using namespace dr::misc::quantity_indices;

  assert(reinterpret_cast<uintptr_t>(imposedStateP[U]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateP[V]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateP[W]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateP[N]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateP[T1]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateP[T2]) % Alignment == 0);

  assert(reinterpret_cast<uintptr_t>(imposedStateM[U]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateM[V]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateM[W]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateM[N]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateM[T1]) % Alignment == 0);
  assert(reinterpret_cast<uintptr_t>(imposedStateM[T2]) % Alignment == 0);

  for (size_t o = 0; o < Config::ConvergenceOrder; ++o) {
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][U]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][V]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][W]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][N]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][T1]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIPlus[o][T2]) % Alignment == 0);

    assert(reinterpret_cast<uintptr_t>(qIMinus[o][U]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][V]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][W]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][N]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][T1]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(qIMinus[o][T2]) % Alignment == 0);

    assert(reinterpret_cast<uintptr_t>(faultStresses.normalStress[o]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(tractionResults.traction1[o]) % Alignment == 0);
    assert(reinterpret_cast<uintptr_t>(tractionResults.traction2[o]) % Alignment == 0);
  }
}

/**
 * Integrate over all Time points with the time weights and calculate the traction for each side
 * according to Carsten Uphoff's Thesis: EQ.: 4.60
 *
 * @param[in] faultStresses
 * @param[in] tractionResults
 * @param[in] impAndEta
 * @param[in] qInterpolatedPlus
 * @param[in] qInterpolatedMinus
 * @param[in] timeWeights
 * @param[out] imposedStatePlus
 * @param[out] imposedStateMinus
 */
template <typename Config, RangeType Type = RangeType::CPU>
inline void postcomputeImposedStateFromNewStress(
    const FaultStresses<Config>& faultStresses,
    const TractionResults<Config>& tractionResults,
    const ImpedancesAndEta<Config>& impAndEta,
    typename Config::RealT imposedStatePlus[Yateto<Config>::Tensor::QInterpolated::size()],
    typename Config::RealT imposedStateMinus[Yateto<Config>::Tensor::QInterpolated::size()],
    const typename Config::RealT qInterpolatedPlus[Config::ConvergenceOrder]
                                                  [Yateto<Config>::Tensor::QInterpolated::size()],
    const typename Config::RealT qInterpolatedMinus[Config::ConvergenceOrder]
                                                   [Yateto<Config>::Tensor::QInterpolated::size()],
    const double timeWeights[Config::ConvergenceOrder],
    unsigned startIndex = 0) {

  // set imposed state to zero
  using QInterpolatedRange = typename QInterpolated<Config, Type>::Range;
  for (auto index = QInterpolatedRange::start; index < QInterpolatedRange::end;
       index += QInterpolatedRange::step) {
    auto i{startIndex + index};
    imposedStatePlus[i] = static_cast<typename Config::RealT>(0.0);
    imposedStateMinus[i] = static_cast<typename Config::RealT>(0.0);
  }

  const auto invZs = impAndEta.invZs;
  const auto invZp = impAndEta.invZp;
  const auto invZsNeig = impAndEta.invZsNeig;
  const auto invZpNeig = impAndEta.invZpNeig;

  using ImposedStateShapeT = typename Config::RealT(*)[misc::numPaddedPoints<Config>];
  auto* imposedStateP = reinterpret_cast<ImposedStateShapeT>(imposedStatePlus);
  auto* imposedStateM = reinterpret_cast<ImposedStateShapeT>(imposedStateMinus);

  using QInterpolatedShapeT =
      const typename Config::RealT(*)[misc::numQuantities<Config>][misc::numPaddedPoints<Config>];
  auto* qIPlus = reinterpret_cast<QInterpolatedShapeT>(qInterpolatedPlus);
  auto* qIMinus = reinterpret_cast<QInterpolatedShapeT>(qInterpolatedMinus);

  using namespace dr::misc::quantity_indices;

#ifndef ACL_DEVICE
  checkAlignmentPostCompute(
      qIPlus, qIMinus, imposedStateP, imposedStateM, faultStresses, tractionResults);
#endif

  for (unsigned o = 0; o < Config::ConvergenceOrder; ++o) {
    auto weight = timeWeights[o];

    using NumPointsRange = typename NumPoints<Config, Type>::Range;
#ifndef ACL_DEVICE
#pragma omp simd
#endif
    for (auto index = NumPointsRange::start; index < NumPointsRange::end;
         index += NumPointsRange::step) {
      auto i{startIndex + index};

      const auto normalStress = faultStresses.normalStress[o][i];
      const auto traction1 = tractionResults.traction1[o][i];
      const auto traction2 = tractionResults.traction2[o][i];

      imposedStateM[N][i] += weight * normalStress;
      imposedStateM[T1][i] += weight * traction1;
      imposedStateM[T2][i] += weight * traction2;
      imposedStateM[U][i] +=
          weight * (qIMinus[o][U][i] - invZpNeig * (normalStress - qIMinus[o][N][i]));
      imposedStateM[V][i] +=
          weight * (qIMinus[o][V][i] - invZsNeig * (traction1 - qIMinus[o][T1][i]));
      imposedStateM[W][i] +=
          weight * (qIMinus[o][W][i] - invZsNeig * (traction2 - qIMinus[o][T2][i]));

      imposedStateP[N][i] += weight * normalStress;
      imposedStateP[T1][i] += weight * traction1;
      imposedStateP[T2][i] += weight * traction2;
      imposedStateP[U][i] += weight * (qIPlus[o][U][i] + invZp * (normalStress - qIPlus[o][N][i]));
      imposedStateP[V][i] += weight * (qIPlus[o][V][i] + invZs * (traction1 - qIPlus[o][T1][i]));
      imposedStateP[W][i] += weight * (qIPlus[o][W][i] + invZs * (traction2 - qIPlus[o][T2][i]));
    }
  }
}

/**
 * adjusts initial stresses based on the given nucleation ones
 *
 * @param[out] initialStressInFaultCS
 * @param[in] nucleationStressInFaultCS
 * @param[in] t0
 * @param[in] dt
 * @param[in] index - device iteration index
 */
template <typename Config,
          RangeType Type = RangeType::CPU,
          typename MathFunctions = seissol::functions::HostStdFunctions>
inline void adjustInitialStress(
    typename Config::RealT initialStressInFaultCS[misc::numPaddedPoints<Config>][6],
    const typename Config::RealT nucleationStressInFaultCS[misc::numPaddedPoints<Config>][6],
    typename Config::RealT fullUpdateTime,
    typename Config::RealT t0,
    typename Config::RealT dt,
    unsigned startIndex = 0) {
  if (fullUpdateTime <= t0) {
    const typename Config::RealT gNuc =
        gaussianNucleationFunction::smoothStepIncrement<MathFunctions>(fullUpdateTime, dt, t0);

    using Range = typename NumPoints<Config, Type>::Range;

#ifndef ACL_DEVICE
#pragma omp simd
#endif
    for (auto index = Range::start; index < Range::end; index += Range::step) {
      auto pointIndex{startIndex + index};
      for (unsigned i = 0; i < 6; i++) {
        initialStressInFaultCS[pointIndex][i] += nucleationStressInFaultCS[pointIndex][i] * gNuc;
      }
    }
  }
}

/**
 * output rupture front, saves update time of the rupture front
 * rupture front is the first registered change in slip rates that exceeds 0.001
 *
 * param[in,out] ruptureTimePending
 * param[out] ruptureTime
 * param[in] slipRateMagnitude
 * param[in] fullUpdateTime
 */
template <typename Config, RangeType Type = RangeType::CPU>
// See https://github.com/llvm/llvm-project/issues/60163
// NOLINTNEXTLINE
inline void saveRuptureFrontOutput(
    bool ruptureTimePending[misc::numPaddedPoints<Config>],
    // See https://github.com/llvm/llvm-project/issues/60163
    // NOLINTNEXTLINE
    typename Config::RealT ruptureTime[misc::numPaddedPoints<Config>],
    const typename Config::RealT slipRateMagnitude[misc::numPaddedPoints<Config>],
    typename Config::RealT fullUpdateTime,
    unsigned startIndex = 0) {

  using Range = typename NumPoints<Config, Type>::Range;

#ifndef ACL_DEVICE
#pragma omp simd
#endif
  for (auto index = Range::start; index < Range::end; index += Range::step) {
    auto pointIndex{startIndex + index};
    constexpr typename Config::RealT ruptureFrontThreshold = 0.001;
    if (ruptureTimePending[pointIndex] && slipRateMagnitude[pointIndex] > ruptureFrontThreshold) {
      ruptureTime[pointIndex] = fullUpdateTime;
      ruptureTimePending[pointIndex] = false;
    }
  }
}

/**
 * Save the maximal computed slip rate magnitude in peakSlipRate
 *
 * param[in] slipRateMagnitude
 * param[in, out] peakSlipRate
 */
template <typename Config, RangeType Type = RangeType::CPU>
inline void savePeakSlipRateOutput(
    const typename Config::RealT slipRateMagnitude[misc::numPaddedPoints<Config>],
    // See https://github.com/llvm/llvm-project/issues/60163
    // NOLINTNEXTLINE
    typename Config::RealT peakSlipRate[misc::numPaddedPoints<Config>],
    unsigned startIndex = 0) {

  using Range = typename NumPoints<Config, Type>::Range;

#ifndef ACL_DEVICE
#pragma omp simd
#endif
  for (auto index = Range::start; index < Range::end; index += Range::step) {
    auto pointIndex{startIndex + index};
    peakSlipRate[pointIndex] = std::max(peakSlipRate[pointIndex], slipRateMagnitude[pointIndex]);
  }
}

template <typename Config, RangeType Type = RangeType::CPU>
inline void computeFrictionEnergy(
    DREnergyOutput<Config>& energyData,
    const typename Config::RealT qInterpolatedPlus[Config::ConvergenceOrder]
                                                  [Yateto<Config>::Tensor::QInterpolated::size()],
    const typename Config::RealT qInterpolatedMinus[Config::ConvergenceOrder]
                                                   [Yateto<Config>::Tensor::QInterpolated::size()],
    const ImpedancesAndEta<Config>& impAndEta,
    const double timeWeights[Config::ConvergenceOrder],
    const typename Config::RealT spaceWeights[NUMBER_OF_SPACE_QUADRATURE_POINTS],
    const DRGodunovData<Config>& godunovData,
    size_t startIndex = 0) {

  auto* slip =
      reinterpret_cast<typename Config::RealT(*)[misc::numPaddedPoints<Config>]>(energyData.slip);
  auto* accumulatedSlip = energyData.accumulatedSlip;
  auto* frictionalEnergy = energyData.frictionalEnergy;
  const double doubledSurfaceArea = godunovData.doubledSurfaceArea;

  using QInterpolatedShapeT =
      const typename Config::RealT(*)[misc::numQuantities<Config>][misc::numPaddedPoints<Config>];
  auto* qIPlus = reinterpret_cast<QInterpolatedShapeT>(qInterpolatedPlus);
  auto* qIMinus = reinterpret_cast<QInterpolatedShapeT>(qInterpolatedMinus);

  const auto aPlus = impAndEta.etaP * impAndEta.invZp;
  const auto bPlus = impAndEta.etaS * impAndEta.invZs;

  const auto aMinus = impAndEta.etaP * impAndEta.invZpNeig;
  const auto bMinus = impAndEta.etaS * impAndEta.invZsNeig;

  using Range = typename NumPoints<Config, Type>::Range;

  using namespace dr::misc::quantity_indices;
  for (size_t o = 0; o < Config::ConvergenceOrder; ++o) {
    const auto timeWeight = timeWeights[o];

#ifndef ACL_DEVICE
#pragma omp simd
#endif
    for (size_t index = Range::start; index < Range::end; index += Range::step) {
      const size_t i{startIndex + index};

      const auto interpolatedSlipRate1 = qIMinus[o][U][i] - qIPlus[o][U][i];
      const auto interpolatedSlipRate2 = qIMinus[o][V][i] - qIPlus[o][V][i];
      const auto interpolatedSlipRate3 = qIMinus[o][W][i] - qIPlus[o][W][i];

      const auto interpolatedSlipRateMagnitude =
          misc::magnitude(interpolatedSlipRate1, interpolatedSlipRate2, interpolatedSlipRate3);

      accumulatedSlip[i] += timeWeight * interpolatedSlipRateMagnitude;

      slip[0][i] += timeWeight * interpolatedSlipRate1;
      slip[1][i] += timeWeight * interpolatedSlipRate2;
      slip[2][i] += timeWeight * interpolatedSlipRate3;

      const auto interpolatedTraction11 = aPlus * qIMinus[o][XX][i] + aMinus * qIPlus[o][XX][i];
      const auto interpolatedTraction12 = bPlus * qIMinus[o][XY][i] + bMinus * qIPlus[o][XY][i];
      const auto interpolatedTraction13 = bPlus * qIMinus[o][XZ][i] + bMinus * qIPlus[o][XZ][i];

      const auto spaceWeight = spaceWeights[i];
      const auto weight = -1.0 * timeWeight * spaceWeight * doubledSurfaceArea;
      frictionalEnergy[i] += weight * (interpolatedTraction11 * interpolatedSlipRate1 +
                                       interpolatedTraction12 * interpolatedSlipRate2 +
                                       interpolatedTraction13 * interpolatedSlipRate3);
    }
  }
}

} // namespace seissol::dr::friction_law::common

#endif // SEISSOL_FRICTIONSOLVER_COMMON_H
