#ifndef GLOBAL_H_
#define GLOBAL_H_

#ifndef __ANDROID__
#define __ANDROID__
#endif
#ifdef __ANDROID__
////// DEBUG | RELEASE

// Debug
//#ifndef DEBUG
//#define DEBUG
//#endif
//#undef NDEBUG

// Relase
#ifndef NDEBUG
#define NDEBUG
#endif
#undef DEBUG

#endif // __ANDROID__


// Application version type
//#define PAID_VERSION // No advertising + No logo into video
#define DEMO_VERSION // No advertising


#define DISPLAY_DELAY               100
#define MAX_COLOR                   255.f
#define MAX_AD_HEIGHT               60 // In pixel

// Matrix color
#define MCAM_RED_COLOR              0x36
#define MCAM_GREEN_COLOR            0xdd
#define MCAM_BLUE_COLOR             0x47

// Application version
#define MCAM_VERSION                "1.00" // Always version with format "?.??"

#define DIGIT_COUNT(d)              static_cast<unsigned char>((d < 10)? 1:((d < 100)? 2:3)) // [0;999]

// Font
#define FONT_TEX_WIDTH              1024.f
#define FONT_TEX_HEIGHT             256.f
#define FONT_WIDTH                  35
#define FONT_HEIGHT                 46

// Camera
#define CAM_WIDTH                   640
#define CAM_HEIGHT                  480
#define CAM_TEX_WIDTH               1024.f
#define CAM_TEX_HEIGHT              512.f

// Log levels (< 5 to log)
#define LOG_LEVEL_BULLETTIME        4
#define LOG_LEVEL_MATRIXLEVEL       4
#define LOG_LEVEL_FRAME2D           4
#define LOG_LEVEL_COUNT2D           4
#define LOG_LEVEL_CONNEXION         4
#define LOG_LEVEL_SEARCHIP          4
#define LOG_LEVEL_PICTURE           4
#define LOG_LEVEL_VIDEO             4
#define LOG_LEVEL_SHARE             4

typedef struct {

    short left;
    short right;
    short top;
    short bottom;

} TouchArea;


#endif // GLOBAL_H_
