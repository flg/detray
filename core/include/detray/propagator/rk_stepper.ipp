/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s).
#include "detray/utils/column_wise_operator.hpp"

// System include(s)
#include <cmath>

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
void detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                        array_t>::state::advance_derivative() {

    // The derivative of position = direction
    matrix_operator().set_block(this->_derivative, this->_track.dir(),
                                e_free_pos0, 0);

    // The derivative of direction = k4
    matrix_operator().set_block(this->_derivative, this->_step_data.k4,
                                e_free_dir0, 0);
}

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
void detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                        array_t>::state::advance_track() {

    const auto& sd = this->_step_data;
    const auto& h = this->_step_size;
    auto& track = this->_track;
    auto pos = track.pos();
    auto dir = track.dir();

    // Update the track parameters according to the equations of motion
    pos = pos + h * dir + h * h / 6. * (sd.k1 + sd.k2 + sd.k3);
    track.set_pos(pos);

    dir = dir + h / 6. * (sd.k1 + 2. * (sd.k2 + sd.k3) + sd.k4);
    dir = vector::normalize(dir);
    track.set_dir(dir);

    // Update path length
    this->_path_length += h;
}

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
void detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                        array_t>::state::advance_jacobian() {
    /// The calculations are based on ATL-SOFT-PUB-2009-002. The update of the
    /// Jacobian matrix is requires only the calculation of eq. 17 and 18.
    /// Since the terms of eq. 18 are currently 0, this matrix is not needed
    /// in the calculation. The matrix A from eq. 17 consists out of 3
    /// different parts. The first one is given by the upper left 3x3 matrix
    /// that are calculated by the derivatives dF/dT (called dFdT) and dG/dT
    /// (calles dGdT). The second is given by the top 3 lines of the rightmost
    /// column. This is calculated by dFdL and dGdL. The remaining non-zero term
    /// is calculated directly. The naming of the variables is explained in eq.
    /// 11 and are directly related to the initial problem in eq. 7.
    /// The evaluation is based by propagating the parameters T and lambda as
    /// given in eq. 16 and evaluating the derivations for matrix A.
    /// @note The translation for u_{n+1} in eq. 7 is in this case a
    /// 3-dimensional vector without a dependency of Lambda or lambda neither in
    /// u_n nor in u_n'. The second and fourth eq. in eq. 14 have the constant
    /// offset matrices h * Id and Id respectively. This involves that the
    /// constant offset does not exist for rectangular matrix dGdu' (due to the
    /// missing Lambda part) and only exists for dFdu' in dlambda/dlambda.

    const auto& sd = this->_step_data;
    const auto& h = this->_step_size;
    // const auto& mass = this->_mass;
    auto& track = this->_track;
    const auto dir = track.dir();
    const auto qop = track.qop();

    // Half step length
    scalar half_h = h * 0.5;

    /*---------------------------------------------------------------------------
     * k_{n} is always in the form of [ A(T) X B ] where A is a function of r'
     * and B is magnetic field and X symbol is for cross product. Hence dk{n}dT
     * can be represented as follows:
     *
     *  dk{n}dT =  d/dT [ A(T) X B ] = dA(T)/dr (X) B
     *
     *  where,
     *  (X) symbol is column-wise cross product between matrix and vector,
     *  k1 = qop * T X B_first,
     *  k2 = qop * ( T + h / 2 * k1 ) X B_middle,
     *  k3 = qop * ( T + h / 2 * k2 ) X B_middle,
     *  k4 = qop * ( T + h * k3 ) * B_last.
    ---------------------------------------------------------------------------*/
    auto dk1dT = matrix_operator().template identity<3, 3>();
    auto dk2dT = matrix_operator().template identity<3, 3>();
    auto dk3dT = matrix_operator().template identity<3, 3>();
    auto dk4dT = matrix_operator().template identity<3, 3>();

    dk1dT = qop * column_wise_op().cross(dk1dT, sd.b_first);
    dk2dT = dk2dT + half_h * dk1dT;
    dk2dT = qop * column_wise_op().cross(dk2dT, sd.b_middle);
    dk3dT = dk3dT + half_h * dk2dT;
    dk3dT = qop * column_wise_op().cross(dk3dT, sd.b_middle);
    dk4dT = dk4dT + h * dk3dT;
    dk4dT = qop * column_wise_op().cross(dk4dT, sd.b_last);

    // dFdT and dGdT are top-left 3x3 submatrix of equation (17)
    auto dFdT = matrix_operator().template identity<3, 3>();
    auto dGdT = matrix_operator().template identity<3, 3>();
    dFdT = dFdT + h / 6. * (dk1dT + dk2dT + dk3dT);
    dFdT = h * dFdT;
    dGdT = dGdT + h / 6. * (dk1dT + 2. * (dk2dT + dk3dT) + dk4dT);

    // Calculate dk{n}dL where L is qop
    vector3 dk1dL = vector::cross(dir, sd.b_first);
    vector3 dk2dL = vector::cross(dir + half_h * sd.k1, sd.b_middle) +
                    qop * half_h * vector::cross(dk1dL, sd.b_middle);
    vector3 dk3dL = vector::cross(dir + half_h * sd.k2, sd.b_middle) +
                    qop * half_h * vector::cross(dk2dL, sd.b_middle);
    vector3 dk4dL = vector::cross(dir + h * sd.k3, sd.b_last) +
                    qop * h * vector::cross(dk3dL, sd.b_last);

    // dFdL and dGdL are top-right 3x1 submatrix of equation (17)
    vector3 dFdL = (h * h) / 6. * (dk1dL + dk2dL + dk3dL);
    vector3 dGdL = h / 6. * (dk1dL + 2. * (dk2dL + dk3dL) + dk4dL);

    // Set transport matrix (D) and update Jacobian transport
    //( JacTransport = D * JacTransport )
    auto D = matrix_operator().template identity<e_free_size, e_free_size>();
    matrix_operator().set_block(D, dFdT, 0, 4);
    matrix_operator().set_block(D, dFdL, 0, 7);
    matrix_operator().set_block(D, dGdT, 4, 4);
    matrix_operator().set_block(D, dGdL, 4, 7);

    /// Calculate (4,4) element of equation (17)
    /// NOTE: Let's skip this element for the moment
    /// const auto p = getter::norm(track.mom());
    /// matrix_operator().element(D, 3, 7) =
    /// h * mass * mass * qop * getter::perp(vector2{1, mass / p});

    this->_jac_transport = D * this->_jac_transport;
}

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
auto detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                        array_t>::state::evaluate_k(const vector3& b_field,
                                                    const int i, const scalar h,
                                                    const vector3& k_prev)
    -> vector3 {
    auto& track = this->_track;

    const auto qop = track.qop();
    const auto dir = track.dir();

    vector3 k_new;

    if (i == 0) {
        k_new = qop * vector::cross(dir, b_field);
    } else {
        k_new = qop * vector::cross(dir + h * k_prev, b_field);
    }

    return k_new;
}

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
template <typename propagation_state_t>
bool detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                        array_t>::step(propagation_state_t& propagation) {
    // Get stepper and navigator states
    state& stepping = propagation._stepping;
    auto& navigation = propagation._navigation;

    auto& sd = stepping._step_data;

    scalar error_estimate = 0;

    // First Runge-Kutta point
    sd.b_first = _magnetic_field.get_field(stepping().pos(), context_type{});
    sd.k1 = stepping.evaluate_k(sd.b_first, 0, 0, vector3{0, 0, 0});

    const auto try_rk4 = [&](const scalar& h) -> bool {
        // State the square and half of the step size
        const scalar h2 = h * h;
        const scalar half_h = h * 0.5;
        auto pos = stepping().pos();
        auto dir = stepping().dir();

        // Second Runge-Kutta point
        const vector3 pos1 = pos + half_h * dir + h2 * 0.125 * sd.k1;
        sd.b_middle = _magnetic_field.get_field(pos1, context_type{});
        sd.k2 = stepping.evaluate_k(sd.b_middle, 1, half_h, sd.k1);

        // Third Runge-Kutta point
        sd.k3 = stepping.evaluate_k(sd.b_middle, 2, half_h, sd.k2);

        // Last Runge-Kutta point
        const vector3 pos2 = pos + h * dir + h2 * 0.5 * sd.k3;
        sd.b_last = _magnetic_field.get_field(pos2, context_type{});
        sd.k4 = stepping.evaluate_k(sd.b_last, 3, h, sd.k3);

        // Compute and check the local integration error estimate
        // @Todo
        const vector3 err_vec = h2 * (sd.k1 - sd.k2 - sd.k3 + sd.k4);
        error_estimate =
            std::max(getter::norm(err_vec), static_cast<scalar>(1e-20));

        return (error_estimate <= stepping._tolerance);
    };

    // Initial step size estimate
    stepping.set_step_size(navigation());

    scalar step_size_scaling = 1.;
    size_t n_step_trials = 0;

    // Adjust initial step size to integration error
    while (!try_rk4(stepping._step_size)) {

        step_size_scaling = std::min(
            std::max(0.25 * unit_constants::mm,
                     std::sqrt(std::sqrt((stepping._tolerance /
                                          std::abs(2. * error_estimate))))),
            4.);

        stepping._step_size *= step_size_scaling;

        // If step size becomes too small the particle remains at the
        // initial place
        if (std::abs(stepping._step_size) <
            std::abs(stepping._step_size_cutoff)) {
            // Not moving due to too low momentum needs an aborter
            printf("Stepper: step size is too small. will break. \n");
            // State is broken
            return navigation.abort();
        }

        // If the parameter is off track too much or given step_size is not
        // appropriate
        if (n_step_trials > stepping._max_rk_step_trials) {
            // Too many trials, have to abort
            printf("Stepper: too many rk4 trials. will break. \n");
            // State is broken
            return navigation.abort();
        }
        n_step_trials++;
    }

    // Update navigation direction
    const step::direction step_dir = stepping._step_size >= 0
                                         ? step::direction::e_forward
                                         : step::direction::e_backward;
    stepping.set_direction(step_dir);

    // Check constraints
    if (std::abs(stepping.step_size()) >
        std::abs(
            stepping.constraints().template size<>(stepping.direction()))) {
        stepping.set_step_size(
            stepping.constraints().template size<>(stepping.direction()));
    }

    // Advance derivative of position and direction w.r.t path length
    stepping.advance_derivative();

    // Advance track state
    stepping.advance_track();

    // Advance jacobian transport
    stepping.advance_jacobian();

    // Call navigation update policy
    policy_t{}(stepping.policy_state(), propagation);

    return true;
}

template <typename magnetic_field_t, typename transform3_t,
          typename constraint_t, typename policy_t,
          template <typename, std::size_t> class array_t>
template <typename propagation_state_t>
typename detray::template rk_stepper<magnetic_field_t, transform3_t,
                                     constraint_t, policy_t,
                                     array_t>::bound_track_parameters_type
detray::rk_stepper<magnetic_field_t, transform3_t, constraint_t, policy_t,
                   array_t>::bound_state(propagation_state_t& /*propagation*/,
                                         const transform3_t& /*trf3*/) {

    return detray::bound_track_parameters<transform3_t>();

    // @TODO: Use detector kernel
    /*
    auto& stepping = propagation._stepping;
    auto& navigation = propagation._navigation;

    // Get the free vector
    const auto& free_vector = stepping().vector();

    // Get the bound vector
    const auto bound_vector =
        vector_engine().free_to_bound_vector(trf3, free_vector);

    // Get the bound covariance
    covariance_engine().bound_to_bound_covariance_update(
        trf3, stepping._bound_covariance, free_vector, stepping._jac_to_global,
        stepping._jac_transport, stepping._derivative);

    // Reset the jacobians in RK stepper state
    covariance_engine().reinitialize_jacobians(
        trf3, bound_vector, stepping._jac_to_global, stepping._jac_transport,
        stepping._derivative);

    return detray::bound_track_parameters(
        navigation.current_object(), bound_vector, stepping._bound_covariance);
    */
}