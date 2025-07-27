/**
 * @file kepler_sim_3d.c
 * @brief A high-speed Keplerian orbit simulator that generates historical
 * 3D planetary position data and saves it to a CSV file.
 *
 * This program fetches orbital elements from NASA for the simulation's start
 * date (the epoch). It then uses Kepler's equations to calculate the daily
 * X, Y, and Z coordinates of the planets over a user-specified date range.
 *
 * A "-debug" command-line argument can be used to display raw API responses.
 *
 * Compilation:
 * gcc kepler_sim_3d.c -o kepler_sim_3d -lcurl -ljansson -lm
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include <math.h>
#include <time.h>

// --- Constants ---
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define SECONDS_IN_DAY (24 * 60 * 60)
#define AU_TO_KM 149597870.7

// Struct to hold the response data from a curl request.
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Struct to hold a planet's Keplerian orbital elements.
struct Planet {
    const char *name;
    const char *id;
    // --- Orbital Elements ---
    double eccentricity;
    double semi_major_axis_au;
    double inclination_deg;
    double lon_asc_node_deg;
    double arg_periapsis_deg;
    double mean_anomaly_deg;
    time_t epoch;
    // --- Calculated Position ---
    double x, y, z; // Heliocentric ecliptic coordinates in AU
};

// --- Function Prototypes ---
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
int fetch_orbital_elements(struct Planet *planet, const char* epoch_str, int debug_mode);
void calculate_position(struct Planet *planet, time_t current_date);

// --- Main ---
int main(int argc, char *argv[]) {
    struct Planet planets[] = {
        {"Mercury", "199"}, {"Venus", "299"}, {"Earth", "399"},
        {"Mars", "499"}, {"Jupiter", "599"}, {"Saturn", "699"},
        {"Uranus", "799"}, {"Neptune", "899"}, {"Pluto", "999"}
    };
    int num_planets = sizeof(planets) / sizeof(planets[0]);

    // Check for debug flag
    int debug_mode = 0;
    if (argc > 1 && strcmp(argv[1], "-debug") == 0) {
        debug_mode = 1;
        printf("Debug mode enabled.\n");
    }

    // --- Get User Input ---
    char start_date_input[11], end_date_input[11], output_filename[100];

    printf("--- 3D High-Speed Keplerian Orbit Simulator ---\n");
    printf("Enter Start Date for simulation (YYYY-MM-DD): ");
    if (scanf("%10s", start_date_input) != 1) {
        fprintf(stderr, "Error: Invalid input format.\n"); return 1;
    }
    printf("Enter End Date for simulation (YYYY-MM-DD): ");
    if (scanf("%10s", end_date_input) != 1) {
        fprintf(stderr, "Error: Invalid input format.\n"); return 1;
    }
    printf("Enter Output Filename (e.g., data_3d.csv): ");
    if (scanf("%99s", output_filename) != 1) {
        fprintf(stderr, "Error: Invalid input format.\n"); return 1;
    }

    // --- Fetch Orbital Elements for the Epoch (which is the start date) ---
    printf("\nFetching orbital elements from NASA for epoch %s...\n", start_date_input);
    curl_global_init(CURL_GLOBAL_ALL);
    for (int i = 0; i < num_planets; i++) {
        if (fetch_orbital_elements(&planets[i], start_date_input, debug_mode) != 0) {
            fprintf(stderr, "Failed to fetch or parse data for %s. Aborting.\n", planets[i].name);
            return 1;
        }
    }
    printf("Successfully fetched all orbital elements.\n\n");

    // --- Open File for Writing ---
    FILE *outfile = fopen(output_filename, "w");
    if (outfile == NULL) {
        perror("Error opening output file");
        return 1;
    }
    fprintf(outfile, "Date");
    for (int i = 0; i < num_planets; i++) fprintf(outfile, ",%s_x,%s_y,%s_z", planets[i].name, planets[i].name, planets[i].name);
    fprintf(outfile, "\n");

    // --- Main Simulation Loop ---
    struct tm start_tm = {0}, end_tm = {0};
    sscanf(start_date_input, "%d-%d-%d", &start_tm.tm_year, &start_tm.tm_mon, &start_tm.tm_mday);
    sscanf(end_date_input, "%d-%d-%d", &end_tm.tm_year, &end_tm.tm_mon, &end_tm.tm_mday);
    start_tm.tm_year -= 1900; start_tm.tm_mon -= 1;
    end_tm.tm_year -= 1900; end_tm.tm_mon -= 1;
    time_t start_t = mktime(&start_tm);
    time_t end_t = mktime(&end_tm);
    time_t current_t = start_t;

    while (current_t <= end_t) {
        char date_str[11];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&current_t));
        printf("Calculating: %s\r", date_str);
        fflush(stdout);

        fprintf(outfile, "%s", date_str);
        for (int i = 0; i < num_planets; i++) {
            calculate_position(&planets[i], current_t);
            fprintf(outfile, ",%.6f,%.6f,%.6f", planets[i].x, planets[i].y, planets[i].z);
        }
        fprintf(outfile, "\n");
        
        current_t += SECONDS_IN_DAY;
    }

    // --- Cleanup ---
    fclose(outfile);
    curl_global_cleanup();
    printf("\n\nSimulation complete. File '%s' has been created.\n", output_filename);
    return 0;
}

// --- Function Implementations ---

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

int fetch_orbital_elements(struct Planet *planet, const char* epoch_str, int debug_mode) {
    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) return -1;

    struct tm epoch_tm = {0};
    sscanf(epoch_str, "%d-%d-%d", &epoch_tm.tm_year, &epoch_tm.tm_mon, &epoch_tm.tm_mday);
    epoch_tm.tm_year -= 1900;
    epoch_tm.tm_mon -= 1;
    time_t epoch_t = mktime(&epoch_tm);
    epoch_t += SECONDS_IN_DAY;
    struct tm *next_day_tm = localtime(&epoch_t);
    char next_day_str[11];
    strftime(next_day_str, sizeof(next_day_str), "%Y-%m-%d", next_day_tm);

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    char url[512];
    snprintf(url, sizeof(url),
             "https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='%s'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='ELEMENTS'&CENTER='@sun'&START_TIME='%s'&STOP_TIME='%s'",
             planet->id, epoch_str, next_day_str);

    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    int success = -1;
    if (curl_easy_perform(curl_handle) == CURLE_OK) {
        if (debug_mode) {
            printf("\n--- RAW API RESPONSE for %s ---\n", planet->name);
            printf("%s\n", chunk.memory);
            printf("-------------------------------------\n");
        }

        char* elements_start = strstr(chunk.memory, "$$SOE");
        if (elements_start) {
            char *ptr;
            int parsed_count = 0;
            double semi_major_axis_km = 0;

            if ((ptr = strstr(elements_start, "EC="))) parsed_count += sscanf(ptr, "EC= %lf", &planet->eccentricity);
            if ((ptr = strstr(elements_start, "IN="))) parsed_count += sscanf(ptr, "IN= %lf", &planet->inclination_deg);
            if ((ptr = strstr(elements_start, "OM="))) parsed_count += sscanf(ptr, "OM= %lf", &planet->lon_asc_node_deg);
            if ((ptr = strstr(elements_start, "W ="))) parsed_count += sscanf(ptr, "W = %lf", &planet->arg_periapsis_deg);
            if ((ptr = strstr(elements_start, "MA="))) parsed_count += sscanf(ptr, "MA= %lf", &planet->mean_anomaly_deg);
            if ((ptr = strstr(elements_start, "A ="))) parsed_count += sscanf(ptr, "A = %lf", &semi_major_axis_km);
            
            planet->semi_major_axis_au = semi_major_axis_km / AU_TO_KM;

            if (parsed_count == 6) {
                planet->epoch = mktime(&epoch_tm);
                success = 0;
            } else {
                 fprintf(stderr, "  - Error: Could only parse %d of 6 orbital elements for %s.\n", parsed_count, planet->name);
            }
        } else {
            fprintf(stderr, "  - Error: Could not find $$SOE marker for %s.\n", planet->name);
        }
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    return success;
}

void calculate_position(struct Planet *planet, time_t current_date) {
    if (planet->semi_major_axis_au <= 0) {
        planet->x = planet->y = planet->z = NAN;
        return;
    }

    double days_since_epoch = difftime(current_date, planet->epoch) / SECONDS_IN_DAY;
    double mean_motion = 360.0 / (sqrt(pow(planet->semi_major_axis_au, 3)) * 365.25);
    double mean_anomaly = fmod(planet->mean_anomaly_deg + mean_motion * days_since_epoch, 360.0);
    double M_rad = mean_anomaly * M_PI / 180.0;

    double E_rad = M_rad;
    for (int i = 0; i < 10; i++) {
        E_rad = E_rad - (E_rad - planet->eccentricity * sin(E_rad) - M_rad) / (1 - planet->eccentricity * cos(E_rad));
    }

    double x_orb = planet->semi_major_axis_au * (cos(E_rad) - planet->eccentricity);
    double y_orb = planet->semi_major_axis_au * sqrt(1 - planet->eccentricity * planet->eccentricity) * sin(E_rad);

    double w_rad = planet->arg_periapsis_deg * M_PI / 180.0;
    double N_rad = planet->lon_asc_node_deg * M_PI / 180.0;
    double i_rad = planet->inclination_deg * M_PI / 180.0;

    // --- Full 3D Rotation ---
    planet->x = x_orb * (cos(w_rad) * cos(N_rad) - sin(w_rad) * sin(N_rad) * cos(i_rad)) -
                y_orb * (sin(w_rad) * cos(N_rad) + cos(w_rad) * sin(N_rad) * cos(i_rad));
    planet->y = x_orb * (cos(w_rad) * sin(N_rad) + sin(w_rad) * cos(N_rad) * cos(i_rad)) +
                y_orb * (-sin(w_rad) * sin(N_rad) + cos(w_rad) * cos(N_rad) * cos(i_rad));
    planet->z = x_orb * (sin(w_rad) * sin(i_rad)) + y_orb * (cos(w_rad) * sin(i_rad));
}
