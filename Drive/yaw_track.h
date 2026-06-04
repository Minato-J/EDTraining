#ifndef __YAW_TRACK_H
#define __YAW_TRACK_H

#include <stdint.h>

void YawTrack_Reset(float current_yaw);
void YawTrack_Update(float current_yaw);
float YawTrack_GetCumulative(void);
uint8_t YawTrack_IsCurveDone(float threshold_deg);

#endif
