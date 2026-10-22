#ifndef CHOUPI_PC_ROMMASK_HPP
#define CHOUPI_PC_ROMMASK_HPP

#ifdef PC_VERSION

#include <cstddef>
#include <cstdint>
#include <vector>

namespace choupi::pc {

struct RomPackage {
  uint8_t package_id = 0;
  std::vector<uint8_t> aid;
  std::vector<uint8_t> components;
};

bool parse_rommask_packages(const uint8_t *image, size_t image_length,
                            std::vector<RomPackage> &packages);

} // namespace choupi::pc

#endif // PC_VERSION

#endif // CHOUPI_PC_ROMMASK_HPP
