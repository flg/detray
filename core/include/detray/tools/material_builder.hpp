/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2022-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s).
#include "detray/tools/material_factory.hpp"
#include "detray/tools/volume_builder.hpp"
#include "detray/tools/volume_builder_interface.hpp"

// System include(s)
#include <memory>
#include <stdexcept>
#include <vector>

namespace detray {

/// @brief Build a volume containing surfaces with material.
///
/// Decorator class to a volume builder that adds the material data to the
/// surfaces while building the volume.
template <typename detector_t>
class material_builder final : public volume_decorator<detector_t> {

    public:
    using material_id = typename detector_t::materials::id;
    using scalar_type = typename detector_t::scalar_type;

    /// @param vol_builder volume builder that should be decorated with material
    DETRAY_HOST
    material_builder(
        std::unique_ptr<volume_builder_interface<detector_t>> vol_builder)
        : volume_decorator<detector_t>(std::move(vol_builder)) {}

    /// Overwrite, to add material in addition to surfaces (only if surfaces are
    /// present in the factory, otherwise only add material)
    /// @{
    DETRAY_HOST
    void add_portals(
        std::shared_ptr<surface_factory_interface<detector_t>> pt_factory,
        typename detector_t::geometry_context ctx = {}) override {

        // If the factory also holds surface data, call base volume builder
        volume_decorator<detector_t>::add_portals(pt_factory, ctx);

        // Add material
        auto mat_factory =
            std::dynamic_pointer_cast<material_factory<detector_t>>(pt_factory);
        if (mat_factory) {
            (*mat_factory)(this->surfaces(), m_materials);
        } else {
            throw std::runtime_error("Not a material factory");
        }
    }

    DETRAY_HOST
    void add_sensitives(
        std::shared_ptr<surface_factory_interface<detector_t>> sf_factory,
        typename detector_t::geometry_context ctx = {}) override {

        // If the factory also holds surface data, call base volume builder
        volume_decorator<detector_t>::add_sensitives(sf_factory, ctx);

        // Add material
        auto mat_factory =
            std::dynamic_pointer_cast<material_factory<detector_t>>(sf_factory);
        if (mat_factory) {
            (*mat_factory)(this->surfaces(), m_materials);
        } else {
            throw std::runtime_error("Not a material factory");
        }
    }

    DETRAY_HOST
    void add_passives(
        std::shared_ptr<surface_factory_interface<detector_t>> ps_factory,
        typename detector_t::geometry_context ctx = {}) override {

        // If the factory also holds surface data, call base volume builder
        volume_decorator<detector_t>::add_passives(ps_factory, ctx);

        // Add material
        auto mat_factory =
            std::dynamic_pointer_cast<material_factory<detector_t>>(ps_factory);
        if (mat_factory) {
            (*mat_factory)(this->surfaces(), m_materials);
        } else {
            throw std::runtime_error("Not a material factory");
        }
    }
    /// @}

    /// Add the volume and the material to the detector @param det
    DETRAY_HOST
    auto build(detector_t &det, typename detector_t::geometry_context ctx = {})
        -> typename detector_t::volume_type * override {
        // This builder is only called on a homogeneous material description
        using mat_types = typename detector_t::material_container::value_types;

        const auto &material = det.material_store();

        // Update the surface material links and shift them according to the
        // number of material slabs/rods that were in the detector previously
        for (auto &sf : this->surfaces()) {
            if (sf.material().id() == material_id::e_slab) {
                sf.update_material(
                    material.template size<material_id::e_slab>());
            }
            if constexpr (mat_types::n_types == 2u) {
                if (sf.material().id() == material_id::e_rod) {
                    sf.update_material(
                        material.template size<material_id::e_rod>());
                }
            }
        }

        // Add material to the detector
        det.append_materials(std::move(m_materials));
        m_materials.clear_all();

        // Call the underlying volume builder(s) and give the volume to the
        // next decorator
        return volume_decorator<detector_t>::build(det, ctx);
    }

    protected:
    // Material container for this volume
    typename detector_t::material_container m_materials{};
};

}  // namespace detray