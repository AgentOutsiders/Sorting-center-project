#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/* We define the package target */
#define TARGET_PACKAGES 20

/* We define the named pipe from main to sorting */
#define FIFO_MAIN_TO_SORTING "main_to_sorting.fifo"

/* We define the named pipe from sorting to display */
#define FIFO_SORTING_TO_DISPLAY "sorting_to_display.fifo"


/* We declare the destination areas */
typedef enum
{
    /* Area A */
    AREA_A,

    /* Area B */
    AREA_B,

    /* Area C */
    AREA_C,

    /* Number of areas, not a real area */
    AREA_COUNT
}
AreaId;


/* We declare the statistics of an area */
typedef struct
{
    /* We store the area id */
    AreaId areaName;

    /* We count the packages received */
    int totalPackages;

    /* We add up the weight received */
    float totalWeight;
}
Area;


/* We declare a package */
typedef struct
{
    /* We store the id */
    uint32_t packageId;

    /* We store the destination area */
    AreaId packageDestination;

    /* We store the weight */
    float packageWeight;
}
Package;


/* We declare the global state of the center */
typedef struct
{
    /* We count the packages processed */
    int numberOfPackageProcessed;

    /* We add up the weight processed */
    float totalWeight;

    /* We keep the last package */
    Package lastPackageReceived;

    /* We store areas A, B, C */
    Area areas[AREA_COUNT];
}
CenterStatistics;

#endif