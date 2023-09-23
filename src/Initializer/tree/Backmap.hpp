#pragma once

#include <cstddef>
#include <limits>
#include <utility>
#include <array>
#include <vector>
#include <cassert>

namespace seissol::initializers {
template <std::size_t MaxDuplicates, typename ColorMap>
class StorageBackmap {
  public:
  using StoragePosition = std::pair<int, unsigned>;
  const inline static StoragePosition NullPosition =
      StoragePosition(-1, std::numeric_limits<unsigned>::max());

  template <typename F>
  void lookup(F&& handler, unsigned id, unsigned duplicate = 0) const {
    assert(duplicate < MaxDuplicates);
    auto position = layerPosition(id, duplicate);
    map.call(position.first, std::forward<F>(handler)); // TODO: forward position.second
  }

  StoragePosition layerPosition(unsigned id, unsigned duplicate = 0) const {
    assert(duplicate < MaxDuplicates);
    return data[id][duplicate];
  }

  template <typename TRef>
  void addElement(int color, const TRef* zeroPosition, const TRef* layerData, int cell, int index) {
    auto position = static_cast<std::size_t>((layerData + index) - zeroPosition) / sizeof(TRef);
    auto storagePosition = StoragePosition(color, position);
    std::size_t j = 0;
    for (j = 0; j < MaxDuplicates; ++j) {
      if (data[cell][j] == NullPosition) {
        data[cell][j] = storagePosition;
        break;
      }
    }
    assert(j < MaxDuplicates);
  }

  template <typename TRef>
  void addLayer(int color,
                const TRef* zeroPosition,
                const TRef* layerData,
                const std::vector<int>& cells) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
      addElement(color, zeroPosition, layerData, cells[i], i);
    }
  }

  private:
  using CellStoragePosition = std::array<StoragePosition, MaxDuplicates>;

  const inline static std::vector<StoragePosition> PreNullCellStoragePosition =
      std::vector<StoragePosition>(MaxDuplicates, NullPosition);
  const inline static CellStoragePosition NullCellStoragePosition =
      CellStoragePosition(PreNullCellStoragePosition.begin(), PreNullCellStoragePosition.end());
  // TODO(David): initialize with NullPosition
  std::vector<CellStoragePosition> data = std::vector<CellStoragePosition>(NullCellStoragePosition);

  ColorMap map;
};
} // namespace seissol::initializers