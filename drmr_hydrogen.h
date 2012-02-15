/* drmr_hydrogen.h
 * LV2 DrMr plugin
 * Copyright 2012 Nick Lanham <nick@afternight.org>
 *
 * Public License v3. source code is available at 
 * <http://github.com/nicklan/drmr>

 * THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#ifndef DRMR_HYDRO_H
#define DRMR_HYDRO_H

kits* scan_kits();
void free_kits(kits* kits);
void free_samples(drmr_sample* samples, int num_samples);
int load_sample(char* path,drmr_layer* layer,double target_rate);
drmr_sample *load_hydrogen_kit(char *path, double rate, int *num_samples);

#endif // DRMR_HYDRO_H
