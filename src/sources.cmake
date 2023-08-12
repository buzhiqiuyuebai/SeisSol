# Source code
add_library(SeisSol-lib

# do YATeTo first, since kernel.cpp usually takes really long

# kernel.cpp usually takes the longest
# (for CPUs, at least; for GPUs, we have a different library alltogether)
${CMAKE_CURRENT_BINARY_DIR}/src/generated_code/kernel.cpp
${CMAKE_CURRENT_BINARY_DIR}/src/generated_code/tensor.cpp
${CMAKE_CURRENT_BINARY_DIR}/src/generated_code/subroutine.cpp
${CMAKE_CURRENT_BINARY_DIR}/src/generated_code/init.cpp

src/Initializer/ParameterDB.cpp
src/Initializer/PointMapper.cpp
src/Initializer/GlobalData.cpp
src/Initializer/InternalState.cpp
src/Initializer/MemoryAllocator.cpp
src/Initializer/CellLocalMatrices.cpp

src/Initializer/time_stepping/LtsLayout.cpp
src/Initializer/time_stepping/LtsParameters.cpp
src/Initializer/time_stepping/Timestep.cpp
src/Initializer/tree/Lut.cpp
src/Initializer/MemoryManager.cpp
src/Initializer/InitialFieldProjection.cpp
src/Initializer/InputParameters.cpp

src/Initializer/ConfigFile.cpp
src/Initializer/ParameterMaterialDB.cpp

src/Initializer/InitProcedure/InitMesh.cpp
src/Initializer/InitProcedure/InitModel.cpp
src/Initializer/InitProcedure/InitIO.cpp
src/Initializer/InitProcedure/InitSideConditions.cpp
src/Initializer/InitProcedure/Init.cpp

src/Modules/Modules.cpp
src/Model/common.cpp
src/Numerical_aux/Functions.cpp
src/Numerical_aux/Transformation.cpp
src/Numerical_aux/Statistics.cpp

src/Solver/Simulator.cpp
src/Solver/FreeSurfaceIntegrator.cpp

src/Solver/time_stepping/AbstractTimeCluster.cpp
src/Solver/time_stepping/ActorState.cpp
src/Solver/time_stepping/MiniSeisSol.cpp
src/Solver/time_stepping/TimeCluster.cpp
src/Solver/time_stepping/AbstractGhostTimeCluster.cpp
src/Solver/time_stepping/DirectGhostTimeCluster.cpp
src/Solver/time_stepping/GhostTimeClusterWithCopy.cpp
src/Solver/time_stepping/CommunicationManager.cpp

src/Solver/time_stepping/TimeManager.cpp
src/Solver/Pipeline/DrTuner.cpp
src/Kernels/DynamicRupture.cpp
src/Kernels/Plasticity.cpp
src/Kernels/TimeCommon.cpp
src/Kernels/Receiver.cpp
src/SeisSol.cpp
src/SourceTerm/Manager.cpp
src/SourceTerm/FSRMReader.cpp
src/SourceTerm/PointSource.cpp
src/Parallel/Pin.cpp
src/Parallel/mpiC.cpp
src/Parallel/FaultMPI.cpp

src/Geometry/MeshTools.cpp
src/Geometry/MeshReader.cpp
src/Monitoring/FlopCounter.cpp
src/Monitoring/LoopStatistics.cpp

src/Checkpoint/Manager.cpp

src/Checkpoint/Backend.cpp
src/Checkpoint/Fault.cpp
src/Checkpoint/posix/Wavefield.cpp
src/Checkpoint/posix/Fault.cpp
src/ResultWriter/AnalysisWriter.cpp
src/ResultWriter/MiniSeisSolWriter.cpp
src/ResultWriter/ClusteringWriter.cpp
src/ResultWriter/ThreadsPinningWriter.cpp
src/ResultWriter/FreeSurfaceWriterExecutor.cpp
src/ResultWriter/PostProcessor.cpp
src/ResultWriter/ReceiverWriter.cpp
src/ResultWriter/FaultWriterExecutor.cpp
src/ResultWriter/FaultWriter.cpp
src/ResultWriter/WaveFieldWriter.cpp
src/ResultWriter/FreeSurfaceWriter.cpp
src/ResultWriter/EnergyOutput.cpp

src/Numerical_aux/ODEInt.cpp
src/Numerical_aux/ODEVector.cpp
src/Physics/InitialField.cpp

src/Equations/poroelastic/Model/datastructures.cpp
src/Equations/elastic/Kernels/GravitationalFreeSurfaceBC.cpp

src/Common/IntegerMaskParser.cpp

src/WavePropagation/dispatcher.cpp
)

set(SYCL_DEPENDENT_SRC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Model/common.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Parallel/MPI.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/FrictionSolver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Misc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/SourceTimeFunction.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/LinearSlipWeakening.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/NoFault.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/ThermalPressurization/ThermalPressurization.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Initializers/BaseDRInitializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Initializers/ImposedSlipRatesInitializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Initializers/LinearSlipWeakeningInitializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Initializers/RateAndStateInitializer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Output/OutputManager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Output/ReceiverBasedOutput.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Output/FaultRefiner/FaultRefiners.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Output/OutputAux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/Output/Builders/ReceiverBasedOutputBuilder.cpp)

set(SYCL_ONLY_SRC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Parallel/AcceleratorDevice.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamicRupture/FrictionLaws/GpuImpl/FrictionSolverDetails.cpp)

target_compile_options(SeisSol-common-properties INTERFACE ${EXTRA_CXX_FLAGS})
target_include_directories(SeisSol-common-properties INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/src/generated_code)

if (MPI)
  target_sources(SeisSol-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/mpio/Wavefield.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/mpio/FaultAsync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/mpio/Fault.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/mpio/WavefieldAsync.cpp
)
endif()

if (HDF5)
  target_sources(SeisSol-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/h5/Wavefield.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Checkpoint/h5/Fault.cpp
    )
endif()

if (HDF5 AND MPI)
  target_sources(SeisSol-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/PartitioningLib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/PUMLReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/time_stepping/LtsWeights/LtsWeights.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/time_stepping/LtsWeights/WeightsModels.cpp
    )
endif()


if (NETCDF)
  target_sources(SeisSol-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SourceTerm/NRFReader.cpp # if netCDF
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/NetcdfReader.cpp
    )
endif()

if (ASAGI)
  target_sources(SeisSol-lib PRIVATE
    #todo:
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Reader/AsagiModule.cpp
    )
endif()

# material sources (to be consolidated)
target_sources(SeisSol-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Equations/elastic/Kernels/DirichletBoundary.cpp
)

# Eqations have to be set at compile time currently (soon-to-make obsolete)
if ("${EQUATIONS}" STREQUAL "elastic")
  target_compile_definitions(SeisSol-common-properties INTERFACE USE_ELASTIC)
elseif ("${EQUATIONS}" STREQUAL "viscoelastic2")
  target_compile_definitions(SeisSol-common-properties INTERFACE USE_VISCOELASTIC2)
elseif ("${EQUATIONS}" STREQUAL "anisotropic")
  target_compile_definitions(SeisSol-common-properties INTERFACE USE_ANISOTROPIC)
elseif ("${EQUATIONS}" STREQUAL "poroelastic")  
  target_compile_definitions(SeisSol-common-properties INTERFACE USE_STP)
  target_compile_definitions(SeisSol-common-properties INTERFACE USE_POROELASTIC)
endif()

target_include_directories(SeisSol-common-properties INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/BatchRecorders)


# GPU code
if (WITH_GPU)
  target_sources(SeisSol-lib PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/BatchRecorders/LocalIntegrationRecorder.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/BatchRecorders/NeighIntegrationRecorder.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/BatchRecorders/PlasticityRecorder.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/Initializer/BatchRecorders/DynamicRuptureRecorder.cpp)


  set(SEISSOL_DEVICE_INCLUDE ${DEVICE_INCLUDE_DIRS}
                             ${CMAKE_CURRENT_SOURCE_DIR}/submodules/yateto/include
                             ${CMAKE_BINARY_DIR}/src/generated_code
                             ${CMAKE_CURRENT_SOURCE_DIR}/src)

  # include cmake files will define SeisSol-device-lib target
  if ("${DEVICE_BACKEND}" STREQUAL "cuda")
    include(${CMAKE_SOURCE_DIR}/src/cuda.cmake)
  elseif ("${DEVICE_BACKEND}" STREQUAL "hip")
    include(${CMAKE_SOURCE_DIR}/src/hip.cmake)
  elseif ("${DEVICE_BACKEND}" STREQUAL "hipsycl" OR "${DEVICE_BACKEND}" STREQUAL "oneapi")
    include(${CMAKE_SOURCE_DIR}/src/sycl.cmake)
  endif()

  target_compile_options(SeisSol-device-lib PRIVATE -fPIC)
  target_include_directories(SeisSol-lib PRIVATE ${DEVICE_INCLUDE_DIRS})

  if ("${EQUATIONS}" STREQUAL "elastic")
    target_compile_definitions(SeisSol-device-lib PRIVATE USE_ELASTIC)
  endif()
endif()
