#ifndef WLT_GLOBAL_H
#define WLT_GLOBAL_H

extern wlt_run_time_config_t *prtconfig;
extern wlt_config_data_t *pconfig;

extern float C2F(float temperature);
extern void wlt_update_and_save_config(wlt_run_time_config_t *rt_config, wlt_config_data_t *config);


// favicon.ico
extern const unsigned char favicon_ico[];
extern const unsigned int favicon_ico_len;


#endif // WLT_GLOBAL_H

