/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s)
#include "detray/definitions/detail/accessor.hpp"
#include "detray/definitions/qualifiers.hpp"

namespace detray {

template <typename transform3_t, template <class> typename intersector_t,
          template <class> typename local_t, typename links_t,
          template <typename, std::size_t> class array_t, std::size_t kDIM>
class mask_base {
    public:
    using intersector_type = intersector_t<transform3_t>;
    using local_type = local_t<transform3_t>;
    using links_type = links_t;

    template <typename T, std::size_t I>
    using array_type = array_t<T, I>;
    using mask_values = array_type<scalar, kDIM>;

    /* Default constructor */
    mask_base() = default;

    DETRAY_HOST_DEVICE
    mask_base(const mask_values& val, const links_type& links)
        : _values(val), _links(links) {}

    DETRAY_HOST_DEVICE
    mask_base& operator=(const mask_values& rhs) {
        this->_values = rhs;
        return (*this);
    }

    /** Equality operator from an array, convenience function
     *
     * @param rhs is the rectangle to be compared with
     *
     * checks identity within epsilon and @return s a boolean*
     **/
    DETRAY_HOST_DEVICE
    bool operator==(const mask_values& rhs) { return (_values == rhs); }

    /** Equality operator
     *
     * @param rhs is the rectangle to be compared with
     *
     * checks identity within epsilon and @return s a boolean*
     **/
    DETRAY_HOST_DEVICE
    bool operator==(const mask_base& rhs) {
        return (_values == rhs._values && _links == rhs._links);
    }

    /** Access operator - non-const
     * @return the reference to the member variable
     */
    DETRAY_HOST_DEVICE
    scalar& operator[](std::size_t value_index) { return _values[value_index]; }

    /** Access operator - const
     * @return a copy of the member variable
     */
    DETRAY_HOST_DEVICE
    scalar operator[](std::size_t value_index) const {
        return _values[value_index];
    }

    /** Return the values */
    DETRAY_HOST_DEVICE
    const mask_values& values() const { return _values; }

    /// Return an associated intersector type
    DETRAY_HOST_DEVICE
    intersector_type intersector() const { return intersector_type{}; };

    /// Return the local frame type
    DETRAY_HOST_DEVICE
    constexpr local_type local() const { return local_type{}; }

    /// @return the links - const reference
    DETRAY_HOST_DEVICE
    const links_type& links() const { return _links; }

    /// @return the volume link - const reference
    DETRAY_HOST_DEVICE
    auto volume_link() const { return detail::get<0>(_links); }

    /// @return the volume link - non-const access
    DETRAY_HOST_DEVICE
    auto volume_link() { return detail::get<0>(_links); }

    /// @return the surface finder link - const reference
    DETRAY_HOST_DEVICE
    auto finder_link() const { return detail::get<1>(_links); }

    /// @return the surface finder link - non-const access
    DETRAY_HOST_DEVICE
    auto finder_link() { return detail::get<1>(_links); }

    protected:
    mask_values _values;
    links_type _links;
};

}  // namespace detray