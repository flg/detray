/** Detray library, part of the ACTS project
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s).
#include "detray/coordinates/coordinate_base.hpp"
#include "detray/definitions/qualifiers.hpp"

namespace detray {

template <typename transform3_t>
struct line2 : public coordinate_base<line2, transform3_t> {

    /// @name Type definitions for the struct
    /// @{

    // Base type
    using base_type = coordinate_base<line2, transform3_t>;
    // Sclar type
    using scalar_type = typename base_type::scalar_type;
    // Point in 2D space
    using point2 = typename base_type::point2;
    // Point in 3D space
    using point3 = typename base_type::point3;
    // Vector in 3D space
    using vector3 = typename base_type::vector3;
    // Matrix actor
    using matrix_operator = typename base_type::matrix_operator;
    // Matrix size type
    using size_type = typename base_type::size_type;
    // 2D matrix type
    template <size_type ROWS, size_type COLS>
    using matrix_type = typename base_type::template matrix_type<ROWS, COLS>;
    // Rotation Matrix
    using rotation_matrix = typename base_type::rotation_matrix;
    // Vector types
    using bound_vector = typename base_type::bound_vector;
    using free_vector = typename base_type::free_vector;
    // Track Helper
    using track_helper = typename base_type::track_helper;
    // Matrix types
    using free_to_bound_matrix = typename base_type::free_to_bound_matrix;
    using bound_to_free_matrix = typename base_type::bound_to_free_matrix;

    /// @}

    /** This method returns the measurement **/
    template <typename smearer_t>
    DETRAY_HOST_DEVICE inline point2 get_measurement(
        const bound_track_parameters<transform3_t> &param, smearer_t &smearer) {
        auto loc = param.local();
        loc[0] = smearer.template get<0>() + std::abs(loc[0]);
        if (loc[0] < 0) {
            loc[0] = 0;
        }
        loc[1] = 0;
        return loc;
    }

    /** This method transform from a point from 3D cartesian frame to a 2D
     * line point */
    DETRAY_HOST_DEVICE
    inline point2 operator()(const point3 &local3, scalar_type sign) const {

        return {sign * getter::perp(local3), local3[2]};
    }

    /** This method transform from a point from global cartesian 3D frame to a
     * local 2D line point */
    DETRAY_HOST_DEVICE
    inline point2 global_to_local(const transform3_t &trf, const point3 &p,
                                  const vector3 &d) const {

        const auto local3 = trf.point_to_local(p);

        // Line direction
        const vector3 z = trf.z();

        // Line center
        const point3 t = trf.translation();

        // Radial vector
        const vector3 r = vector::cross(z, d);

        // Assign the sign depending on the position w.r.t line
        // Right: -1
        // Left: 1
        const scalar_type sign =
            vector::dot(r, t - p) > 0. ? scalar_type{-1.} : scalar_type{1.};

        return this->operator()(local3, sign);
    }

    /** This method transform from a local 2D line point to a point global
     * cartesian 3D frame*/
    template <typename mask_t>
    DETRAY_HOST_DEVICE inline point3 local_to_global(const transform3_t &trf,
                                                     const mask_t & /*mask*/,
                                                     const point2 &p,
                                                     const vector3 &d) const {

        // Line direction
        const vector3 z = trf.z();

        // Radial vector
        const vector3 r = vector::cross(z, d);

        // Local Z poisition in global cartesian coordinate
        const point3 locZ_in_global = trf.point_to_global(point3{0., 0., p[1]});

        return locZ_in_global + p[0] * vector::normalize(r);
    }

    template <typename mask_t>
    DETRAY_HOST_DEVICE inline rotation_matrix reference_frame(
        const transform3_t &trf3, const mask_t & /*mask*/,
        const point3 & /*pos*/, const vector3 &dir) const {

        rotation_matrix rot = matrix_operator().template zero<3, 3>();

        // y axis of the new frame is the z axis of line coordinate
        const auto new_yaxis =
            matrix_operator().template block<3, 1>(trf3.matrix(), 0, 2);

        // x axis of the new frame is (yaxis x track direction)
        auto new_xaxis = vector::cross(new_yaxis, dir);
        new_xaxis = vector::normalize(new_xaxis);

        // z axis
        const auto new_zaxis = vector::cross(new_xaxis, new_yaxis);

        matrix_operator().element(rot, 0, 0) = new_xaxis[0];
        matrix_operator().element(rot, 1, 0) = new_xaxis[1];
        matrix_operator().element(rot, 2, 0) = new_xaxis[2];
        matrix_operator().template set_block<3, 1>(rot, new_yaxis, 0, 1);
        matrix_operator().element(rot, 0, 2) = new_zaxis[0];
        matrix_operator().element(rot, 1, 2) = new_zaxis[1];
        matrix_operator().element(rot, 2, 2) = new_zaxis[2];

        return rot;
    }

    template <typename mask_t>
    DETRAY_HOST_DEVICE inline void set_bound_pos_to_free_pos_derivative(
        bound_to_free_matrix &free_to_bound_jacobian, const transform3_t &trf3,
        const mask_t &mask, const point3 &pos, const vector3 &dir) const {

        const auto frame = reference_frame(trf3, mask, pos, dir);

        // Get d(x,y,z)/d(loc0, loc1)
        const auto bound_pos_to_free_pos_derivative =
            matrix_operator().template block<3, 2>(frame, 0, 0);

        matrix_operator().template set_block(free_to_bound_jacobian,
                                             bound_pos_to_free_pos_derivative,
                                             e_free_pos0, e_bound_loc0);
    }

    template <typename mask_t>
    DETRAY_HOST_DEVICE inline void set_free_pos_to_bound_pos_derivative(
        free_to_bound_matrix &bound_to_free_jacobian, const transform3_t &trf3,
        const mask_t &mask, const point3 &pos, const vector3 &dir) const {

        const auto frame = reference_frame(trf3, mask, pos, dir);
        const auto frameT = matrix_operator().transpose(frame);

        // Get d(loc0, loc1)/d(x,y,z)
        const auto free_pos_to_bound_pos_derivative =
            matrix_operator().template block<2, 3>(frameT, 0, 0);

        matrix_operator().template set_block(bound_to_free_jacobian,
                                             free_pos_to_bound_pos_derivative,
                                             e_bound_loc0, e_free_pos0);
    }

    template <typename mask_t>
    DETRAY_HOST_DEVICE inline void set_bound_angle_to_free_pos_derivative(
        bound_to_free_matrix &bound_to_free_jacobian, const transform3_t &trf3,
        const mask_t &mask, const point3 &pos, const vector3 &dir) const {

        // local2
        const auto local2 = this->global_to_local(trf3, pos, dir);

        // Reference frame
        const auto frame = reference_frame(trf3, mask, pos, dir);

        // new x_axis
        const auto new_xaxis = getter::vector<3>(frame, 0, 0);

        // new y_axis
        const auto new_yaxis = getter::vector<3>(frame, 0, 1);

        // new z_axis
        const auto new_zaxis = getter::vector<3>(frame, 0, 2);

        // the projection of direction onto ref frame normal
        scalar_type ipdn = 1. / vector::dot(dir, new_zaxis);

        // d(n_x,n_y,n_z)/dPhi
        const auto dNdPhi = matrix_operator().template block<3, 1>(
            bound_to_free_jacobian, e_free_dir0, e_bound_phi);

        // Get new_yaxis X d(n_x,n_y,n_z)/dPhi
        auto y_cross_dNdPhi = vector::cross(new_yaxis, dNdPhi);

        // d(n_x,n_y,n_z)/dTheta
        const auto dNdTheta = matrix_operator().template block<3, 1>(
            bound_to_free_jacobian, e_free_dir0, e_bound_theta);

        // build the cross product of d(D)/d(eBoundPhi) components with y axis
        auto y_cross_dNdTheta = vector::cross(new_yaxis, dNdTheta);

        const scalar_type C = ipdn * local2[0];
        // and correct for the x axis components
        vector3 phi_to_free_pos_derivative =
            y_cross_dNdPhi - new_xaxis * vector::dot(new_xaxis, y_cross_dNdPhi);

        phi_to_free_pos_derivative = C * phi_to_free_pos_derivative;

        vector3 theta_to_free_pos_derivative =
            y_cross_dNdTheta -
            new_xaxis * vector::dot(new_xaxis, y_cross_dNdTheta);

        theta_to_free_pos_derivative = C * theta_to_free_pos_derivative;

        // Set the jacobian components
        matrix_operator().element(bound_to_free_jacobian, e_free_pos0,
                                  e_bound_phi) = phi_to_free_pos_derivative[0];
        matrix_operator().element(bound_to_free_jacobian, e_free_pos1,
                                  e_bound_phi) = phi_to_free_pos_derivative[1];
        matrix_operator().element(bound_to_free_jacobian, e_free_pos2,
                                  e_bound_phi) = phi_to_free_pos_derivative[2];
        matrix_operator().element(bound_to_free_jacobian, e_free_pos0,
                                  e_bound_theta) =
            theta_to_free_pos_derivative[0];
        matrix_operator().element(bound_to_free_jacobian, e_free_pos1,
                                  e_bound_theta) =
            theta_to_free_pos_derivative[1];
        matrix_operator().element(bound_to_free_jacobian, e_free_pos2,
                                  e_bound_theta) =
            theta_to_free_pos_derivative[2];
    }
};

}  // namespace detray