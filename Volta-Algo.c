#include "stdio.h"
#include "string.h"
#include "dirent.h"
#include "time.h"

// external module
#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"

// scores an image with an arbitrary number. Higher is better. It uses a box blur algorithm as the main method and integral image technique for mapping (optimization).
int process_image(const char* file) {
    // setup parameters
    int isGRAYSCALE = 1;
    int minWidth = 1024, minHeight = 1024;
    int boxblurRad = 24; // radius of the box blur, 1 is a 3x3 box, 2 is a 5x5 box, and so on.
                         // v something I figured out, the boxblur radius acts like a precision modifier. Higher, and it takes more time. 

    // image pointers
    int width, height, channels;
    // i accidentally putted this AFTER the margin calculations. woops
    unsigned char* img = stbi_load(file, &width, &height, &channels, isGRAYSCALE);
    
    // how large the pinhole in the middle of the image. 0.5 is 50% and so on, so forth.
    float PinHoleSize = 0.5;
    // -- margin size base, creates a box vignette around the image, ignoring not inside the center.
    int x_margin = (width * (1.0 - PinHoleSize)) / 2;
    int y_margin = (height * (1.0 - PinHoleSize)) / 2;
    // --

    // the size of the box blur window, for averaging.
    int windowSize = (boxblurRad * 2 + 1) * (boxblurRad * 2 + 1);

    double areaRatio = (double)(width * height) / (double)(minWidth * minHeight);

    // sanity check
    if (img == NULL) { // fixed the bug where it won't process the score even if the image is 1024x1024.
        printf("[FAILURE] Image Load Fail.\n");
        return;
    }

    if ( width <= minWidth-1 || height <= minHeight-1) {
        printf("[FAILURE] Image too small. Minimum size is %d x %d.\n", minWidth, minHeight);
        stbi_image_free(img); // memory leak fix.
        return;
    }

    // setup base variables
    int totalPixels = width * height;

    // constant lookups for limits so the forloops won't have to recalculate each time.
    int width_middle = width / 2;
    int height_middle = height / 2;
    int y_padding = y_margin + boxblurRad;
    int x_padding = x_margin + boxblurRad;
    int y_limit = height - y_margin - boxblurRad;
    int x_limit = width - x_margin - boxblurRad;
    float brightnessLimit = 255.0;

    // the brightest nxn pixel in the image.
    double ImageScore = -67;
    double ContrastScore = -67;

    // start benchmark timer.
    time_t startTime = time(NULL);
    // get global image average.
    double globalBrightness = 0;
    for (int i = 0; i < totalPixels; i++) {
        globalBrightness += img[i];
    }
    double globalBrightnessAverage = globalBrightness / (double)totalPixels;

    unsigned int* imageMemory = calloc((width + 1) * (height + 1), sizeof(unsigned int)); // memory for the summed area table, to optimize the box blur calculation.

    // global brightness average for loop. Also builds our imageMemory because might aswell cause it already goes through each pixel in the image.
    // removing the need to go through the image again inside the main forloop.
    for (int y = 1; y <= height; y++) {
        int rowBrightness = 0;
        for (int x = 1; x <= width; x++) {
            int pixel = img[(y - 1) * width + (x - 1)];
            rowBrightness += pixel;
            globalBrightness += pixel;
            
            // uses integral image technique for the box blur's aura farm, by precomputing the sum of all pixels instead of doing it inside the main box blur loop. Each pixel.
            // -> https://www.slideshare.net/slideshow/integral-image-summed-area-table/72942067
            imageMemory[y * (width + 1) + x] = imageMemory[(y - 1) * (width + 1) + x] + rowBrightness;
        }
    }

    // main algorithm loop. Uses Box Blur and Integral Image mapping to find the brightness (mostly this) and contrast, then scores the image based on the contrast and distance from the center.
    for (int y = y_padding; y < y_limit; y++) {
        for (int x = x_padding; x < x_limit; x++) {

            // build bounding box coords around x,y
            int x1 = x - boxblurRad;
            int y1 = y - boxblurRad;
            int x2 = x + boxblurRad;
            int y2 = y + boxblurRad;
            
            // find the summed value of our boxblur inside the imageMemory that got mapped earlier. 
            int boxblurBrightness = imageMemory[(y2 + 1) * (width + 1) + (x2 + 1)] - 
                                    imageMemory[(y1) * (width + 1) + (x2 + 1)] - 
                                    imageMemory[(y2 + 1) * (width + 1) + (x1)] + 
                                    imageMemory[(y1) * (width + 1) + (x1)];
            
            // get the average brightness of the box blur.
            double boxblurAvg = (double)boxblurBrightness / (double)windowSize;

            // check the contrast dif between the boxblur and global average.
            double contrastAverageDiff = fabs(boxblurAvg - globalBrightnessAverage); // returns the absolute value, the f in abs is for floating point numbers.
            
            // score penalty for how far the nxn box from the center of the image.
            double distance = abs(x - width_middle) + abs(y - height_middle);
            double currentScore = (contrastAverageDiff * brightnessLimit) - (distance*areaRatio);

            if (currentScore > ImageScore) {
                ImageScore = currentScore;
                ContrastScore = contrastAverageDiff;
            }
        }
    }
    stbi_image_free(img); // free the image memory.

    // end benchmark timer.
    time_t endTime = time(NULL);
    double timeTaken = difftime(endTime, startTime);

    char new_filename[256]; // temp
    printf("[OUTPUT] Image Score: %.2f | Contrast: %.2f | Area Ratio: %.2f | Time: %.2f seconds\n", ImageScore, ContrastScore, areaRatio, timeTaken);

    // TODO: RENAME THE FILE INTO THE SCORE.
}

int main() {
    struct dirent *entry;
    const char* folder = "../images";
    DIR *directory = opendir(folder);

    if (directory == NULL) {
        printf("[FAILURE] Directory is null.\n"); return 1;
    }

    while (entry = readdir(directory)) {
        if ( strstr(entry->d_name, ".png") || strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg") ) {
            char filepath[512];
            snprintf(&filepath, sizeof(filepath), "%s/%s", folder, entry->d_name); // build the path to the image.

            printf("[ALERT] Processing Image: %s\n", filepath); // print the file being processed.
            process_image(filepath);
        }
    }
    closedir(directory);
    return 0;
}