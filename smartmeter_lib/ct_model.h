#ifndef CT_MODEL_H
#define CT_MODEL_H

/*
 * Current Transformer (CT) Model
 * Used for phase error compensation due to temperature change and current range in CT
 *
 */

#include <stdint.h>
#include <stdio.h>

#define CT_MODELTYPE_LINEAR_REGRESSION 0
#define CT_MODELTYPE_NEURALNET         1



/* 
 * Struct for CT profile
 *
 */
typedef struct
{
    uint8_t     model_type;
    double      coeffs[10];
    double	offset;

}ct_profile_t;

double ct_get_phase_error(ct_profile_t *ct_profile, double temperature, double current);

#endif
