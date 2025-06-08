#include <math.h>
#include <string.h>
#include <omp.h>
#include "fftw3.h"

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif


// Constantes pour le traitement par blocs et l'optimisation mémoire
#define BLOCK_WIDTH 32
#define BLOCK_HEIGHT 32
#define PI 3.14159265358979323846f

// Préchargement mémoire pour optimiser les accès
#define PREFETCH(ptr) __builtin_prefetch(ptr)

// Variables globales pour les plans FFT et les buffers
static fftwf_plan g_fft2d_plan = NULL;
static float *g_fft_input_tmp = NULL;
static fftwf_complex *g_fft_result = NULL;

static fftwf_plan g_ifft2d_plan = NULL;
static fftwf_complex *g_ifft_input_tmp = NULL;
static float *g_ifft_result = NULL;

static int g_width = 0;
static int g_height = 0;
static int g_line_length = 0;
static int g_initialized = 0;

/**
 * Libère les ressources FFTW
 */
void cleanup_fftw_resources() {
    if (g_fft2d_plan) {
        fftwf_destroy_plan(g_fft2d_plan);
        g_fft2d_plan = NULL;
    }
    
    if (g_fft_input_tmp) {
        fftwf_free(g_fft_input_tmp);
        g_fft_input_tmp = NULL;
    }
    
    if (g_fft_result) {
        fftwf_free(g_fft_result);
        g_fft_result = NULL;
    }
	
    if (g_ifft2d_plan) {
        fftwf_destroy_plan(g_ifft2d_plan);
        g_ifft2d_plan = NULL;
    }
	
    if (g_ifft_input_tmp) {
        fftwf_free(g_ifft_input_tmp);
        g_ifft_input_tmp = NULL;
    }
    
    if (g_ifft_result) {
        fftwf_free(g_ifft_result);
        g_ifft_result = NULL;
    }
    
    g_width = 0;
    g_height = 0;
	g_line_length = 0;
    g_initialized = 0;
}

/**
 * Initialise les ressources FFTW pour la réutilisation
 * @param width Largeur de l'image
 * @param height Hauteur de l'image
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int init_fftw_resources(int width, int height, int line_length) {
    // Si déjà initialisé avec les mêmes dimensions, pas besoin de réinitialiser
    if (g_initialized && g_width == width && g_height == height && g_line_length == line_length) {
        return 0;
    }
    
    // Nettoyer les ressources existantes si nécessaire
    cleanup_fftw_resources();
    
    // Initialiser FFTW avec support multi-threading (optionnel, à faire une seule fois)
    fftwf_init_threads();
    fftwf_plan_with_nthreads(omp_get_max_threads());
    
    // Allouer la mémoire
    g_fft_input_tmp = fftwf_malloc(sizeof(float) * width * height);
    g_fft_result = fftwf_malloc(sizeof(fftwf_complex) * width * height);
	
	g_ifft_result = fftwf_malloc(sizeof(float) * width * height);
    g_ifft_input_tmp = fftwf_malloc(sizeof(fftwf_complex) * height * (width/2 + 1));    

	if (!g_fft_input_tmp || !g_fft_result || !g_ifft_input_tmp || !g_ifft_result) {
        cleanup_fftw_resources();
        return -1;
    }
    
    // Créer les plans FFT
    g_fft2d_plan = fftwf_plan_dft_r2c_2d(height, width, g_fft_input_tmp, g_fft_result, FFTW_MEASURE);
	g_ifft2d_plan = fftwf_plan_dft_c2r_2d(height, width, g_ifft_input_tmp, g_ifft_result, FFTW_MEASURE);
    
    if (!g_fft2d_plan || !g_ifft2d_plan) {
        cleanup_fftw_resources();
        return -1;
    }
    
    g_width = width;
    g_height = height;
	g_line_length = line_length;
    g_initialized = 1;
    
    return 0;
}

/**
 * Filtre le spectre de fréquence pour éliminer le moiré spécifique aux écrans Kaleido 3
 * 
 * @param spectrum Spectre FFT complet (modifié sur place)
 * @param width Largeur de l'image
 * @param height Hauteur de l'image
 * @param param_radius_min Rayon minimal pour le filtre passe-bas
 * @param param_radius_max_diviser Diviseur pour calculer le rayon maximal
 */
void filter_spectrum_for_kaleido(fftwf_complex *spectrum, int width, int height, 
                                float param_radius_min, float param_radius_max_diviser) {
    
    float radius_min = param_radius_min;
    float radius_max = width / param_radius_max_diviser;

    int center_x = width / 2;
    int center_y = height / 2;
    
    float radius_min_squared = radius_min * radius_min;
    float radius_max_squared = radius_max * radius_max;
    float radius_diff_inv = 1.0f / (radius_max - radius_min);

    const float PI_2 = PI / 2;
    const float PI_4 = PI / 4;
    const float angle_threshold = 0.05f;
    const float angle_threshold_diag = 0.1f;
    const float magnitude_threshold = 10000.0f;

    // OpenMP parallèle sur les blocs verticaux
    #pragma omp parallel for schedule(dynamic)
    for (int by = 0; by < height; by += BLOCK_HEIGHT) {
        int block_h = (by + BLOCK_HEIGHT <= height) ? BLOCK_HEIGHT : height - by;
        
        for (int bx = 0; bx < width; bx += BLOCK_WIDTH) {
            int block_w = (bx + BLOCK_WIDTH <= width) ? BLOCK_WIDTH : width - bx;

            if (bx + BLOCK_WIDTH < width) {
                PREFETCH(&spectrum[2 * (by * width + (bx + BLOCK_WIDTH))]);
            }

            for (int y = 0; y < block_h; y++) {
                for (int x = 0; x < block_w; x++) {
                    int px = bx + x;
                    int py = by + y;
                    int idx = py * width + px;

                    float dx = px - center_x;
                    float dy = py - center_y;
                    float radius_squared = dx * dx + dy * dy;

                    float attenuation = 1.0f;

                    if (radius_squared > radius_max_squared) {
                        attenuation = 0.0f;
                    } else if (radius_squared <= radius_min_squared) {
                        attenuation = 1.0f;
                    } else {
                        float radius = sqrtf(radius_squared);
                        float angle = atan2f(dy, dx);

                        float real = ((float*)spectrum)[2*idx];
                        float imag = ((float*)spectrum)[2*idx+1];
                        float magnitude = sqrtf(real * real + imag * imag);

                        float angle_mod = fmodf(fabsf(angle), PI_2);

                        if ((angle_mod < angle_threshold || angle_mod > PI_2 - angle_threshold) && 
                            radius_squared > 4 * radius_min_squared) {
                            attenuation = (magnitude > magnitude_threshold) ? 
                                0.01f : 1.0f - (radius - radius_min) * radius_diff_inv * 0.5f;
                        } else if (fabsf(fmodf(fabsf(angle - PI_4), PI_2)) < angle_threshold_diag && 
                                radius_squared > 4 * radius_min_squared) {
                            attenuation = 0.3f;
                        } else {
                            attenuation = 1.0f - (radius - radius_min) * radius_diff_inv * 0.2f;
                        }
                    }

                    ((float*)spectrum)[2*idx] *= attenuation;
                    ((float*)spectrum)[2*idx+1] *= attenuation;
                }
            }
        }
    }
}

/**
 * Applique la FFT 2D à une image en niveaux de gris
 * Implémente l'algorithme de Cooley-Tukey (par lignes puis colonnes)
 * 
 * @param input_data Données de l'image d'entrée
 * @param output_spectrum Spectre de sortie complexe
 * @param width Largeur de l'image
 * @param height Hauteur de l'image
 * @param line_length Longueur de ligne (peut inclure padding)
 */
void fft2d_grayscale(unsigned char *input_data, fftwf_complex *output_spectrum, 
                    int width, int height, int line_length) {  
    // Conversion RGB24 → niveau de gris (luminance)
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char r = input_data[y * line_length + x * 3 + 0];
            unsigned char g = input_data[y * line_length + x * 3 + 1];
            unsigned char b = input_data[y * line_length + x * 3 + 2];
            float gray = (r + g + b)/3;
            g_fft_input_tmp[y * width + x] = gray;
        }
    }
    
    // Appliquer la FFT 2D avec le plan préexistant
    fftwf_execute(g_fft2d_plan);
    
    // Copier et centrer le spectre dans output_spectrum avec symétrie hermitienne
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        int dst_y = (y + height / 2) % height;
        for (int x = 0; x < width / 2 + 1; x++) {
            int dst_x = (x + width / 2) % width;
            float *out_ptr = (float *)&output_spectrum[dst_y * width + dst_x];
            float *in_ptr = (float *)&g_fft_result[y * (width / 2 + 1) + x];
            out_ptr[0] = in_ptr[0];  // Re
            out_ptr[1] = in_ptr[1];  // Im
            // Remplir miroir hermitien
            if (x > 0 && x < width / 2) {
                int mirror_x = (width - x) % width;
                int dst_mirror_x = (mirror_x + width / 2) % width;
                float *mirror_ptr = (float *)&output_spectrum[dst_y * width + dst_mirror_x];
                mirror_ptr[0] = in_ptr[0];      // Re identique
                mirror_ptr[1] = -in_ptr[1];     // Im conjuguée
            }
        }
    }
    // Note: on ne détruit pas le plan ni ne libère la mémoire ici
}


/**
 * Applique la transformée de Fourier inverse 2D pour récupérer l'image
 * 
 * @param input_spectrum Spectre d'entrée complexe
 * @param output_data Données de sortie de l'image
 * @param width Largeur
 * @param height Hauteur
 * @param line_length Longueur de ligne (peut inclure padding)
 */
void ifft2d_grayscale(fftwf_complex *input_spectrum, unsigned char *output_data,
                     int width, int height, int line_length) {  
    // Réorganiser le spectre centré vers le format attendu par FFTW pour c2r
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        int dst_y = y;
        int src_y = (y + height/2) % height;
        
        for (int x = 0; x < width/2 + 1; x++) {
            int src_x = (x + width/2) % width;
            
            // Accès correct au format interleaved
            float *out_ptr = (float *)&g_ifft_input_tmp[dst_y * (width/2 + 1) + x];
            float *in_ptr = (float *)&input_spectrum[src_y * width + src_x];
            
            out_ptr[0] = in_ptr[0];  // Partie réelle
            out_ptr[1] = in_ptr[1];  // Partie imaginaire
        }
    }
	
	// Appliquer la IFFT 2D avec le plan préexistant
    fftwf_execute(g_ifft2d_plan);
    
    // Normaliser et convertir les résultats en RGB (image en niveaux de gris)
    float norm_factor = 1.0f / (width * height);

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Normaliser
            float pixel_value = g_ifft_result[y * width + x] * norm_factor;
            
            // Limiter les valeurs entre 0 et 255
            int pixel_int = (int)pixel_value;
            pixel_int = (pixel_int < 0) ? 0 : ((pixel_int > 255) ? 255 : pixel_int);
            
            // Écrire la valeur dans les 3 canaux RGB
            output_data[y * line_length + x * 3] = (unsigned char)pixel_int;
            output_data[y * line_length + x * 3 + 1] = (unsigned char)pixel_int;
            output_data[y * line_length + x * 3 + 2] = (unsigned char)pixel_int;
        }
    }
    // Note: on ne détruit pas le plan ni ne libère la mémoire ici
}

/**
 * Fonction principale pour supprimer le moiré
 * 
 * @param fb_data Données du framebuffer d'entrée (modifiées sur place)
 * @param width Largeur de l'image
 * @param height Hauteur de l'image
 * @param line_length Longueur de ligne du framebuffer
 * @param param_radius_min Rayon minimal pour le filtre passe-bas
 * @param param_radius_max_diviser Diviseur pour calculer le rayon maximal
 */
EXPORT void remove_moire(unsigned char *fb_data, int width, int height, int line_length,
                 float param_radius_min, float param_radius_max_diviser) {
    // Initialiser ou réutiliser les ressources FFTW
    if (init_fftw_resources(width, height, line_length) != 0) {
        fprintf(stderr, "Erreur d'initialisation des ressources FFTW\n");
        return;
    }
    
    // Allouer mémoire alignée pour le spectre FFT
    fftwf_complex *fft_spectrum = fftwf_malloc(sizeof(fftwf_complex) * width * height);
    
    // Appliquer la FFT 2D
    fft2d_grayscale(fb_data, fft_spectrum, width, height, line_length);
    
    // Filtrer le spectre pour éliminer le moiré
    filter_spectrum_for_kaleido(fft_spectrum, width, height, param_radius_min, param_radius_max_diviser);
    
    // Appliquer l'IFFT 2D
	ifft2d_grayscale(fft_spectrum, fb_data, width, height, line_length);
    
    
    // Libérer la mémoire temporaire
    fftwf_free(fft_spectrum);
    
}

EXPORT int init_moire_resources() {
    // Initialisation des threads FFTW une seule fois
    fftwf_init_threads();
    fftwf_plan_with_nthreads(omp_get_max_threads());
    return 0;
}

EXPORT void cleanup_moire_resources() {
    cleanup_fftw_resources();
    fftwf_cleanup_threads();
    fftwf_cleanup();
}