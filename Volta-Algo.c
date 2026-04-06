#include "stdio.h"
#include "string.h"
#include "dirent.h"

// external module
#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"

int process_image(const char* file) {
    // setup parameters
    int isGRAYSCALE = 1;
    int minWidth = 256, minHeight = 256;
    int boxblurRad = 1; // radius of the box blur, 1 is a 3x3 box, 2 is a 5x5 box, and so on.

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

    // constant lookups for distance so the forloops don't have to divide every time.
    int width_middle = width / 2;
    int height_middle = height / 2;

    // sanity check
    if (img == NULL || width <= minWidth || height <= minHeight) {
        printf("[FAILURE] Image Load Fail.\n");
        return;
    }

    // setup base variables
    int totalPixels = width * height;

    // the brightest nxn pixel in the image
    int ImageScore = -67;

    for (int y = y_margin + boxblurRad; y < (height - y_margin - boxblurRad); y++) {
        for (int x = x_margin + boxblurRad; x < (width - x_margin - boxblurRad); x++) {

            int boxblurBrightness = 0;                                      // Creates the box blur matrix, then sums all of the pixels in this grid.
            for (int dy = -boxblurRad; dy <= boxblurRad; dy++) {            // [-1, 0, 1 ] [ -1 ]       example of a 3x3 grid centered around (x, y) 
                for (int dx = -boxblurRad; dx <= boxblurRad; dx++) {        //             [  0 ]       with dy = -1, 0, 1 and dx = -1, 0, 1 .
                    boxblurBrightness += img[(y + dy) * width + (x + dx)];  //             [  1 ]
                }                                                           // No need to worry about rgb channels because of the isGRAYSCALE flag. 
            }

            // score penalty for how far the nxn box from the center of the image.
            int distance = abs(x - width_middle) + abs(y - height_middle);
            int currentScore = boxblurBrightness - distance;

            if (currentScore > ImageScore) {
                ImageScore = currentScore;
            }
        }
    }
    stbi_image_free(img); // free the image memory.

    char new_filename[256]; // temp
    printf("[OUTPUT] Image Score: %d\n", ImageScore);

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