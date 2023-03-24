
#ifndef GLOBAL_TIMESTEP_HPP_
#define GLOBAL_TIMESTEP_HPP_
#include <vector>
#include <string>

#include "Initializer/ParameterDB.h"

namespace seissol::initializer {
    struct GlobalTimestep {
        std::vector<double> elementTimeStep;
        double globalMinTimeStep;
        double globalMaxTimeStep;
    };

    GlobalTimestep computeTimesteps(double cfl, double maximumAllowedTimeStep, const std::string& velocityModel, const seissol::initializers::C2VArray& ctov);
}

#endif
