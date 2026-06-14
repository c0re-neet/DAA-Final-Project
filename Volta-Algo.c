#include "stdio.h"
#include "string.h"
#include "dirent.h"
#include "time.h"

// external module
#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"

// scores an image with an arbitrary number. Higher is better. It uses a box blur algorithm as the main method and integral image technique for mapping (optimization).
void process_image(const char* file) {
    // setup parameters
    int isGRAYSCALE = 1;
    int minWidth = 1024, minHeight = 1024;
    int boxblurRad = 24; // radius of the box blur, 1 is a 3x3 box, 2 is a 5x5 box, and so on.

    // image pointers
    int width, height, channels;
    unsigned char* img = stbi_load(file, &width, &height, &channels, isGRAYSCALE);

    // sanity check
    if (img == NULL) { 
        printf("[FAILURE] Image Load Fail: %s\n", file);
        return;
    }

    if (width <= minWidth - 1 || height <= minHeight - 1) {
        printf("[FAILURE] Image too small. Minimum size is %d x %d.\n", minWidth, minHeight);
        stbi_image_free(img); // memory leak fix for early exit.
        return;
    }

    // how large the pinhole in the middle of the image. 0.5 is 50% and so on, so forth.
    float PinHoleSize = 0.5f;
    // margin size base, creates a box vignette around the image
    int x_margin = (int)(width * (1.0f - PinHoleSize)) / 2;
    int y_margin = (int)(height * (1.0f - PinHoleSize)) / 2;

    // the size of the box blur window, for averaging.
    int windowSize = (boxblurRad * 2 + 1) * (boxblurRad * 2 + 1);

    double areaRatio = ((double)width * (double)height) / ((double)minWidth * (double)minHeight);

    // setup base variables
    int totalPixels = width * height;

    // constant lookups for limits so the forloops won't have to recalculate each time.
    int width_middle = width / 2;
    int height_middle = height / 2;
    int y_padding = y_margin + boxblurRad;
    int x_padding = x_margin + boxblurRad;
    int y_limit = height - y_margin - boxblurRad;
    int x_limit = width - x_margin - boxblurRad;
    float brightnessLimit = 255.0f;

    double ImageScore = -67.0;
    double ContrastScore = -67.0;

    // start benchmark timer.
    time_t startTime = time(NULL);
    double globalBrightness = 0.0;
    
    // allocate memory for the summed area table
    unsigned int* imageMemory = calloc((width + 1) * (height + 1), sizeof(unsigned int));
    
    // safety check for RAM availability
    if (imageMemory == NULL) {
        printf("[FAILURE] Memory allocation failed for integral image.\n");
        stbi_image_free(img);
        return;
    }

    // Builds our imageMemory AND calculates global brightness simultaneously
    for (int y = 1; y <= height; y++) {
        int rowBrightness = 0;
        for (int x = 1; x <= width; x++) {
            int pixel = img[(y - 1) * width + (x - 1)];
            rowBrightness += pixel;
            globalBrightness += pixel;
            
            // uses integral image technique for the box blur's aura farm
            imageMemory[y * (width + 1) + x] = imageMemory[(y - 1) * (width + 1) + x] + rowBrightness;
        }
    }

    // Calculate average AFTER the loop completes
    double globalBrightnessAverage = globalBrightness / (double)totalPixels;

    // main algorithm loop. 
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
            double currentScore = (contrastAverageDiff * brightnessLimit) - (distance * areaRatio);

            if (currentScore > ImageScore) {
                ImageScore = currentScore;
                ContrastScore = contrastAverageDiff;
            }
        }
    }

    // free the image AND the integral image memory.
    stbi_image_free(img); 
    free(imageMemory);

    // end benchmark timer.
    time_t endTime = time(NULL);
    double timeTaken = difftime(endTime, startTime);

    printf("[OUTPUT] Image Score: %.2f | Contrast: %.2f | Area Ratio: %.2f | Time: %.2f seconds\n", ImageScore, ContrastScore, areaRatio, timeTaken);
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