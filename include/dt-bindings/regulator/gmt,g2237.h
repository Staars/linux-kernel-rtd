#ifndef __DT_BINDINGS_SOC_REALTEK_GMT_G2237_H
#define __DT_BINDINGS_SOC_REALTEK_GMT_G2237_H

#define G2237_DC_MODE_AUTO       0
#define G2237_DC_MODE_FORCE_PWM  1
#define G2237_LDO_MODE_NORMAL  (G2237_DC_MODE_AUTO)
#define G2237_LDO_MODE_ECO     2

#define G2237_LPOFF_TO_SHUTDOWN 0
#define G2237_LPOFF_TO_RESTART  1

#define G2237_TIME_IT_0128MS 0
#define G2237_TIME_IT_0500MS 1
#define G2237_TIME_IT_1000MS 2
#define G2237_TIME_IT_1500MS 3

#define G2237_TIME_LP_01S 0
#define G2237_TIME_LP_02S 1
#define G2237_TIME_LP_03S 2
#define G2237_TIME_LP_04S 3

#define G2237_TIME_LPOFF_06S 0
#define G2237_TIME_LPOFF_07S 1
#define G2237_TIME_LPOFF_08S 2
#define G2237_TIME_LPOFF_10S 3

#endif
