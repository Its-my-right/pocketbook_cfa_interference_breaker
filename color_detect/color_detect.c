/**
 * color_detect.c - Détection efficace de pixels colorés dans un framebuffer
 * 
 * Ce code permet de déterminer si une image contient des pixels colorés (non gris)
 * en utilisant des optimisations NEON, OpenMP et une gestion efficace de la mémoire.
 * Format d'image attendu: RGB 24-bit (3 octets par pixel)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

/* Définition de l'export pour l'utilisation en tant que bibliothèque partagée */
#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

/* Alignement mémoire optimal pour les opérations SIMD */
#define MEM_ALIGN 16

/**
 * Détermine si un pixel unique est coloré (non gris) en comparant les canaux R, G, B
 * Un pixel est considéré comme coloré si la différence entre deux canaux
 * quelconques dépasse la tolérance spécifiée.
 */
static inline bool is_pixel_colored(uint8_t r, uint8_t g, uint8_t b, int tolerance) {
    /* Un pixel est coloré si la différence entre deux canaux quelconques est supérieure à la tolérance */
    return (abs(r - g) > tolerance) || 
           (abs(r - b) > tolerance) || 
           (abs(g - b) > tolerance);
}

/**
 * Version scalaire (non-NEON) pour analyser un bloc de pixels
 * Cette fonction est utilisée comme fallback lorsque NEON n'est pas disponible
 * ou pour les bords de l'image qui ne peuvent pas être traités par paquets.
 */
static bool is_block_colored_scalar(const uint8_t* data, int stride, 
                                    int x_start, int y_start,
                                    int block_width, int block_height,
                                    int img_width, int img_height,
                                    int tolerance) __attribute__((unused));

static bool is_block_colored_scalar(const uint8_t* data, int stride, 
                                    int x_start, int y_start,
                                    int block_width, int block_height,
                                    int img_width, int img_height, 
                                    int tolerance) {
    /* Parcours du bloc de pixels */
    for (int y = y_start; y < y_start + block_height && y < img_height; y++) {
        const uint8_t* row = data + (y * stride);
        
        for (int x = x_start; x < x_start + block_width && x < img_width; x++) {
            /* Pour chaque pixel, récupération des composantes RGB - 3 octets par pixel */
            const uint8_t* px = row + (x * 3);
            
            /* Si le pixel est coloré, on peut arrêter immédiatement */
            if (is_pixel_colored(px[0], px[1], px[2], tolerance)) {
                return true;
            }
        }
    }
    
    /* Aucun pixel coloré trouvé dans ce bloc */
    return false;
}

#ifdef __ARM_NEON
/**
 * Version optimisée avec NEON pour traiter plusieurs pixels en parallèle
 * Cette fonction analyse un bloc d'image en utilisant les instructions SIMD NEON
 * pour une performance optimale sur les architectures ARM.
 */
static bool is_block_colored_neon(const uint8_t* data, int stride,
                                  int x_start, int y_start,
                                  int block_width, int block_height,
                                  int img_width, int img_height,
                                  int tolerance) {
    const uint8_t tol = (uint8_t)tolerance;
    uint8x8_t tolerance_vec = vdup_n_u8(tol);
    
    /* Parcours ligne par ligne dans le bloc */
    for (int y = y_start; y < y_start + block_height && y < img_height; y++) {
        const uint8_t* row = data + (y * stride);
        
        /* Traitement de 8 pixels à la fois (3 octets par pixel = 24 octets) */
        for (int x = x_start; x < x_start + block_width && x < img_width; x += 8) {
            /* Vérification des limites */
            if (x + 8 > img_width) {
                /* Traitement des pixels restants avec la méthode scalaire */
                for (int i = x; i < img_width; i++) {
                    const uint8_t* px = row + (i * 3);
                    if (is_pixel_colored(px[0], px[1], px[2], tolerance)) {
                        return true;
                    }
                }
                break;
            }
            
            /* Préchargement des données pour réduire les latences mémoire */
            __builtin_prefetch(row + ((x + 16) * 3), 0, 0);
            
            /* Chargement des 8 pixels (24 octets) avec désentrelacement des canaux RGB */
            uint8x8x3_t pixels = vld3_u8(row + (x * 3));
            
            /* Extraction des canaux R, G, B */
            uint8x8_t r_channel = pixels.val[0];
            uint8x8_t g_channel = pixels.val[1];
            uint8x8_t b_channel = pixels.val[2];
            
            /* Calcul des différences absolues entre canaux */
            uint8x8_t diff_rg = vabd_u8(r_channel, g_channel);
            uint8x8_t diff_rb = vabd_u8(r_channel, b_channel);
            uint8x8_t diff_gb = vabd_u8(g_channel, b_channel);
            
            /* Comparaison avec la tolérance */
            uint8x8_t mask_rg = vcgt_u8(diff_rg, tolerance_vec);
            uint8x8_t mask_rb = vcgt_u8(diff_rb, tolerance_vec);
            uint8x8_t mask_gb = vcgt_u8(diff_gb, tolerance_vec);
            
            /* Combinaison des masques (OR logique) */
            uint8x8_t mask = vorr_u8(vorr_u8(mask_rg, mask_rb), mask_gb);
            
            /* Vérification si au moins un pixel est coloré dans ce groupe */
            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask), 0);
            if (mask_val != 0) {
                return true;
            }
        }
    }
    
    /* Aucun pixel coloré trouvé dans ce bloc */
    return false;
}
#endif

/**
 * Fonction principale exportée pour l'interface Lua
 * Analyse un framebuffer pour déterminer s'il contient des pixels colorés
 * 
 * @param data Pointeur vers les données de l'image (format RGB 24 bits)
 * @param width Largeur de l'image en pixels
 * @param height Hauteur de l'image en pixels
 * @param stride Longueur d'une ligne en octets (scanline)
 * @param tolerance Seuil de différence entre les canaux pour considérer un pixel comme coloré
 * @return true si l'image contient au moins un pixel coloré, false sinon
 */
EXPORT bool is_framebuffer_colored(uint8_t* data, int width, int height, int stride, int tolerance) {
    /* Paramètres optimaux pour les blocs de traitement */
    const int BLOCK_WIDTH = 8;  /* Identique au code Lua pour la cohérence */
    const int BLOCK_HEIGHT = 16;  /* Identique au code Lua pour la cohérence */
    
    /* Variable partagée pour indiquer si un pixel coloré a été trouvé */
    volatile bool found_colored = false;
    
    /* Configuration du nombre optimal de threads */
    int num_threads = omp_get_max_threads();
    omp_set_num_threads(num_threads);
    
    /* Traitement parallèle par blocs avec OpenMP */
    #pragma omp parallel
    {
        /* Distribution des blocs aux threads avec possibilité d'arrêt anticipé */
        #pragma omp for collapse(2) schedule(dynamic)
        for (int y = 0; y < height; y += BLOCK_HEIGHT) {
            for (int x = 0; x < width; x += BLOCK_WIDTH) {
                /* Vérification rapide si un autre thread a déjà trouvé un pixel coloré */
                if (found_colored) {
                    continue;
                }
                
                bool block_has_color = false;
                
                /* Sélection de l'implémentation optimale */
                #ifdef __ARM_NEON
                block_has_color = is_block_colored_neon(data, stride, x, y,
                                                       BLOCK_WIDTH, BLOCK_HEIGHT,
                                                       width, height, tolerance);
                #else
                block_has_color = is_block_colored_scalar(data, stride, x, y,
                                                         BLOCK_WIDTH, BLOCK_HEIGHT,
                                                         width, height, tolerance);
                #endif
                
                /* Si un pixel coloré est trouvé, mise à jour de la variable partagée */
                if (block_has_color) {
                    #pragma omp atomic write
                    found_colored = true;
                    
                    /* Signaler aux autres threads de s'arrêter */
                    #pragma omp cancel for
                }
            }
        }
    }
    
    return found_colored;
}