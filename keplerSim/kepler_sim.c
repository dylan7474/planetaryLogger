/**
 * @file kepler_sim.c
 * @brief A high-speed Keplerian orbit simulator that generates historical
 * planetary longitude data and saves it to a CSV file.
 *
 * This program fetches the orbital elements for each planet from NASA for a
 * single "epoch" date. It then uses Kepler's equations to calculate the daily
 * positions of the planets over a user-specified date range, providing a
 * very fast alternative to making an API call for every single day.
 *
 * Compilation:
 * gcc kepler_sim.c -o kepler_sim -lcurl -ljansson -lm
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
    double eccentricity; // e
    double semi_major_axis_au; // a
    double inclination_deg; // i
    double lon_asc_node_deg; // LAN
    double arg_periapsis_deg; // w
    double mean_anomaly_deg; // M at epoch
    time_t epoch; // The time at which these elements are valid
};

// --- Function Prototypes ---
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
int fetch_orbital_elements(struct Planet *planet, const char* epoch_str);
double calculate_longitude(struct Planet *planet, time_t current_date);

// --- Main ---
int main(void) {
    struct Planet planets[] = {
        {"Mercury", "199"}, {"Venus", "299"}, {"Earth", "399"},
        {"Mars", "499"}, {"Jupiter", "599"}, {"Saturn", "699"},
        {"Uranus", "799"}, {"Neptune", "899"}, {"Pluto", "999"}
    };
    int num_planets = sizeof(planets) / sizeof(planets[0]);

    // --- Get User Input ---
    char start_date_input[11], end_date_input[11], epoch_date_input[11], output_filename[100];

    printf("--- High-Speed Keplerian Orbit Simulator ---\n");
    printf("Enter Epoch Date (YYYY-MM-DD) to get orbital elements (e.g., 2000-01-01): ");
    scanf("%10s", epoch_date_input);
    printf("Enter Start Date for simulation (YYYY-MM-DD): ");
    scanf("%10s", start_date_input);
    printf("Enter End Date for simulation (YYYY-MM-DD): ");
    scanf("%10s", end_date_input);
    printf("Enter Output Filename (e.g., kepler_data.csv): ");
    scanf("%99s", output_filename);

    // --- Fetch Orbital Elements for the Epoch ---
    printf("\nFetching orbital elements from NASA for epoch %s...\n", epoch_date_input);
    curl_global_init(CURL_GLOBAL_ALL);
    for (int i = 0; i < num_planets; i++) {
        if (fetch_orbital_elements(&planets[i], epoch_date_input) != 0) {
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
    for (int i = 0; i < num_planets; i++) fprintf(outfile, ",%s", planets[i].name);
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
            double longitude = calculate_longitude(&planets[i], current_t);
            fprintf(outfile, ",%.4f", longitude);
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

// Fetches orbital elements from NASA for a given epoch
int fetch_orbital_elements(struct Planet *planet, const char* epoch_str) {
    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) return -1;

    // Calculate the day after the epoch for a valid API date range
    struct tm epoch_tm = {0};
    sscanf(epoch_str, "%d-%d-%d", &epoch_tm.tm_year, &epoch_tm.tm_mon, &epoch_tm.tm_mday);
    epoch_tm.tm_year -= 1900;
    epoch_tm.tm_mon -= 1;
    time_t epoch_t = mktime(&epoch_tm);
    epoch_t += SECONDS_IN_DAY; // Add one day
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
        // --- DIAGNOSTIC ---
        printf("\n--- RAW API RESPONSE for %s ---\n", planet->name);
        printf("%s\n", chunk.memory);
        printf("-------------------------------------\n");
        // --- END DIAGNOSTIC ---

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
            
            // Convert Semi-major axis from km to AU
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

// Calculates the geocentric longitude of a planet for a given date
double calculate_longitude(struct Planet *planet, time_t current_date) {
    if (planet->semi_major_axis_au <= 0) {
        return NAN;
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

    double x_ecl = x_orb * (cos(w_rad) * cos(N_rad) - sin(w_rad) * sin(N_rad) * cos(i_rad)) -
                   y_orb * (sin(w_rad) * cos(N_rad) + cos(w_rad) * sin(N_rad) * cos(i_rad));
    double y_ecl = x_orb * (cos(w_rad) * sin(N_rad) + sin(w_rad) * cos(N_rad) * cos(i_rad)) +
                   y_orb * (-sin(w_rad) * sin(N_rad) + cos(w_rad) * cos(N_rad) * cos(i_rad));

    double longitude_rad = atan2(y_ecl, x_ecl);
    double longitude_deg = longitude_rad * (180.0 / M_PI);
    if (longitude_deg < 0) longitude_deg += 360;

    return longitude_deg;
}
