/**
 * @file planetary_logger.c
 * @brief A command-line tool to fetch historical planetary longitude data from
 * NASA's JPL Horizons system and save it to a CSV file.
 *
 * This program prompts the user for a start date and a number of days,
 * then iterates through that range, fetching the geocentric ecliptic longitude
 * for each major celestial body and writing the results to a CSV file.
 *
 * Compilation:
 * gcc planetary_logger.c -o planetary_logger -lcurl -ljansson -lm
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

// Struct to hold the response data from a curl request.
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Struct to hold planetary data.
struct Planet {
    const char *name;
    const char *id;
    double longitude;
};

// Callback function for libcurl
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

// Parses planetary data from NASA API response
int parse_planet_data(const char *json_text, double *longitude) {
    json_error_t error;
    json_t *root = json_loads(json_text, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", error.text);
        return -1;
    }
    json_t *result = json_object_get(root, "result");
    if (!json_is_string(result)) { json_decref(root); return -1; }
    const char *result_text = json_string_value(result);
    const char *data_start = strstr(result_text, "$$SOE");
    if (!data_start) { json_decref(root); return -1; }
    const char *x_ptr = strstr(data_start, "X =");
    const char *y_ptr = strstr(data_start, "Y =");
    if (!(x_ptr && y_ptr)) { json_decref(root); return -1; }
    double x_km, y_km;
    sscanf(x_ptr, "X =%lf", &x_km);
    sscanf(y_ptr, "Y =%lf", &y_km);
    
    double longitude_rad = atan2(y_km, x_km);
    *longitude = longitude_rad * (180.0 / M_PI);
    if (*longitude < 0) *longitude += 360;

    json_decref(root);
    return 0;
}

int main(void) {
    struct Planet planets[] = {
        {"Sun", "10"}, {"Moon", "301"}, {"Mercury", "199"}, {"Venus", "299"},
        {"Mars", "499"}, {"Jupiter", "599"}, {"Saturn", "699"}, {"Uranus", "799"},
        {"Neptune", "899"}, {"Pluto", "999"}
    };
    int num_planets = sizeof(planets) / sizeof(planets[0]);

    // --- Get User Input ---
    char start_date_input[11];
    int num_days_to_log;
    char output_filename[100];

    printf("--- NASA Planetary Data Logger ---\n");
    printf("This tool will generate a CSV file with daily planetary longitudes.\n");
    printf("Enter Start Date (YYYY-MM-DD): ");
    scanf("%10s", start_date_input);
    printf("Enter Number of Days to Log: ");
    scanf("%d", &num_days_to_log);
    printf("Enter Output Filename (e.g., data.csv): ");
    scanf("%99s", output_filename);

    // --- Open File for Writing ---
    FILE *outfile = fopen(output_filename, "w");
    if (outfile == NULL) {
        perror("Error opening output file");
        return 1;
    }

    // Write CSV Header
    fprintf(outfile, "Date");
    for (int i = 0; i < num_planets; i++) {
        fprintf(outfile, ",%s", planets[i].name);
    }
    fprintf(outfile, "\n");

    // --- Main Data Fetching Loop ---
    curl_global_init(CURL_GLOBAL_ALL);

    // Convert start date string to a time_t object
    struct tm start_tm = {0};
    sscanf(start_date_input, "%d-%d-%d", &start_tm.tm_year, &start_tm.tm_mon, &start_tm.tm_mday);
    start_tm.tm_year -= 1900;
    start_tm.tm_mon -= 1;
    time_t current_t = mktime(&start_tm);

    for (int day = 0; day < num_days_to_log; day++) {
        struct tm *current_tm = localtime(&current_t);
        char date_str[11], next_date_str[11];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", current_tm);
        
        time_t next_t = current_t + (24 * 60 * 60);
        struct tm *next_tm = localtime(&next_t);
        strftime(next_date_str, sizeof(next_date_str), "%Y-%m-%d", next_tm);

        printf("Processing: %s\n", date_str);
        fprintf(outfile, "%s", date_str);

        for (int i = 0; i < num_planets; i++) {
            CURL *curl_handle = curl_easy_init();
            if (curl_handle) {
                struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
                char url[512];
                snprintf(url, sizeof(url),
                         "https://ssd.jpl.nasa.gov/api/horizons.api?format=json&COMMAND='%s'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='VECTORS'&CENTER='@399'&START_TIME='%s'&STOP_TIME='%s'&STEP_SIZE='1d'&VEC_TABLE='1'",
                         planets[i].id, date_str, next_date_str);

                curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(curl_handle, CURLOPT_URL, url);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
                if (curl_easy_perform(curl_handle) == CURLE_OK) {
                    parse_planet_data(chunk.memory, &planets[i].longitude);
                }
                curl_easy_cleanup(curl_handle);
                free(chunk.memory);
            }
            fprintf(outfile, ",%.4f", planets[i].longitude);
        }
        fprintf(outfile, "\n");
        
        // Move to the next day
        current_t = next_t;
    }

    // --- Cleanup ---
    fclose(outfile);
    curl_global_cleanup();
    printf("\nData logging complete. File '%s' has been created.\n", output_filename);

    return 0;
}
