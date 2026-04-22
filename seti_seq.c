// ═══════════════════════════════════════════════════════════════

// SETI Signal Analyzer  -  VERSION SECUENCIAL

// Computación Paralela

// Universidad del Desarrollo — 2026

// ═══════════════════════════════════════════════════════════════

#include <stdio.h>

#include <stdlib.h>

#include <math.h>

#include <time.h>

#include <string.h>

 

#define N_CHANNELS    1048576   // 2^20 canales de frecuencia

#define N_FRAMES      512       // frames temporales por canal

#define THRESHOLD     6.5       // umbral SNR para candidatos

#define DRIFT_RATES   8         // tasas de deriva Doppler a probar

 

// ─── Estructura de señal candidata ──────────────────────────────

typedef struct {

    int    channel;

    int    frame;

    int    drift_idx;

    double snr;

    double freq_mhz;

} Candidate;

 

// ─── Genera datos sintéticos de radiotelescopi ──────────────────

void generate_spectrogram(float *data, int channels, int frames) {

    srand(42);

    for (int c = 0; c < channels; c++) {

        for (int f = 0; f < frames; f++) {

            float noise = ((float)rand()/RAND_MAX) * 2.0f - 1.0f;

            data[c * frames + f] = noise;

        }

    }

    // Inyectar señales artificiales (simulación de candidatos reales)

    int inject_ch[] = {102400, 524288, 786432};

    for (int k = 0; k < 3; k++) {

        int ch = inject_ch[k];

        for (int f = 0; f < frames; f++) {

            data[ch * frames + f] += 8.0f * sinf(2*M_PI*f*0.1f);

        }

    }

}

 

// ─── Algoritmo de coincidencia de deriva Doppler ────────────────

// Esta función es el CUELLO DE BOTELLA — O(N_CHANNELS * N_FRAMES * DRIFT_RATES)

int dedoppler_search(float *data, int channels, int frames,

                     Candidate *results, int *n_results) {

    double drift_table[DRIFT_RATES] = {

        -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 4.0  // Hz/s

    };

    *n_results = 0;

    double t_start = 1420.0;  // Frecuencia de la línea de hidrógeno (MHz)

 

    for (int d = 0; d < DRIFT_RATES; d++) {

        for (int c = 0; c < channels; c++) {

            double sum = 0.0, sum2 = 0.0;

            // Integración a lo largo de la trayectoria de deriva

            for (int f = 0; f < frames; f++) {

                int shifted_c = c + (int)(drift_table[d] * f * 0.001);

                if (shifted_c < 0 || shifted_c >= channels) break;

                double val = data[shifted_c * frames + f];

                sum  += val;

                sum2 += val * val;

            }

            double mean = sum / frames;

            double var  = (sum2 / frames) - mean * mean;

            double snr  = (var > 0) ? mean / sqrt(var) : 0.0;

 

            if (snr > THRESHOLD && *n_results < 10000) {

                results[*n_results].channel   = c;

                results[*n_results].frame     = 0;

                results[*n_results].drift_idx = d;

                results[*n_results].snr       = snr;

                results[*n_results].freq_mhz  =

                    t_start + c * (10.0 / channels);

                (*n_results)++;

            }

        }

    }

    return *n_results;

}

 

// ─── Calcula métricas de rendimiento ────────────────────────────

void print_metrics(double t_elapsed, int n_results,

                   int channels, int frames, int drifts) {

    long long total_ops = (long long)channels * frames * drifts;

    double gflops = total_ops / (t_elapsed * 1e9);

    printf("\n=== METRICAS DE RENDIMIENTO ===\n");

    printf("Tiempo total        : %.4f s\n",   t_elapsed);

    printf("Canales procesados  : %d\n",       channels);

    printf("Operaciones totales : %lld\n",     total_ops);

    printf("GFLOPs efectivos    : %.4f\n",     gflops);

    printf("Candidatos hallados : %d\n",       n_results);

    printf("Throughput          : %.2f Mch/s\n",

           (channels / 1e6) / t_elapsed);

}

 

int main(void) {

    printf("SETI Dedoppler Search — Version Secuencial\n");

    printf("Canales: %d  |  Frames: %d  |  Derivas: %d\n",

            N_CHANNELS, N_FRAMES, DRIFT_RATES);

 

    // Reservar espacio para el espectrograma

    size_t sz = (size_t)N_CHANNELS * N_FRAMES * sizeof(float);

    float *spectrogram = (float *)malloc(sz);

    if (!spectrogram) { perror("malloc"); return 1; }

    Candidate *results = malloc(10000 * sizeof(Candidate));

    if (!results)    { perror("malloc"); return 1; }

 

    // Generar datos del espectrograma sintético

    printf("Generando espectrograma...\n");

    generate_spectrogram(spectrogram, N_CHANNELS, N_FRAMES);

 

    // Búsqueda de señales — SECCIÓN CRÍTICA A PARALELIZAR

    printf("Iniciando búsqueda dedoppler...\n");

    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);

 

    int n_results = 0;

    dedoppler_search(spectrogram, N_CHANNELS, N_FRAMES,

                     results, &n_results);

 

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (t1.tv_sec - t0.tv_sec) +

                     (t1.tv_nsec - t0.tv_nsec) / 1e9;

 

    // Mostrar los 5 mejores candidatos por SNR

    printf("\nTop candidatos:\n");

    for (int i = 0; i < (n_results < 5 ? n_results : 5); i++) {

        printf("  [%d] Canal %6d | %.4f MHz | SNR=%.3f | Deriva=%d\n",

               i+1, results[i].channel, results[i].freq_mhz,

               results[i].snr, results[i].drift_idx);

    }

 

    print_metrics(elapsed, n_results, N_CHANNELS, N_FRAMES, DRIFT_RATES);

 

    free(spectrogram);

    free(results);

    return 0;

}

