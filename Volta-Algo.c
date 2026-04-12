#include "stdio.h"
#include "string.h"
#include "dirent.h"

// external module
#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"

int process_image(const char* file) {
    // setup parameters
    int isGRAYSCALE = 1;
    int minWidth = 1024, minHeight = 1024;
    int boxblurRad = 12; // radius of the box blur, 1 is a 3x3 box, 2 is a 5x5 box, and so on.
                         // v something I figured out, the boxblur radius acts like a precision modifier. Higher, and it takes more time. 

    // image pointers
    int width, height, channels;
    // i accidentally putted this AFTER the margin calculations. woops
    unsigned char* img = stbi_load(file, &width, &height, &channels, isGRAYSCALE);
    
    // how large the pinhole in the middle of the image. 0.5 is 50% and so on, so forth.
    float PinHoleSize = 0.5;
    // -- margin size base, creates a box vignette around the image, ignoring not inside the center.
    int x_margin = (width * (1 - PinHoleSize)) / 2;
    int y_margin = (height * (1 - PinHoleSize)) / 2;
    // --

    double areaRatio = (double)(width * height) / (double)(minWidth * minHeight);

    // sanity check
    if (img == NULL || width <= minWidth-1 || height <= minHeight-1) { // fixed the bug where it won't process the score even if the image is 1024x1024.
        printf("[FAILURE] Image Load Fail.\n");
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

    // the brightest nxn pixel in the image.
    double ImageScore = -67;
    double ContrastScore = -67;

    // get global image average.
    double globalBrightness = 0;
    for (int i = 0; i < totalPixels; i++) {
        globalBrightness += img[i];
    }
    double globalBrightnessAverage = globalBrightness / (double)totalPixels;

    for (int y = y_padding; y < y_limit; y++) {
        for (int x = x_padding; x < x_limit; x++) {

            int boxblurBrightness = 0;

            int brightPixels = 0;
            int darkPixels = 255;
                                                                            
            for (int dy = -boxblurRad; dy <= boxblurRad; dy++) {            // Creates the box blur matrix, then sums all of the pixels in this grid.     
                for (int dx = -boxblurRad; dx <= boxblurRad; dx++) {        // [-1, 0, 1 ] [ -1 ]       example of a 3x3 grid centered around (x, y)
                    int pixel = img[(y + dy) * width + (x + dx)];           //             [  0 ]       with dy = -1, 0, 1 and dx = -1, 0, 1 .
                    boxblurBrightness += pixel;                             //             [  1 ]
                                                                            // No need to worry about rgb channels because of the isGRAYSCALE flag.
                    if (pixel > brightPixels) { brightPixels = pixel; }     
                    if (pixel < darkPixels) { darkPixels = pixel; }
                }                                                           
            }
            
            // contrast score of the box blur, higher is better.
            double currentContrastScore = brightPixels - darkPixels;

            // the size of the box blur window, for averaging.
            int windowSize = (boxblurRad * 2 + 1) * (boxblurRad * 2 + 1); 
            double boxblurAvg = (double)boxblurBrightness / (double)windowSize;
            // check the contrast dif between the boxblur and global average.
            double contrastAverageDiff = fabs(boxblurAvg - globalBrightnessAverage);
            
            // score penalty for how far the nxn box from the center of the image.
            double distance = abs(x - width_middle) + abs(y - height_middle);
            double currentScore = (contrastAverageDiff * (double)currentContrastScore) - (distance*areaRatio);

            if (currentScore > ImageScore) {
                ImageScore = currentScore;
                ContrastScore = currentContrastScore;
            }
        }
    }
    stbi_image_free(img); // free the image memory.

    // BRO THE ALGORITHM IS NOW O(N^5)
    // TODO: OPTIMIZE; clean up too cus this looks ugly.

    char new_filename[256]; // temp
    printf("[OUTPUT] Image Score: %.2f | Contrast: %.2f | Area Ratio: %.2f\n", ImageScore, ContrastScore, areaRatio);

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