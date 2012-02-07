// Hydro header

#ifndef DRMR_HYDRO_H
#define DRMR_HYDRO_H

kits* scan_kits();
void free_samples(drmr_sample* samples, int num_samples);
int load_hydrogen_kit(DrMr* drmr, char* path);

#endif // DRMR_HYDRO_H
