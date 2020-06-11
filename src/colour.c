#include "colour.h"

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>


#define OUTPUT_TERMINAL_CHARSET_ " .:-=+*#%@"


const enum ColourSchemeType COLOUR_SCHEME_TYPE_MIN = COLOUR_SCHEME_TYPE_ALL;
const enum ColourSchemeType COLOUR_SCHEME_TYPE_MAX = COLOUR_SCHEME_TYPE_MATRIX;


static const char *OUTPUT_TERMINAL_CHARSET = OUTPUT_TERMINAL_CHARSET_;
static const size_t OUTPUT_TERMINAL_CHARSET_LENGTH = (sizeof(OUTPUT_TERMINAL_CHARSET_) - 1) / sizeof(char);
static const int COLOUR_SCALE_MULTIPLIER = 20;
static const double CHAR_SCALE_MULTIPLIER = 0.3;

static double smoothFactor;


static int mapColourSchemeAll(struct ColourRGB *rgb, unsigned int n, enum EscapeStatus status);
static int mapColourSchemeBlackWhite(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeWhiteBlack(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeGreyscale(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeRedWhite(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeFire(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeAllVibrant(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeRedHot(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeMatrix(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static int mapColourSchemeASCII(struct ColourRGB *rgb, double n, enum EscapeStatus status);
static void hsvToRGB(struct ColourRGB *rgb, struct ColourHSV *hsv);
static double smooth(unsigned int iterations);


void initialiseColourScheme(struct ColourScheme *scheme, enum ColourSchemeType colour)
{
    const struct ColourScheme COLOUR_SCHEME_DEFAULT =
    {
        COLOUR_SCHEME_TYPE_ALL,
        BIT_DEPTH_24,
        mapColourSchemeAll
    };

    scheme->colour = colour;

    switch (colour)
    {
        case COLOUR_SCHEME_TYPE_DEFAULT:
            scheme->colour = COLOUR_SCHEME_DEFAULT.colour;
            scheme->depth = COLOUR_SCHEME_DEFAULT.depth;
            scheme->mapColour = COLOUR_SCHEME_DEFAULT.mapColour;
        case COLOUR_SCHEME_TYPE_ASCII:
            scheme->depth = BIT_DEPTH_ASCII;
            scheme->mapColour = mapColourSchemeASCII;
            break;
        case COLOUR_SCHEME_TYPE_ALL:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeAll;
            break;
        case COLOUR_SCHEME_TYPE_BLACK_WHITE:
            scheme->depth = BIT_DEPTH_1;
            scheme->mapColour = mapColourSchemeBlackWhite;
            break;
        case COLOUR_SCHEME_TYPE_WHITE_BLACK:
            scheme->depth = BIT_DEPTH_1;
            scheme->mapColour = mapColourSchemeWhiteBlack;
            break;
        case COLOUR_SCHEME_TYPE_GREYSCALE:
            scheme->depth = BIT_DEPTH_8;
            scheme->mapColour = mapColourSchemeGreyscale;
            break;
        case COLOUR_SCHEME_TYPE_RED_WHITE:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeRedWhite;
            break;
        case COLOUR_SCHEME_TYPE_FIRE:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeFire;
            break;
        case COLOUR_SCHEME_TYPE_ALL_VIBRANT:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeAllVibrant;
            break;
        case COLOUR_SCHEME_TYPE_RED_HOT:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeRedHot;
            break;
        case COLOUR_SCHEME_TYPE_MATRIX:
            scheme->depth = BIT_DEPTH_24;
            scheme->mapColour = mapColourSchemeMatrix;
            break;
        default:
            scheme->colour = COLOUR_SCHEME_DEFAULT.colour;
            scheme->depth = COLOUR_SCHEME_DEFAULT.depth;
            scheme->mapColour = COLOUR_SCHEME_DEFAULT.mapColour;
            break;
    }

    return;
}


/* Set iteration count smoothing factor */
void setSmoothFactor(double escapeRadius)
{
    smoothFactor = log(log(escapeRadius)) / log(escapeRadius);
    return;
}


/* Maps an iteration count to an RGB value */
int mapColour(struct ColourRGB *rgb, struct ColourScheme *scheme, unsigned int iterations, enum EscapeStatus status)
{
    double n;

    if (status == ESCAPED)
    {
        n = smooth(iterations);
    }

    return (scheme->mapColour(rgb, n, status));
}


int mapColourSchemeAll(struct ColourRGB *rgb, unsigned int n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .s = 0.6,
        .v = 0.0
    };

    if (status == ESCAPED)
    {
        hsv.h = fmod(COLOUR_SCALE_MULTIPLIER * n, 360.0);
        hsv.v = 0.8;
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


int mapColourSchemeBlackWhite(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    if (status == UNESCAPED)
    {
        /* Set n'th bit of the byte */
        rgb->r |= 1 << (7 - (unsigned int) n);
    }
    else if (status == ESCAPED)
    {
        /* Unset n'th bit of the byte */
        rgb->r &= ~(1 << (7 - (unsigned int) n));
    }

    return 0;
}


int mapColourSchemeWhiteBlack(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    if (status == UNESCAPED)
    {
        /* Unset n'th bit of the byte */
        rgb->r &= ~(1 << (7 - (unsigned int) n));
    }
    else if (status == ESCAPED)
    {
        /* Set n'th bit of the byte */
        rgb->r |= 1 << (7 - (unsigned int) n);
    }

    return 0;
}


int mapColourSchemeGreyscale(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    rgb->r = 0;

    if (status == UNESCAPED)
    {
        /* Gets values between 0 and 255 */
        rgb->r = (uint8_t) (255 - fabs(fmod(n * 8.5, 510) - 255));
        
        if (rgb->r < 30)
        {
            /* Prevents shade getting too dark */
            rgb->r = 30;
        }
    }

    return 0;
}


int mapColourSchemeRedWhite(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .h = 0.0,
        .s = 1.0,
        .v = 1.0
    };

    if (status == ESCAPED)
    {
        hsv.s = 0.7 - fabs(fmod(n / 20.0, 1.4) - 0.7);

        if (hsv.s > 0.7)
        {
            hsv.s = 0.7;
        }
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


int mapColourSchemeFire(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .s = 0.85,
        .v = 0.0
    };

    if (status == ESCAPED)
    {
        /* Varies hue between 0 and 50 - red to yellow */
        hsv.h = 50.0 - fabs(fmod(n * 2.0, 100.0) - 50.0);
        hsv.v = 0.85;
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


int mapColourSchemeAllVibrant(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .s = 1.0,
        .v = 0.0
    };

    if (status == ESCAPED)
    {
        hsv.h = fmod(COLOUR_SCALE_MULTIPLIER * n, 360.0);
        hsv.v = 1.0;
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


int mapColourSchemeRedHot(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .h = 0.0,
        .s = 1.0,
        .v = 0.0
    };

    if (status == ESCAPED)
    {
        /* Gets values between 0 and 90 */
        n = 90.0 - fabs(fmod(n * 2.0, 180.0) - 90.0);

        if (n <= 30.0)
        {
            /* Varies brightness of red */
            hsv.v = n / 30.0;
        }
        else
        {
            /* Varies hue between 0 and 60 - red to yellow */
            hsv.h = n - 30.0;
            hsv.v = 1.0;
        }
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


int mapColourSchemeMatrix(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    struct ColourHSV hsv =
    {
        .h = 120.0,
        .s = 1.0,
        .v = 0.0
    };

    if (status == ESCAPED)
    {
        hsv.v = (90.0 - fabs(fmod(n * 2.0, 180.0) - 90.0)) / 90.0;
    }

    hsvToRGB(rgb, &hsv);

    return 0;
}


/* Maps a given iteration count to an index of outputChars[] */
int mapASCII(struct ColourRGB *rgb, double n, enum EscapeStatus status)
{
    size_t i = OUTPUT_TERMINAL_CHARSET_LENGTH - 1;

    if (status == ESCAPED)
    {
        i = (int) fmod((CHAR_SCALE_MULTIPLIER * n), i);
    }

    return OUTPUT_TERMINAL_CHARSET[i];
}




static void hsvToRGB(struct ColourRGB *rgb, struct ColourHSV *hsv)
{
    unsigned char i;    
    double p, q, r;

    if (hsv->h < 0.0)
    {
        hsv->h = 0.0;
    }

    if (hsv->s < 0.0)
    {
        hsv->s = 0.0;
    }

    if (hsv->v < 0.0)
    {
        hsv->v = 0.0;
    }
    
    if (hsv->v == 0.0)
    {
        rgb->r = rgb->g = rgb->b = 0.0;
        return;
    }
    
    i = (unsigned char) floor(hsv->h / 60.0);

    hsv->h = (hsv->h / 60.0) - i;

    p = hsv->v * (1.0 - hsv->s) * 255.0;
    q = hsv->v * (1.0 - hsv->s * hsv->h) * 255.0;
    r = hsv->v * (1.0 - hsv->s * (1.0 - hsv->h)) * 255.0;

    switch (i)
    {
        case 0:
            rgb->r = hsv->v;
            rgb->g = r;
            rgb->b = p;
            break;
        case 1:
            rgb->r = q;
            rgb->g = hsv->v;
            rgb->b = p;
            break;
        case 2:
            rgb->r = p;
            rgb->g = hsv->v;
            rgb->b = r;
            break;
        case 3:
            rgb->r = p;
            rgb->g = q;
            rgb->b = hsv->v;
            break;
        case 4:
            rgb->r = r;
            rgb->g = p;
            rgb->b = hsv->v;
            break;
        case 5:
            rgb->r = hsv->v;
            rgb->g = p;
            rgb->b = q;
            break;
        default:
            rgb->r = hsv->v;
            rgb->g = r;
            rgb->b = p;
            break;
    }

    return;
}


/* Makes discrete iteration count a continuous value */
static double smooth(unsigned int iterations)
{
    return (iterations + 1.0 - smoothFactor);
}