#include "Main.h"

////// Languages
typedef enum {

    LANG_EN = 0,
    LANG_FR,
    LANG_DE,
    LANG_ES,
    LANG_IT,
    LANG_PT

} Language;
static const Language g_MainLang = LANG_EN;

////// Textures
#define NO_TEXTURE_LOADED       0xFF

BOOL engGetFontGrayscale() { return NO; }

#define TEXTURE_ID_APP          2 // EngActivity.TEXTURE_ID_FONT + 1
#define TEXTURE_ID_PANEL        3

unsigned char engLoadTexture(EngResources* resources, unsigned char Id) {
    switch (Id) {

        case TEXTURE_ID_APP: {
            
            unsigned char* data = [resources getBufferPNG:@"app" inGrayScale:NO];
            if (data == NULL) {
                NSLog(@"ERROR: Failed to get PNG buffer (line:%d)", __LINE__);
                break;
            }
            return platformLoadTexture(TEXTURE_ID_APP, static_cast<short>(resources.pngWidth),
                                       static_cast<short>(resources.pngHeight), data);
        }
        case TEXTURE_ID_PANEL: {

            unsigned char* data = [resources getBufferPNG:@"panel" inGrayScale:YES];
            if (data == NULL) {
                NSLog(@"ERROR: Failed to get PNG buffer (line:%d)", __LINE__);
                break;
            }
            return platformLoadTexture(TEXTURE_ID_PANEL, static_cast<short>(resources.pngWidth),
                                       static_cast<short>(resources.pngHeight), data, true);
        }
        default: {

            NSLog(@"ERROR: Failed to load PNG ID %d", Id);
            break;
        }
    }
    return NO_TEXTURE_LOADED;
}

////// Sounds
void engLoadSound(EngResources* resources, unsigned char Id) {
    switch (Id) {

        default: {

            NSLog(@"ERROR: Failed to load OGG ID %d", Id);
            break;
        }
    }
}

////// Advertising
#ifdef LIBENG_ENABLE_ADVERTISING
#include <libeng/Advertising/Advertising.h>

static NSString* ADV_UNIT_ID = @"ca-app-pub-1474300545363558/6019684627";
#ifdef DEBUG
static const NSString* IPAD_DEVICE_UID = @"655799b1c803de3417cbb36833b6c40c";
static const NSString* IPHONE_YACIN_UID = @"10053bb6983c6568b88812fbcfd7ab89";
#endif

BOOL engGetAdType() { return FALSE; } // TRUE: Interstitial; FALSE: Banner
void engLoadAd(EngAdvertising* ad, GADRequest* request) {

    static bool init = false;
    if (!init) {
        if ([[UIScreen mainScreen] bounds].size.width > 468)
            [ad getBanner].adSize = kGADAdSizeFullBanner;
        else
            [ad getBanner].adSize = kGADAdSizeBanner;
        [ad getBanner].adUnitID = ADV_UNIT_ID;
        init = true;
    }
#ifdef DEBUG
    request.testDevices = [NSArray arrayWithObjects: @"Simulator", IPAD_DEVICE_UID, IPHONE_YACIN_UID, nil];
#endif
    [[ad getBanner] loadRequest:request];
}
void engDisplayAd(EngAdvertising* ad, unsigned char Id) {

    CGFloat xPos = ([[UIScreen mainScreen] bounds].size.width - [ad getBanner].frame.size.width) / 2.0;
    [[ad getBanner] setHidden:NO];
    [ad getBanner].frame =  CGRectMake(xPos, -[ad getBanner].frame.size.height,
                                       [ad getBanner].frame.size.width, [ad getBanner].frame.size.height);
    [UIView animateWithDuration:1.5 animations:^{
        [ad getBanner].frame =  CGRectMake(xPos, 0, [ad getBanner].frame.size.width,
                                           [ad getBanner].frame.size.height);
    } completion:^(BOOL finished) {
        if (finished)
            ad.status = static_cast<unsigned char>(Advertising::STATUS_DISPLAYED);
    }];
}
void engHideAd(EngAdvertising* ad, unsigned char Id) { [[ad getBanner] setHidden:YES]; }
#endif

////// Social
#ifdef LIBENG_ENABLE_SOCIAL
BOOL engReqInfoField(SocialField field, unsigned char socialID) {

    switch (socialID) {
        case Network::FACEBOOK: {

            switch (field) {
                //case FIELD_NAME: return YES;
                //case FIELD_GENDER: return YES;
                case FIELD_BIRTHDAY: return YES;
                case FIELD_LOCATION: return YES;
                default: return YES; // FIELD_NAME & FIELD_GENDER are always retrieved
            }
            break;
        }
        case Network::GOOGLE:
            return YES; // All fields are always retrieved (if any)

        default: {

            NSLog(@"ERROR: Wrong social identifier %d", socialID);
            break;
        }
    }
    return NO;
}
#endif
