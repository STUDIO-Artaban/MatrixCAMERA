#ifndef PANELCOORDS_H_
#define PANELCOORDS_H_

#define PANEL_TEX_WIDTH     512.f
#define PANEL_TEX_HEIGHT    512.f

#define PANEL_X0            1
#define PANEL_Y0            241
#define PANEL_X2            319
#define PANEL_Y2            383

#define SCREEN_X0           0
#define SCREEN_Y0           0
#define SCREEN_X2           320
#define SCREEN_Y2           240

#define WAIT_X0             321
#define WAIT_Y0             1
#define WAIT_X2             511
#define WAIT_Y2             191

#define WAIT_SIZE           (WAIT_X2 - WAIT_X0) // == (WAIT_Y2 - WAIT_Y0)

#define LAND_X0             1
#define LAND_Y0             385
#define LAND_X2             126
#define LAND_Y2             510

#define LAND_SIZE           (LAND_X2 - LAND_X0) // == (LAND_Y2 - LAND_Y0) == PORT_SIZE

#define PORT_X0             129
#define PORT_Y0             385
#define PORT_X2             254
#define PORT_Y2             510

#define SHARE_X0            257
#define SHARE_Y0            385
#define SHARE_X2            383
#define SHARE_Y2            511

#define SHARE_SIZE          WAIT_SIZE

#define PRESS_X0            321
#define PRESS_Y0            193
#define PRESS_X2            511
#define PRESS_Y2            383

#define PRESS_SIZE          WAIT_SIZE

#define PLAY_X0             384
#define PLAY_Y0             384
#define PLAY_X2             511
#define PLAY_Y2             511

#define PLAY_SIZE           WAIT_SIZE

// Font texture
#define BACK_X0             1
#define BACK_Y0             139
#define BACK_X2             154
#define BACK_Y2             255

#define FACEBOOK_X0         156
#define FACEBOOK_Y0         139
#define FACEBOOK_X2         272
#define FACEBOOK_Y2         255

#define GOOGLE_X0           274
#define GOOGLE_Y0           139
#define GOOGLE_X2           390
#define GOOGLE_Y2           255

#define TWITTER_X0          392
#define TWITTER_Y0          139
#define TWITTER_X2          508
#define TWITTER_Y2          255

#define YOUTUBE_X0          510
#define YOUTUBE_Y0          139
#define YOUTUBE_X2          626
#define YOUTUBE_Y2          255

#define LOGO_X0             628
#define LOGO_Y0             139
#define LOGO_X2             750
#define LOGO_Y2             174

#define LOGO_HEIGHT         (LOGO_Y2 - LOGO_Y0)
#define LOGO_WIDTH          (LOGO_X2 - LOGO_X0)


#endif // PANELCOORDS_H_
