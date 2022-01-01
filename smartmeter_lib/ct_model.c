#include "ct_model.h"




double ct_get_phase_error(ct_profile_t *ct_profile, double temperature, double current)
{
    double phase_error;
    
    // For linear regression model
    if(ct_profile->model_type == CT_MODELTYPE_LINEAR_REGRESSION)
    {
	phase_error = (ct_profile->coeffs[0] * temperature) + (ct_profile->coeffs[1] * current) + ct_profile->offset; 
    }
    else{
    }

    return phase_error;
}
