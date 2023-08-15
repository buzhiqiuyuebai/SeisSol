#ifndef SEISSOL_RATEANDSTATE_H
#define SEISSOL_RATEANDSTATE_H

#include "BaseFrictionLaw.h"
#include "DynamicRupture/FrictionLaws/RateAndStateCommon.h"

namespace seissol::dr::friction_law {
/**
 * General implementation of a rate and state solver
 * Methods are inherited via CRTP and must be implemented in the child class.
 */
template <typename Config, class Derived, class TPMethod>
class RateAndStateBase
    : public BaseFrictionLaw<Config, RateAndStateBase<Config, Derived, TPMethod>> {
  public:
  using RealT = typename Config::RealT;
  explicit RateAndStateBase(DRParameters* drParameters)
      : BaseFrictionLaw<Config, RateAndStateBase<Config, Derived, TPMethod>>::BaseFrictionLaw(
            drParameters),
        tpMethod(TPMethod(drParameters)) {}

  void updateFrictionAndSlip(FaultStresses<Config> const& faultStresses,
                             TractionResults<Config>& tractionResults,
                             std::array<RealT, misc::numPaddedPoints<Config>>& stateVariableBuffer,
                             std::array<RealT, misc::numPaddedPoints<Config>>& strengthBuffer,
                             unsigned ltsFace,
                             unsigned timeIndex) {
    bool hasConverged = false;

    // compute initial slip rate and reference values
    auto initialVariables = static_cast<Derived*>(this)->calcInitialVariables(
        faultStresses, stateVariableBuffer, timeIndex, ltsFace);
    std::array<RealT, misc::numPaddedPoints<Config>> absoluteShearStress =
        std::move(initialVariables.absoluteShearTraction);
    std::array<RealT, misc::numPaddedPoints<Config>> localSlipRate =
        std::move(initialVariables.localSlipRate);
    std::array<RealT, misc::numPaddedPoints<Config>> normalStress =
        std::move(initialVariables.normalStress);
    std::array<RealT, misc::numPaddedPoints<Config>> stateVarReference =
        std::move(initialVariables.stateVarReference);
    // compute slip rates by solving non-linear system of equations
    this->updateStateVariableIterative(hasConverged,
                                       stateVarReference,
                                       localSlipRate,
                                       stateVariableBuffer,
                                       normalStress,
                                       absoluteShearStress,
                                       faultStresses,
                                       timeIndex,
                                       ltsFace);

    // check for convergence
    if (!hasConverged) {
      static_cast<Derived*>(this)->executeIfNotConverged(stateVariableBuffer, ltsFace);
    }
    // compute final thermal pressure and normalStress
    tpMethod.calcFluidPressure(
        normalStress, this->mu, localSlipRate, this->deltaT[timeIndex], true, timeIndex, ltsFace);
    updateNormalStress(normalStress, faultStresses, timeIndex, ltsFace);
    // compute final slip rates and traction from average of the iterative solution and initial
    // guess
    this->calcSlipRateAndTraction(stateVarReference,
                                  localSlipRate,
                                  stateVariableBuffer,
                                  normalStress,
                                  absoluteShearStress,
                                  faultStresses,
                                  tractionResults,
                                  timeIndex,
                                  ltsFace);
  }

  void preHook(std::array<RealT, misc::numPaddedPoints<Config>>& stateVariableBuffer,
               unsigned ltsFace) {
// copy state variable from last time step
#pragma omp simd
    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
      stateVariableBuffer[pointIndex] = this->stateVariable[ltsFace][pointIndex];
    }
  }

  void postHook(std::array<RealT, misc::numPaddedPoints<Config>>& stateVariableBuffer,
                unsigned ltsFace) {
    static_cast<Derived*>(this)->resampleStateVar(stateVariableBuffer, ltsFace);
  }

  void copyLtsTreeToLocal(seissol::initializers::Layer& layerData,
                          seissol::initializers::DynamicRupture<Config> const* const dynRup,
                          RealT fullUpdateTime) {
    auto* concreteLts =
        dynamic_cast<seissol::initializers::LTSRateAndState<Config> const* const>(dynRup);
    a = layerData.var(concreteLts->rsA);
    sl0 = layerData.var(concreteLts->rsSl0);
    stateVariable = layerData.var(concreteLts->stateVariable);
    static_cast<Derived*>(this)->copyLtsTreeToLocal(layerData, dynRup, fullUpdateTime);
    tpMethod.copyLtsTreeToLocal(layerData, dynRup, fullUpdateTime);
  }

  /**
   * Contains all the variables, which are to be computed initially in each timestep.
   */
  struct InitialVariables {
    std::array<RealT, misc::numPaddedPoints<Config>> absoluteShearTraction{0};
    std::array<RealT, misc::numPaddedPoints<Config>> localSlipRate{0};
    std::array<RealT, misc::numPaddedPoints<Config>> normalStress{0};
    std::array<RealT, misc::numPaddedPoints<Config>> stateVarReference{0};
  };

  /*
   * Compute shear stress magnitude, localSlipRate, effective normal stress, reference state
   * variable. Also sets slipRateMagnitude member to reference value.
   */
  InitialVariables calcInitialVariables(
      FaultStresses<Config> const& faultStresses,
      std::array<RealT, misc::numPaddedPoints<Config>> const& localStateVariable,
      unsigned int timeIndex,
      unsigned int ltsFace) {
    // Careful, the state variable must always be corrected using stateVarZero and not
    // localStateVariable!
    std::array<RealT, misc::numPaddedPoints<Config>> stateVarReference;
    std::copy(localStateVariable.begin(), localStateVariable.end(), stateVarReference.begin());

    std::array<RealT, misc::numPaddedPoints<Config>> absoluteTraction;
    std::array<RealT, misc::numPaddedPoints<Config>> normalStress;
    std::array<RealT, misc::numPaddedPoints<Config>> temporarySlipRate;

    updateNormalStress(normalStress, faultStresses, timeIndex, ltsFace);
#pragma omp simd
    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
      // calculate absolute value of stress in Y and Z direction
      const RealT totalTraction1 = this->initialStressInFaultCS[ltsFace][pointIndex][3] +
                                   faultStresses.traction1[timeIndex][pointIndex];
      const RealT totalTraction2 = this->initialStressInFaultCS[ltsFace][pointIndex][5] +
                                   faultStresses.traction2[timeIndex][pointIndex];
      absoluteTraction[pointIndex] = misc::magnitude(totalTraction1, totalTraction2);

      // The following process is adapted from that described by Kaneko et al. (2008)
      this->slipRateMagnitude[ltsFace][pointIndex] = misc::magnitude(
          this->slipRate1[ltsFace][pointIndex], this->slipRate2[ltsFace][pointIndex]);
      this->slipRateMagnitude[ltsFace][pointIndex] =
          std::max(rs::almostZero(), this->slipRateMagnitude[ltsFace][pointIndex]);
      temporarySlipRate[pointIndex] = this->slipRateMagnitude[ltsFace][pointIndex];
    } // End of pointIndex-loop
    return {absoluteTraction, temporarySlipRate, normalStress, stateVarReference};
  }

  void updateStateVariableIterative(
      bool& hasConverged,
      std::array<RealT, misc::numPaddedPoints<Config>> const& stateVarReference,
      std::array<RealT, misc::numPaddedPoints<Config>>& localSlipRate,
      std::array<RealT, misc::numPaddedPoints<Config>>& localStateVariable,
      std::array<RealT, misc::numPaddedPoints<Config>>& normalStress,
      std::array<RealT, misc::numPaddedPoints<Config>> const& absoluteShearStress,
      FaultStresses<Config> const& faultStresses,
      unsigned int timeIndex,
      unsigned int ltsFace) {
    std::array<RealT, misc::numPaddedPoints<Config>> testSlipRate{0};
    for (unsigned j = 0; j < settings.numberStateVariableUpdates; j++) {
#pragma omp simd
      for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
        // fault strength using friction coefficient and fluid pressure from previous
        // timestep/iteration update state variable using sliprate from the previous time step
        localStateVariable[pointIndex] =
            static_cast<Derived*>(this)->updateStateVariable(pointIndex,
                                                             ltsFace,
                                                             stateVarReference[pointIndex],
                                                             this->deltaT[timeIndex],
                                                             localSlipRate[pointIndex]);
      }
      this->tpMethod.calcFluidPressure(normalStress,
                                       this->mu,
                                       localSlipRate,
                                       this->deltaT[timeIndex],
                                       false,
                                       timeIndex,
                                       ltsFace);

      updateNormalStress(normalStress, faultStresses, timeIndex, ltsFace);

      // solve for new slip rate
      hasConverged = this->invertSlipRateIterative(
          ltsFace, localStateVariable, normalStress, absoluteShearStress, testSlipRate);

#pragma omp simd
      for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
        // update local slip rate, now using V=(Vnew+Vold)/2
        // For the next SV update, use the mean slip rate between the initial guess and the one
        // found (Kaneko 2008, step 6)
        localSlipRate[pointIndex] = 0.5 * (this->slipRateMagnitude[ltsFace][pointIndex] +
                                           std::fabs(testSlipRate[pointIndex]));

        // solve again for Vnew
        this->slipRateMagnitude[ltsFace][pointIndex] = std::fabs(testSlipRate[pointIndex]);

        // update friction coefficient based on new state variable and slip rate
        this->mu[ltsFace][pointIndex] =
            static_cast<Derived*>(this)->updateMu(ltsFace,
                                                  pointIndex,
                                                  this->slipRateMagnitude[ltsFace][pointIndex],
                                                  localStateVariable[pointIndex]);
      } // End of pointIndex-loop
    }
  }

  void calcSlipRateAndTraction(
      std::array<RealT, misc::numPaddedPoints<Config>> const& stateVarReference,
      std::array<RealT, misc::numPaddedPoints<Config>> const& localSlipRate,
      std::array<RealT, misc::numPaddedPoints<Config>>& localStateVariable,
      std::array<RealT, misc::numPaddedPoints<Config>> const& normalStress,
      std::array<RealT, misc::numPaddedPoints<Config>> const& absoluteTraction,
      FaultStresses<Config> const& faultStresses,
      TractionResults<Config>& tractionResults,
      unsigned int timeIndex,
      unsigned int ltsFace) {
#pragma omp simd
    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
      // SV from mean slip rate in tmp
      localStateVariable[pointIndex] =
          static_cast<Derived*>(this)->updateStateVariable(pointIndex,
                                                           ltsFace,
                                                           stateVarReference[pointIndex],
                                                           this->deltaT[timeIndex],
                                                           localSlipRate[pointIndex]);

      // update LocMu for next strength determination, only needed for last update
      this->mu[ltsFace][pointIndex] =
          static_cast<Derived*>(this)->updateMu(ltsFace,
                                                pointIndex,
                                                this->slipRateMagnitude[ltsFace][pointIndex],
                                                localStateVariable[pointIndex]);
      const RealT strength = -this->mu[ltsFace][pointIndex] * normalStress[pointIndex];
      // calculate absolute value of stress in Y and Z direction
      const RealT totalTraction1 = this->initialStressInFaultCS[ltsFace][pointIndex][3] +
                                   faultStresses.traction1[timeIndex][pointIndex];
      const RealT totalTraction2 = this->initialStressInFaultCS[ltsFace][pointIndex][5] +
                                   faultStresses.traction2[timeIndex][pointIndex];
      // update stress change
      this->traction1[ltsFace][pointIndex] =
          (totalTraction1 / absoluteTraction[pointIndex]) * strength -
          this->initialStressInFaultCS[ltsFace][pointIndex][3];
      this->traction2[ltsFace][pointIndex] =
          (totalTraction2 / absoluteTraction[pointIndex]) * strength -
          this->initialStressInFaultCS[ltsFace][pointIndex][5];

      // Compute slip
      // ABS of locSlipRate removed as it would be the accumulated slip that is usually not needed
      // in the solver, see linear slip weakening
      this->accumulatedSlipMagnitude[ltsFace][pointIndex] +=
          this->slipRateMagnitude[ltsFace][pointIndex] * this->deltaT[timeIndex];

      // Update slip rate (notice that locSlipRate(T=0)=-2c_s/mu*s_xy^{Godunov} is the slip rate
      // caused by a free surface!)
      this->slipRate1[ltsFace][pointIndex] =
          -this->impAndEta[ltsFace].invEtaS *
          (this->traction1[ltsFace][pointIndex] - faultStresses.traction1[timeIndex][pointIndex]);
      this->slipRate2[ltsFace][pointIndex] =
          -this->impAndEta[ltsFace].invEtaS *
          (this->traction2[ltsFace][pointIndex] - faultStresses.traction2[timeIndex][pointIndex]);

      // correct slipRate1 and slipRate2 to avoid numerical errors
      const RealT locSlipRateMagnitude = misc::magnitude(this->slipRate1[ltsFace][pointIndex],
                                                         this->slipRate2[ltsFace][pointIndex]);
      if (locSlipRateMagnitude != 0) {
        this->slipRate1[ltsFace][pointIndex] *=
            this->slipRateMagnitude[ltsFace][pointIndex] / locSlipRateMagnitude;
        this->slipRate2[ltsFace][pointIndex] *=
            this->slipRateMagnitude[ltsFace][pointIndex] / locSlipRateMagnitude;
      }

      // Save traction for flux computation
      tractionResults.traction1[timeIndex][pointIndex] = this->traction1[ltsFace][pointIndex];
      tractionResults.traction2[timeIndex][pointIndex] = this->traction2[ltsFace][pointIndex];

      // update directional slip
      this->slip1[ltsFace][pointIndex] +=
          this->slipRate1[ltsFace][pointIndex] * this->deltaT[timeIndex];
      this->slip2[ltsFace][pointIndex] +=
          this->slipRate2[ltsFace][pointIndex] * this->deltaT[timeIndex];
    } // End of BndGP-loop
  }

  void saveDynamicStressOutput(unsigned int face) {
#pragma omp simd
    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {

      if (this->ruptureTime[face][pointIndex] > 0.0 &&
          this->ruptureTime[face][pointIndex] <= this->mFullUpdateTime &&
          this->dynStressTimePending[face][pointIndex] &&
          this->mu[face][pointIndex] <=
              (this->drParameters->muW +
               0.05 * (this->drParameters->rsF0 - this->drParameters->muW))) {
        this->dynStressTime[face][pointIndex] = this->mFullUpdateTime;
        this->dynStressTimePending[face][pointIndex] = false;
      }
    }
  }

  /**
   * Solve for new slip rate (\f$\hat{s}\f$), applying the Newton-Raphson algorithm.
   * \f$\hat{s}\f$ has to fulfill
   * \f[g := \frac{1}{\eta_s} \cdot (\sigma_n \cdot \mu - \Theta) - \hat{s} = 0.\f] c.f. Carsten
   * Uphoff's dissertation eq. (4.57). Find root of \f$g\f$ with \f$g^\prime = \partial g / \partial
   * \hat{s}\f$: \f$\hat{s}_{i+1} = \hat{s}_i - ( g_i / g^\prime_i )\f$
   * @param ltsFace index of the face for which we invert the sliprate
   * @param localStateVariable \f$\psi\f$, needed to compute \f$\mu = f(\hat{s}, \psi)\f$
   * @param normalStress \f$\sigma_n\f$
   * @param absoluteShearStress \f$\Theta\f$
   * @param slipRateTest \f$\hat{s}\f$
   */
  bool invertSlipRateIterative(
      unsigned int ltsFace,
      std::array<RealT, misc::numPaddedPoints<Config>> const& localStateVariable,
      std::array<RealT, misc::numPaddedPoints<Config>> const& normalStress,
      std::array<RealT, misc::numPaddedPoints<Config>> const& absoluteShearStress,
      std::array<RealT, misc::numPaddedPoints<Config>>& slipRateTest) {
    // Note that we need double precision here, since single precision led to NaNs.
    double muF[misc::numPaddedPoints<Config>], dMuF[misc::numPaddedPoints<Config>];
    double g[misc::numPaddedPoints<Config>], dG[misc::numPaddedPoints<Config>];

    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
      // first guess = sliprate value of the previous step
      slipRateTest[pointIndex] = this->slipRateMagnitude[ltsFace][pointIndex];
    }

    for (unsigned i = 0; i < settings.maxNumberSlipRateUpdates; i++) {
#pragma omp simd
      for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
        // calculate friction coefficient and objective function
        muF[pointIndex] = static_cast<Derived*>(this)->updateMu(
            ltsFace, pointIndex, slipRateTest[pointIndex], localStateVariable[pointIndex]);
        dMuF[pointIndex] = static_cast<Derived*>(this)->updateMuDerivative(
            ltsFace, pointIndex, slipRateTest[pointIndex], localStateVariable[pointIndex]);
        g[pointIndex] = -this->impAndEta[ltsFace].invEtaS *
                            (std::fabs(normalStress[pointIndex]) * muF[pointIndex] -
                             absoluteShearStress[pointIndex]) -
                        slipRateTest[pointIndex];
      }

      // max element of g must be smaller than newtonTolerance
      const bool hasConverged = std::all_of(std::begin(g), std::end(g), [&](auto val) {
        return std::fabs(val) < settings.newtonTolerance;
      });
      if (hasConverged) {
        return hasConverged;
      }
#pragma omp simd
      for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {

        // derivative of g
        dG[pointIndex] = -this->impAndEta[ltsFace].invEtaS *
                             (std::fabs(normalStress[pointIndex]) * dMuF[pointIndex]) -
                         1.0;
        // newton update
        const RealT tmp3 = g[pointIndex] / dG[pointIndex];
        slipRateTest[pointIndex] = std::max(rs::almostZero(), slipRateTest[pointIndex] - tmp3);
      }
    }
    return false;
  }

  void updateNormalStress(std::array<RealT, misc::numPaddedPoints<Config>>& normalStress,
                          FaultStresses<Config> const& faultStresses,
                          size_t timeIndex,
                          size_t ltsFace) {
#pragma omp simd
    for (size_t pointIndex = 0; pointIndex < misc::numPaddedPoints<Config>; pointIndex++) {
      normalStress[pointIndex] = std::min(static_cast<RealT>(0.0),
                                          faultStresses.normalStress[timeIndex][pointIndex] +
                                              this->initialStressInFaultCS[ltsFace][pointIndex][0] -
                                              this->tpMethod.getFluidPressure(ltsFace, pointIndex));
    }
  }

  protected:
  // Attributes
  RealT (*a)[misc::numPaddedPoints<Config>];
  RealT (*sl0)[misc::numPaddedPoints<Config>];
  RealT (*stateVariable)[misc::numPaddedPoints<Config>];

  TPMethod tpMethod;
  rs::Settings settings{};
};

} // namespace seissol::dr::friction_law
#endif // SEISSOL_RATEANDSTATE_H
