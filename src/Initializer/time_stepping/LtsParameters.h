#ifndef SEISSOL_LTSCONFIGURATION_H
#define SEISSOL_LTSCONFIGURATION_H

#include <memory>
#include <yaml-cpp/yaml.h>

namespace seissol::initializers::time_stepping {

enum class ClusterMergingCostModel {
  // TODO(Lukas) Need better names.
  // Use cost without wiggle and cluster merge as baseline
  CostComputedFromBaseline,
  // First find best wiggle factor (without merge) and use this as baseline
  CostComputedFromBestWiggleFactor,
};

class LtsParameters {
  private:
  unsigned int rate;
  double wiggleFactorMinimum;
  double wiggleFactorStepsize;
  bool wiggleFactorEnforceMaximumDifference;
  unsigned int maxNumberOfClusters;
  bool autoMergeClusters;
  double allowedPerformanceLossRatioAutoMerge;
  ClusterMergingCostModel clusterMergingCostModel = ClusterMergingCostModel::CostComputedFromBestWiggleFactor;

  public:
  [[nodiscard]] unsigned int getRate() const;
  [[nodiscard]] bool isWiggleFactorUsed() const;
  [[nodiscard]] double getWiggleFactorMinimum() const;
  [[nodiscard]] double getWiggleFactorStepsize() const;
  [[nodiscard]] bool getWiggleFactorEnforceMaximumDifference() const;
  [[nodiscard]] int getMaxNumberOfClusters() const;
  [[nodiscard]] bool isAutoMergeUsed() const;
  [[nodiscard]] double getAllowedPerformanceLossRatioAutoMerge() const;
  [[nodiscard]] ClusterMergingCostModel getClusterMergingCostModel() const;

  LtsParameters(unsigned int rate,
                double wiggleFactorMinimum,
                double wiggleFactorStepsize,
                bool wigleFactorEnforceMaximumDifference,
                int maxNumberOfClusters,
                bool ltsAutoMergeClusters,
                double allowedPerformanceLossRatioAutoMerge);
};

LtsParameters readLtsParametersFromYaml(std::shared_ptr<YAML::Node>& params);

} // namespace seissol::initializers::time_stepping

#endif // SEISSOL_LTSCONFIGURATION_H
