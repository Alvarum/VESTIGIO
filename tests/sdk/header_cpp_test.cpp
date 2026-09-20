#include "vestigio/vestigio.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout<VgContextDesc>::value, "Context descriptor must be POD-like");
static_assert(std::is_standard_layout<VgTransform>::value, "Transform must be POD-like");
static_assert(sizeof(VgAsset) == sizeof(std::uint64_t), "Asset handle must be 64-bit");

int main() {
    VgContextDesc description{};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    return description.api_version == 1u ? 0 : 1;
}
